/*
 * sysfiletest3.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main()
{
	char path1[32];
	char path2[32];
	char newpath[13] = "./emu0/mytest";
	int rv;

	rv = __getcwd(path1, 32);
	if(rv < 0){
		printf("getcwd 1 failed.\n");
		return -1;
	}

	/* ensure null termination */
	path1[31] = 0;

	printf("The current dir is %s\n",path1);

	rv = chdir(newpath);
	if(rv < 0){
		printf("chdir failed.\n");
		return -1;
	}

	rv = __getcwd(path2, 32);
	if(rv < 0){
		printf("getcwd 2 failed.\n");
		return -1;
	}

	/* ensure null termination */
	path2[31] = 0;

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