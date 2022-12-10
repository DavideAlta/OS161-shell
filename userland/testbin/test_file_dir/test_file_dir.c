/*
 * test_file_dir.c - chdir change the current working directory
 *					 getcwd show the current working directory
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <limits.h>

int
main()
{
	char path1[PATH_MAX+1], *p;
	char path2[PATH_MAX+1];
	char newpath[13] = "emu0:/mytest";
	int fd,rv;
	char file_rd[9+1] = "where.txt";
	static char readbuf[PATH_MAX+1];
	char back_dir[3+1] = "../";
	

	
	p = getcwd(path1, sizeof(path1));
	if(p == NULL){
		printf("getcwd 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	/* ensure null termination */
	path1[31] = 0;


	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	rv = read(fd, readbuf, sizeof(readbuf));
	if (rv<0) {
		printf("File read 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// ensure null termination
	readbuf[rv-1] = 0;

	printf("The current dir is %s\n",path1);
	printf("The where.txt contain: %s \n",readbuf);


	rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}


	rv = chdir(newpath);
	if(rv == -1){
		printf("chdir failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	p = getcwd(path2, sizeof(path2));
	if(p == NULL){
		printf("getcwd 2 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	/* ensure null termination */
	path2[PATH_MAX-1] = 0;


	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	rv = read(fd, readbuf, sizeof(readbuf));
	if (rv<0) {
		printf("File read 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

    // ensure null termination
	readbuf[rv-1] = 0;

	printf("The current dir is %s\n",path2);
	printf("The where.txt contain: %s \n",readbuf);

    rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}


	// return back with relative command ../
	rv = chdir(back_dir);
	if(rv == -1){
		printf("chdir failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	p = getcwd(path1, sizeof(path1));
	if(p == NULL){
		printf("getcwd 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	/* ensure null termination */
	path1[31] = 0;


	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	

	rv = read(fd, readbuf, sizeof(readbuf));
	if (rv<0) {
		printf("File read 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// ensure null termination
	readbuf[rv-1] = 0;

	printf("The current dir is %s\n",path1);
	printf("The where.txt contain: %s \n",readbuf);


	rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

    printf("test directory complete successfully\n");

	return 0;
}