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

	const char *file_rd = "teostread.txt";
	const char *file_wr = "teostwrite.txt";

	int fd, rv;

	fd = open(file_wr, O_CREAT|O_WRONLY, 0664);
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

	fd = open(file_rd, O_RDONLY, 0664);
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
	
	printf("Passed filetest.\n");
	return 0;
}
