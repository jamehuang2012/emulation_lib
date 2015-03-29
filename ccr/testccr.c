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

static mqd_t mq;
int main(void) 
{
	
	struct mq_attr attr;
	attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;
    static ccr_trackData_t theTrackData;
	
	mq = mq_open(MQ_NAME,  O_WRONLY, 0644, &attr);

	if ( mq == -1) {
		fprintf(stderr,"mq_open return error %d\n",errno);
		return -1; 
	}
	
	/* Asign the data */
	
	theTrackData.cardMgrResult.command = CARD_MANAGER_ACQUIRE_CARD_CMD;
	theTrackData.cardMgrResult.error = 0;
	theTrackData.cardMgrResult.status = 0;
	theTrackData.cardMgrResult.readerStatus = 0;
	//theTrackData.track1Status = 1;
	strcpy(theTrackData.track1Data,"B5113400694348629^POUR VOUS/FOR YOU^18091211000000434000000");
	strcpy(theTrackData.track2Data,"5113400694348629=180912110000434");
	theTrackData.track1DataLen = strlen(theTrackData.track1Data);
	theTrackData.track2DataLen = strlen(theTrackData.track2Data);
	theTrackData.track1Status = 0;
	theTrackData.track2Status = 0;
	theTrackData.track3DataLen = 0;
	
	
	if ( mq_send(mq, (char *)&theTrackData, sizeof(struct tagTrackData_t), 0) < 0 ) {
		fprintf(stderr,"mq send error \n");
	}
	
	sleep(1);
	
	
	return 0;
	
	
	
	
}
