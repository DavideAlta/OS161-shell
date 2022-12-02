/*
 * test_file_rw.c - test flow:
 *					1) Write testwrite.txt (wronly, excl and creat modes)
 *					2) Read testread.txt (rdonly mode)
 *					3) Write againg testwrite.txt (rdwr and append mode)
 *					4) Read testwrite.txt (with lseek to restart the offset)
 *					5) Write againg testwrite.txt (rdwr and trunc mode)
 *					6) Read testwrite.txt (with lseek to restart the offset)
 *
 * (test of open in all modes, write, read, close, lseek)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int
main()
{
	static char writebuf1[6+1] = "Hello!";
	static char writebuf2[12+1] = "Hello again!";
	static char writebuf3[12+1] = "Hello only!!";
	static char readbuf1[8];
	static char readbuf2[6+12+1];
	static char readbuf3[12+1];

	char file_rd[21+1] = "./mytest/testread.txt";
	char file_wr[22+1] = "./mytest/testwrite.txt";

	int fd, rv;

	// Ensure buf ternimations to \0
	writebuf1[6] = 0;
	writebuf2[12] = 0;
	file_rd[21] = 0;
	file_wr[22] = 0;

	// Create the file if it doesn't exist, fails if already exists
	fd = open(file_wr, O_EXCL|O_CREAT|O_WRONLY);
	if (fd < 0) {
		printf("File open wr (creation) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Write the opened file (-> "Hello!")
	rv = write(fd, writebuf1, 6);
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

	// Open another file to read
	fd = open(file_rd, O_RDONLY);
	if (fd<0) {
		printf("File open rd failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Read the opened file
	rv = read(fd, readbuf1, 6);
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

	// Write the opened file (buf will be appended -> "Hello!Hello again!")
	rv = write(fd, writebuf2, 12);
	if (rv<0) {
		printf("File write (append) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}
	printf("%s is appended on file %s\n",writebuf2,file_wr);

	// Move offset at 0 position to then read all the file
	rv = lseek(fd, 0, SEEK_SET);
    if(rv != 0){
        printf("lseek failed.\n");
		printf("Error: %s\n",strerror(errno));
        return -1;
    }

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
	readbuf2[18] = 0;
	
	printf("The read string (after append) is %s\n",readbuf2);

	// Re-re-open the written file in trunc mode
	fd = open(file_wr, O_RDWR|O_TRUNC);
	if (fd < 0) {
		printf("File open wr (trunc) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return -1;
	}

	// Write the opened file (after trunc)
	rv = write(fd, writebuf3, 12);
	if (rv<0) {
		printf("File write (trunc) failed.\n");
		printf("Error: %s\n",strerror(errno));
		return rv;
	}
	printf("%s is written on file %s\n",writebuf3,file_wr);

	// Move offset at 0 position to then read all the file
	rv = lseek(fd, 0, SEEK_SET);
    if(rv != 0){
        printf("lseek failed.\n");
		printf("Error: %s\n",strerror(errno));
        return -1;
    }

	// Read the opened file
	rv = read(fd, readbuf3, 12);
	if (rv<0) {
		printf("File read (after trunc) failed.\n");
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
	readbuf3[12] = 0;
	
	printf("The read string (after trunc) is %s\n",readbuf3); 

	return 0;
}