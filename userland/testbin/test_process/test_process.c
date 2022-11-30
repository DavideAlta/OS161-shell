/*
 * test_process.c - the main process create a child by forking
 *                  and wait for its termination by waitpid.
 * 
 * (test of fork, waitpid, getpid, _exit)
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

int main(void){
	
    pid_t pid, pidc, pidp;
    int status;

    printf("test_process is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
        printf("Error: %s\n",strerror(errno));
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        printf("This message is printed by child process (pid = %d).\n",pidc);
        _exit(1024);
    
    }else{ // Parent
        pidp = getpid();
        waitpid(pid, &status, 0); // options argument is unused in os161
        printf("This message is printed by parent process (pid = %d) that is waiting for its child (pid = %d).\n",pidp,pid);
        printf("The child process is exited with exit status %d\n", status);
        return 0;
    }
}