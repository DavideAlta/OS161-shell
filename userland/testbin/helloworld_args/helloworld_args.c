/*
 * helloworld_args - test a simple printf()
 */

#include <stdio.h>
#include <unistd.h>
#include <err.h>

int main(int argc, char **argv){

    printf("Hello World !!!!!!!!!!!\n");
    printf("My arguments are:\n");
    for(int i=0;i<argc;i++){
        printf("- %s\n",argv[i]);
    }
    printf("No more arguments.\n");
	
    return 0;
}