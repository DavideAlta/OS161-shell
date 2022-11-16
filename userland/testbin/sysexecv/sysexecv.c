/*
 * sysexecv
 */

#include <stdio.h>
#include <unistd.h>
#include <err.h>

int main(void){
	
    pid_t pid, pidc, pidp;
    int rv, status;

    printf("sysexecv test is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        rv = execv("/testbin/helloworld",NULL); // try without args
        if (rv == -1){
            printf("execv failed.\n");
		    return -1;
        }
        printf("This message is printed by child process (pid = %d).\n",pidc);
        _exit(7);

    }else{ // Parent
        pidp = getpid();
        waitpid(pid, &status, 0);
        printf("This message is printed by parent process (pid = %d). The child pid is %d\n",pidp,pid);
    }

	return 0;
}