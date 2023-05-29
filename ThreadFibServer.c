//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/resource.h>
//-----------------------------------------------------------------------------
int ChildPID;
int threadNum = 0; //---- Set global counter of the number of running threads to 0
//-----------------------------------------------------------------------------
void SIGXCPUhandler(int sig) { //---- Signal handler for SIGXCPU

    //---- Receive anymore SIGXCPU signals
    printf("S: Received a SIGXCPU, ignoring anymore\n");
    if (signal(SIGXCPU,SIG_IGN) == SIG_ERR) {
        perror("Could not ignore SIGXCPU");
        exit(EXIT_FAILURE);
    }

    //---- Send SIGUSR1 to child process
    printf("S: Received a SIGXCPU, sending SIGGUSR1 to interface\n");
    if (kill(ChildPID,SIGUSR1) == -1) {
        perror("S: Could not send SIGUSR1 to child process");
        exit(EXIT_FAILURE);
    }    
}
//-----------------------------------------------------------------------------
long Fibonacci(long WhichNumber) { //----Function to compute the Nth Fibonacci number recursively

    if (WhichNumber <= 1) { //----If the 1st or second number, return the answer directly
        return(WhichNumber);
    } else { //----Otherwise return the sum of the previous two Fibonacci numbers
        return(Fibonacci(WhichNumber - 2) + Fibonacci(WhichNumber - 1));
    }
}
//-----------------------------------------------------------------------------
void * FibonacciThread(void * num) {

    //---- Calculate the Nth Fibonacci number using the recursive O(2^n) algorithm
    //---- and print out the result
    int WhichNumber = * ((int *) num);
    printf("S: Fibonacci %d is %ld\n",WhichNumber,Fibonacci(WhichNumber));

    //---- Decrement the number of running threads
    threadNum--;
    return NULL;
}
//-----------------------------------------------------------------------------
int main(int argc, char * argv[]) {

    FILE * FIBORead;
    extern int ChildPID;
    pthread_t fibCal;
    struct rlimit lim;
    struct rusage usage;
    int status;
    int num = 1;
    extern int threadNum;

    //---- Check if inputs are valid
    if (argc != 2) {
        perror("S: Invalid input");
        return(EXIT_FAILURE);
    }

    //---- Make pipe FIBOPIPE
    unlink("FIBOPIPE"); //---- Delete the existing pipe if any
    mkfifo("FIBOPIPE",0600); //---- Create new pipe with a specified name FIBOPIPE

    //---- Create a child process
    if ((ChildPID = fork()) == -1) {
        perror("Could not fork");
        exit(EXIT_FAILURE);
    }
    if (ChildPID == 0) { //----In the child
        //---- Run the user interface program with named pipe being passed
        //--- as the command line parameter
        execlp("./ThreadFibInterface","ThreadFibInterface","FIBOPIPE",NULL);
        perror("S: Could not execute ThreadFibInterface");
        exit(EXIT_FAILURE);
    }

    //---- Open pipe for reading
    if ((FIBORead = fopen("FIBOPIPE","r")) == NULL) {
        perror("S: Could not open FIFO to read");
        exit(EXIT_FAILURE);
    }

    //---- Get SIGXCPU handler
    if (signal(SIGXCPU,SIGXCPUhandler) == SIG_ERR) {
        perror("S: Could not get SIGXCPU handler");
        exit(EXIT_FAILURE);
    }

    //---- Limit CPU usage to num of secs, given as command line parameter
    if (getrlimit(RLIMIT_CPU,&lim) == -1){
        perror("S: Could not get CPU limit");
        exit(EXIT_FAILURE);
    };
    lim.rlim_cur = atoi(argv[1]);
    printf("S: Setting CPU limit to %llu\n",lim.rlim_cur);
    if (setrlimit(RLIMIT_CPU,&lim) == -1) {
        perror("S: Could not set CPU limit");
        exit(EXIT_FAILURE);
    }

    //---- Repeatedly
    while(fscanf(FIBORead," %d",&num) == 1 && num > 0){ //---- Read integet N off the pipe; if 0, exit
        printf("S: Received %d from interface\n",num);

        //---- Report the CPU time used
        long long time = (usage.ru_utime.tv_sec+usage.ru_stime.tv_sec)*1000000+(usage.ru_utime.tv_usec+usage.ru_stime.tv_usec);
        printf("S: Server has used %llds %lldus\n",time/1000000,time%1000000);
        
        //---- Start thread for calculating the Nth Fibonacci number
        if (pthread_create(&fibCal,NULL,FibonacciThread,&num) != 0) {
            perror("S: Could not create thread");
            exit(EXIT_FAILURE);
        } //---- Detach thread
        if (pthread_detach(fibCal) != 0) {
            perror("S: Could not detach thread");
            exit(EXIT_FAILURE);
        }
        printf("S: Created and detached the thread for %d\n",num);

        //---- Increment number of running threads
        threadNum++;
    }

    fclose(FIBORead); //---- Close pipe for reading

    while (threadNum > 0) { //---- Wait for all threads to finish
        printf("S: Waiting for %d threads\n",threadNum);
        sleep(1); 
    } 
    
    wait(&status); //---- Wait for child process to complete, clean up child process zombie
    printf("S: Child %d completed with status %d\n",ChildPID,status);

    //---- Delete named pipe
    if (unlink("FIBOPIPE") == -1) {
        perror("S: Could not delete named pipe");
        exit(EXIT_FAILURE);
    }

    return(EXIT_SUCCESS);
}
//-----------------------------------------------------------------------------