#include <sys/mman.h>

#include        <sys/types.h>   /* basic system data types */
#include        <sys/time.h>    /* timeval{} for select() */
#include        <time.h>                /* timespec{} for pselect() */
#include        <errno.h>
#include        <fcntl.h>               /* for nonblocking */
#include        <limits.h>              /* PIPE_BUF */
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <sys/stat.h>    /* for S_xxx file mode constants */
#include        <unistd.h>

#include   	<string.h>
#include 	<stdio.h>
#define SHM_LEN 4

#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int main(int argc,int **argv)
{
	int fd;	
	char *prt;

	fd = shm_open("shmdio",O_RDWR |O_CREAT,FILE_MODE);
	
	ftruncate(fd,SHM_LEN);

	prt = mmap(NULL,SHM_LEN,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);


	while (1) {
		
		sleep(1);
		fprintf(stderr,"%2x %2x %2x %2x", prt[0],prt[1],prt[2],prt[3]);
	}
	
	exit(0);
	
		
}
