/*
 * syswait
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <err.h>

int main(void){
	
    pid_t pid, pidc, pidp;
    int status;

    printf("syswait test is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        printf("This message is printed by child process (pid = %d).\n",pidc);
        _exit(4);
    
    }else{ // Parent
        pidp = getpid();
        waitpid(pid, &status, 0);
        printf("This message is printed by parent process (pid = %d) that is waiting for its child (pid = %d).\n",pidp,pid);
        printf("The child process is exited with exit status %d\n", status);
        fork();
        printf("AAAAAAAAAAAA");
    }

	return 0;
}