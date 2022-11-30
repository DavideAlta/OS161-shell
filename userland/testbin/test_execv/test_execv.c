/*
 * test_execv.c - After forking a new executable file
 *                is loaded and the child runs it.
 * 
 * It accepts the executable file's path by cmd line, showing
 * also runprogram working.
 * 
 * (test of fork, execv, waitpid, getpid, _exit)
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv){
	
    pid_t pid, pidc, pidp;
    int rv, status;
    char *execv_args[4];
	execv_args[0] = (char *)"foo";
    execv_args[1] = (char *)"hello!";
    execv_args[2] = (char *)"1";
	execv_args[3] = NULL;

    (void)argc;

    printf("test_execv is running...\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Forking has failed (pid = %d).\n",pid);
        printf("Error: %s\n",strerror(errno));
    
    }else if(pid == 0){ // Child
        pidc = getpid();
        rv = execv(argv[0],execv_args);

        if (rv == -1){
            printf("execv has failed.\n");
            printf("Error: %s\n",strerror(errno));
            return rv;
        }

        // On execv success the following code will not be executed
        printf("This message is printed by child process (pid = %d).\n",pidc);
        _exit(-8);

    }else{ // Parent
        pidp = getpid();
        waitpid(pid, &status, 0);
        printf("This message is printed by parent process (pid = %d) with status %d. The child pid is %d.\n",pidp,status,pid);
    }

	return 0;
}