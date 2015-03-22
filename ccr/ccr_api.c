#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <pthread.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include "logger.h"
#include "callback.h"
#include "cardrdrdatadefs.h"



#define TRACK1_DATA_SIZE 79
#define TRACK2_DATA_SIZE 41
#define TRACK3_DATA_SIZE 41

#define CCR_DEVELOPMENT_READER_MODEL "3R917"

static callback_fn_t cb_ccr = NULL;
static ccr_trackData_t theTrackData;


static sem_t sem_cb; 


pthread_once_t init_block = PTHREAD_ONCE_INIT;

/*  Call back routine */

void *ccr_callback_thread(void *arg) 
{
	/* binary sem wait */

	sem_wait(&sem_cb);

	(cb_ccr) (0, 0, (uint8_t *) & theTrackData, 0);
	
}

/*
*	CCR emulation  
*/
void ccr_init_routine(void)
{
	sem_init(&sem_cb,0,-1);  /* binary semaphore*/	
}

int ccr_callback_register(callback_fn_t callback)
{
    cb_ccr = callback;
    return 0;
}

int ccr_init(void)
{
    int status;
    status = pthread_once(&init_block, ccr_init_routine);
    if (status != 0) {
        fprintf(stderr, "init thread fail\n");
        return -1;
    }

    return 0;

}


int start_ccr(void)
{
	return 0;
}

int stop_ccr(void)
{
	return 0;
}


int exit_ccr(void)
{
	return 0;
}
	

