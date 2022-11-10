/*
 * sysfiletest2.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main()
{
	static char readbuf[8];

	char file_rd[22] = "./mytest/testread.txt";

	int fd, fd_clone = 4, rv;

	// Set filename ternimation to \0
	file_rd[21] = 0;

	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		return -1;
	}

	rv = read(fd, readbuf, 8);
	if (rv<0) {
		printf("File read 1 failed.\n");
		return -1;
	}

    /* ensure null termination */
	readbuf[7] = 0;

    printf("1st reading = %s\n",readbuf);

    rv = dup2(fd, fd_clone);
    if (rv != fd_clone){
        printf("dup2 failed.\n");
        return -1;
    }

    rv = lseek(fd, 2, SEEK_SET);
    /*if(rv != 6){
        printf("lseek failed.\n");
        return -1;
    }*/

    rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}

    rv = read(fd_clone, readbuf, 8);
    if (rv<0) {
		printf("File read 2 failed.\n");
		return -1;
	}

    /* ensure null termination */
	readbuf[7] = 0;

    printf("2nd reading = %s\n",readbuf);

    rv = close(fd_clone);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}

    printf("THE END !!!\n");

	return 0;
}