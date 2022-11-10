/*
 * sysfiletest.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main()
{
	static char writebuf[8] = "Hello!\n";
	static char readbuf[8];

	char file_rd[22] = "./mytest/testread.txt";
	char file_wr[23] = "./mytest/testwrite.txt";

	int fd, rv;

	// Set filename ternimation to \0
	file_rd[21] = 0;
	file_wr[22] = 0;

	fd = open(file_wr, O_CREAT|O_WRONLY);
	if (fd<0) {
		printf("File open wr failed.\n");
		return -1;
	}

	rv = write(fd, writebuf, 8);
	if (rv<0) {
		printf("File write failed.\n");
		return -1;
	}

	rv = close(fd);
	if (rv<0) {
		printf("File close wr failed.\n");
		return -1;	}

	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		return -1;
	}

	rv = read(fd, readbuf, 8);
	if (rv<0) {
		printf("File read failed.\n");
		return -1;
	}

	rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}

	/* ensure null termination */
	readbuf[7] = 0;
	
	printf("The read string is %s\n",readbuf);

	return 0;
}
