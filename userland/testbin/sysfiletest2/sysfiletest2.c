/*
 * test_file_duplseek.c - it ...
 *
 * (test of open, read, close, dup2, lseek)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main()
{
	static char readbuf1[8];
    static char readbuf2[8];

	char file_rd[22] = "./mytest/testread.txt";

	int fd, fd_clone = 4, rv;

	// Set filename ternimation to \0
	file_rd[21] = 0;

	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		return -1;
	}

	rv = read(fd, readbuf1, 8);
	if (rv<0) {
		printf("File read 1 failed.\n");
		return -1;
	}

    /* ensure null termination */
	readbuf1[7] = 0;

    printf("1st reading = %s\n",readbuf1);

    rv = dup2(fd, fd_clone);
    if (rv != fd_clone){
        printf("dup2 failed.\n");
        return -1;
    }

    rv = lseek(fd, -10, SEEK_END);
    if(rv != 12){ // 22 - 10
        printf("lseek failed.\n");
        return -1;
    }

    rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}
    
    rv = read(fd_clone, readbuf2, 8);
    if (rv<0) {
		printf("File read 2 failed.\n");
		return -1;
	}

    /* ensure null termination */
	readbuf2[7] = 0;

    printf("2nd reading = %s\n",readbuf2);

    rv = close(fd_clone);
	if (rv<0) {
		printf("File close rd failed.\n");
		return -1;
	}

    printf("THE END !!!\n");

	return 0;
}