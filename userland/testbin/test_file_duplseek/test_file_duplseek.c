/*
 * test_file_duplseek.c - it test dup2 and lseek:
 *						  1) dup2 duplicate fd
 *						  2) lseek move the offset using 
 *							 the cloned file descriptor.		
 *
 * (test of open, read, close, dup2, lseek)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

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
		printf("Error: %s\n",strerror(errno));
		return fd;
	}

	rv = read(fd, readbuf1, 8);
	if (rv<0) {
		printf("File read 1 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

    /* ensure null termination */
	readbuf1[7] = 0;

    printf("1st reading = %s\n",readbuf1);

	// duplicate the file descriptor 
    rv = dup2(fd, fd_clone);
    if (rv != fd_clone){
        printf("dup2 failed.\n");
        printf("Error: %s\n",strerror(errno));
		return rv;
	}

    rv = lseek(fd, -10, SEEK_END);
    if(rv != 17){ // 27 - 10
        printf("lseek failed.\n");
        printf("Error: %s\n",strerror(errno));
		return rv;
    }

    rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}
    
	// the file isn't close since fd_clone still point to it 
    rv = read(fd_clone, readbuf2, 8);
    if (rv<0) {
		printf("File read 2 failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

    /* ensure null termination */
	readbuf2[7] = 0;

    printf("2nd reading = %s\n",readbuf2);

    rv = close(fd_clone);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

    printf("Test complete successfully\n");

	return 0;
}