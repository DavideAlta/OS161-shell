/*
 * sysfork - test the fork() system call by printing
 * 2 different messages for child and parent processes.
 */

#include <stdio.h>
#include <unistd.h>
#include <err.h>

int main(void){
	
    pid_t pid, pidc, pidp;

    printf("sysfork test is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        printf("This message is printed by child process (pid = %d).\n",pidc);
    
    }else{ // Parent
        pidp = getpid();
        printf("This message is printed by parent process (pid = %d). The child pid is %d\n",pidp,pid);
    
    }

	return 0;
}