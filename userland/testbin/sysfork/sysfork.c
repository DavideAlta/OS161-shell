/*
 * sysfork - test the fork() system call by printing
 * 2 different messages for child and parent processes.
 */

#include <stdio.h>
#include <unistd.h>
#include <err.h>

int main(void){
	
    pid_t pid;

    printf("sysfork test is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed.\n");
    
    }else if(pid == 0){ // Child
        printf("This message is printed by child process.\n");
    
    }else{ // Parent
        printf("This message is printed by parent process. The child pid is %d\n",pid);
    
    }

    (void)pid;

	return 0;
}

// TO DO: per la printf serve sys_write e l'apertura della console