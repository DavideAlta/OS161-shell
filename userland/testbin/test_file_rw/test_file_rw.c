/*
 * test_file_rw.c - 
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int
main()
{
	// SISTEMARE TUTTO
	static char writebuf1[6+1] = "Hello!";
	static char writebuf2[11+1] = "Hello again!";
	static char readbuf1[6];
	static char readbuf2[6+11+1];

	char file_rd[22] = "./mytest/testread.txt";
	char file_wr[23] = "./mytest/testwrite.txt";

	int fd, rv;

	// Ensure buf ternimations to \0
	writebuf1[6] = 0;
	writebuf2[11] = 0;
	file_rd[21] = 0;
	file_wr[22] = 0;

	// Create the file if it doesn't exist
	fd = open(file_wr, O_CREAT|O_WRONLY);
	if (fd < 0) {
		printf("File open wr (creation) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Write the opened file
	rv = write(fd, writebuf1, 7);
	if (rv<0) {
		printf("File write failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}
	printf("%s is written on file %s\n",writebuf1,file_wr);

	// Close the file
	rv = close(fd);
	if (rv<0) {
		printf("File close wr failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

	// Open a file to read
	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Read the opened file
	rv = read(fd, readbuf1, 8);
	if (rv<0) {
		printf("File read failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

	// Close the file
	rv = close(fd);
	if (rv<0) {
		printf("File close rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

	// Ensure ternimation at \0
	readbuf1[7] = 0;
	
	printf("The read string is %s\n",readbuf1);

	// Re-open the written file in append mode
	fd = open(file_wr, O_RDWR|O_APPEND);
	if (fd < 0) {
		printf("File open wr (append) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Write the opened file (buf will be appended)
	rv = write(fd, writebuf2, 12);
	if (rv<0) {
		printf("File write (append) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}
	printf("%s is appended on file %s\n",writebuf2,file_wr);

	// Read the opened file
	rv = read(fd, readbuf2, 18);
	if (rv<0) {
		printf("File read (after append) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

	// Close the file
	rv = close(fd);
	if (rv<0) {
		printf("File close rd (after append) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}

	// Ensure ternimation at \0
	readbuf2[19] = 0;
	
	printf("The read string (after append) is %s\n",readbuf2);

	return 0;
}