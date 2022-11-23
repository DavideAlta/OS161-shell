/*
 * sysexecv
 */

#include <stdio.h>
#include <unistd.h>
#include <err.h>

int main(void){
	
    pid_t pid, pidc, pidp;
    int rv, status;
    char *args[4];
	args[0] = (char *)"foo";
    args[1] = (char *)"hello!";
    args[2] = (char *)"1";
	args[3] = NULL;

    printf("sysexecv test is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        // With args
        rv = execv("/testbin/helloworld_args",args);
        // Without args
        //rv = execv("/testbin/helloworld",NULL);
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