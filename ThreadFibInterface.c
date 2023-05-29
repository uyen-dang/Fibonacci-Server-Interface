//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <signal.h>
//-----------------------------------------------------------------------------
#define NUM_LENGTH 1000000000
#define STRING_LENGTH 127
//-----------------------------------------------------------------------------
typedef char String[STRING_LENGTH];
//-----------------------------------------------------------------------------
int PPID;
bool endLoop = false;
//-----------------------------------------------------------------------------
void SIGUSR1handler(int sig) { //---- Signal handler for SIGUSR1

    printf("I: Received a SIGUSR1, stopping loop\n");
    printf("I: Reading from user abandoned\n");

    //---- Cause loop to end
    extern bool endLoop;
    endLoop = true;
}
//-----------------------------------------------------------------------------
int main(int argc, char *argv[]) {

    FILE * FIBORead;
    extern int PPID;
    PPID = getppid();
    String inputStr;
    int input;

    //---- Get handler for SIGUSR1
    if (signal(SIGUSR1,SIGUSR1handler) == SIG_ERR) {
        perror("I: Could not get SIGUSR1 handler");
        return(EXIT_FAILURE);
    }

    //---- Open named pipe for writing
    if ((FIBORead = fopen(argv[1],"w")) == NULL) {
        perror("I: Could not open named pipe");
        return(EXIT_FAILURE);
    }

    //---- Repeatedly 
    while (!endLoop) {

        //---- Prompt the user and read input from terminal
        //---- assuming positive integer input
        printf("I: Which Fibonacci number do you want: ");
        fgets(inputStr,NUM_LENGTH,stdin);
        input = atoi(inputStr);

        //---- Set input = 0 to be sent to server
        if (endLoop) {
            input = 0;
        }

        //--- Write up pipe if integer is greater than or equal to 0
        if (fprintf(FIBORead,"%d\n",input) == -1) {
                perror("I: Could not write to named pipe");
                return(EXIT_FAILURE);
            };
        fflush(FIBORead);

        //---- End loop if input is 0
        if (input == 0) { 
            endLoop = true;
            printf("I: Interface is exiting\n");
        }
    }

    //---- Close pipe for writing
    fclose(FIBORead);

    return(EXIT_SUCCESS);
}
//-----------------------------------------------------------------------------