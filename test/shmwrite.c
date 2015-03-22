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
#include        "dio.h"
#include    	<stdint.h>
#define SHM_LEN 4096

#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
static volatile fpga_t *fpga;
static volatile uint16_t *data;

int main(int argc,int **argv)
{
	int fd;	
	char *prt;
	int i = 0; 

	fd = shm_open("shm-dio",O_RDWR |O_CREAT,FILE_MODE);
	
	ftruncate(fd,SHM_LEN);

	prt = mmap(NULL,SHM_LEN,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    	fpga = (fpga_t *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    	data = (uint16_t *)fpga;



	while (1) {
		
		fpga->dio_in_32_47.gpio9 ^= 1;
		sleep(1);
		i ++;
		memcpy(prt,&i,4);
	}
	
	exit(0);
	
		
}
