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
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>



#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include "logger.h"
#include "callback.h"
#include "cardrdrdatadefs.h"


#define MQ_NAME "/mq_ccr"

#define MAX_SIZE    1024


#define TRACK1_DATA_SIZE 79
#define TRACK2_DATA_SIZE 41
#define TRACK3_DATA_SIZE 41

#define CCR_DEVELOPMENT_READER_MODEL "3R917"

static callback_fn_t cb_ccr = NULL;
static ccr_trackData_t theTrackData;

static int ccr_active = 0; /* 0 -- no active or power off , 1 - active and power on*/

static sem_t sem_cb; 

static pthread_t callback_thread_id = 0;

static pthread_t icrw_thread_id = 0;

pthread_once_t init_block = PTHREAD_ONCE_INIT;

/*  Call back routine */

void *ccr_callback_thread(void *arg) 
{
	/* binary sem wait */
	while ( 1) {
	sem_wait(&sem_cb);

	(cb_ccr) (0, 0, (uint8_t *) & theTrackData, 0);
	}
	
}


static mqd_t mq;
/* CCR mangarment thread */

void *ccr_msgr_thread(void *arg)
{
	ssize_t bytes_read;
	ccr_trackData_t trackData;
	/* Receive cmd from other thread , let's say from lua task */
	/* If power is on and get cmd from  start_ccr */
	/* the data from MsgQue will procedure to callback */
	
	while (1) {
		bytes_read = mq_receive(mq, (char *)&trackData, MAX_SIZE, NULL);
		ALOGD("CCR","Byte read = %d", bytes_read);
		if ( bytes_read == sizeof(struct tagTrackData_t)) {
			//ALOGD("CCR","return = %d",trackData.cardMgrResult);
			ALOGD("CCR","Track1 Status = %d  lenghth = %d",trackData.track1Status,trackData.track1DataLen);
			ALOGD("CCR","Track2 Status = %d length = %d",trackData.track2Status,trackData.track2DataLen);
			ALOGD("CCR","Track3 Status %d length = %d",trackData.track3Status,trackData.track3DataLen);
			ALOGD("CCR","Track1 = %s", trackData.track1Data);
			ALOGD("CCR","Track2 = %s", trackData.track2Data);
			if (ccr_active == 1 ) {
				//sem_post(&sem_cb);
				(cb_ccr) (0, 0, (uint8_t *) & trackData, 0);
			} else {
				ALOGD("CCR","CCR not active or power off = %d",ccr_active);
			}
		
			
		}
		
		
		
		
	}
		
}

/*
*	CCR emulation  
*/
void ccr_init_routine(void)
{
	int result;
	struct mq_attr attr;
	attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;
    
	sem_init(&sem_cb,0,0);  /* binary semaphore*/	
	mq = mq_open(MQ_NAME, O_CREAT | O_RDONLY, 0644, &attr);
	if ( mq == -1) {
		ALOGD("CCR","mq_open return error %d %d",errno,mq);
	}
	
	

    if ((result =
	 pthread_create(&icrw_thread_id, NULL, ccr_msgr_thread,
			(void *) NULL))) {
		ALOGE("CCR","Unable to spawn acquirecard thread thread.");
    }


    if ((result =
	 pthread_create(&callback_thread_id, NULL, ccr_callback_thread,
			(void *) NULL))) {
	 ALOGE("CCR","Unable to spawn ccr callback thread.");
    }
    
    
	
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
        ALOGD("CCR", "init thread fail");
        return -1;
    }

    return 0;

}


int start_ccr(void)
{
	ccr_active = 1; 
	ALOGD("CCR","start_ccr");
	return 0;
}

int stop_ccr(void)
{
	ccr_active = 0;
	ALOGD("CCR","stop_ccr");
	return 0;
}


int exit_ccr(void)
{
	ALOGD("CCR","exit_ccr");
	return 0;
}
	

int ccr_power_on(void)
{
	ALOGD("CCR","ccr_power_on");
	return 0;
}

int ccr_power_off(void)
{
	ALOGD("CCR","ccr_power_off");
	ccr_active = 0;
	return 0;
}

