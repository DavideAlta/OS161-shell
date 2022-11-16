/*
 * sysfiletest3.c
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <limits.h>

int
main()
{
	char path1[PATH_MAX+1], *p;
	char path2[PATH_MAX+1];
	//char newpath[7] = "/mytest";
	int rv;

	
	p = getcwd(path1, sizeof(path1));
	if(p == NULL){
		printf("getcwd 1 failed.\n");
		return -1;
	}

	/* ensure null termination */
	path1[31] = 0;

	printf("The current dir is %s\n",path1);

	rv = chdir("emu0:/mytest");
	if(rv == -1){
		printf("chdir failed.\n");
		return -1;
	}

	p = getcwd(path2, sizeof(path2));
	if(p == NULL){
		printf("getcwd 2 failed.\n");
		return -1;
	}

	/* ensure null termination */
	path2[PATH_MAX-1] = 0;

	printf("The current dir is %s\n",path2);

	/*fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		return -1;
	}

	rv = read(fd, readbuf1, 8);
	if (rv<0) {
		printf("File read 1 failed.\n");
		return -1;
	}

    // ensure null termination
	readbuf1[7] = 0;

    rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}*/

    printf("THE END !!!\n");

	return 0;
}