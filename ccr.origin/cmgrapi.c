/*
 * cmgrapi.c
 *
 *  Created on: Jul 11, 2013
 *      Author: james
 */

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

#include "config.h"
#include "cmgrapi.h"
#include "cardrdrdatadefs.h"
#include "svsCCR.h"
#include "Connect.h"
#include "Disconnect.h"
#include "Prtcl_RS232C_8.h"
#include "Command.h"
#include <svsPM.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include "logger.h"
#include "callback.h"

// Allow for terminating 0 byte
#define TRACK1_DATA_SIZE 79
#define TRACK2_DATA_SIZE 41
#define TRACK3_DATA_SIZE 41

#define CCR_DEVELOPMENT_READER_MODEL "3R917"

#define bool unsigned char
#define false 0
#define true 1
extern unsigned long Port;
extern unsigned long Baudrate;
bool bConnected = false;
bool bInitialized = false;
bool bAuthorized = false;
bool bAcquireCardState = false;
bool bShutdown = false;
bool bThreadExiting = false;
int nCommandStep = 0;
// For the thread to report that it has processed
// bAcquireCardState set to false
bool bAcquireCardPaused = false;

int nAcquireCardCmd = 0;
int command_step;		/* read card step status */
ccr_VersionNumbers_t gReaderFirmwareVersions;
ccr_modelSerialNumber_t gReaderModelSN;

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t icrw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t callback_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t callback_p_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cmd_cond = PTHREAD_COND_INITIALIZER;	/* Signals change to value */
pthread_cond_t icrw_cond = PTHREAD_COND_INITIALIZER;	/* Signals change to value */
pthread_cond_t callback_cond = PTHREAD_COND_INITIALIZER;	/* Signals change to value */
pthread_once_t init_block = PTHREAD_ONCE_INIT;
static ccr_cardMgrCmd_t cardMgrCmd;

static pthread_t callback_thread_id = 0;
static pthread_t cmd_thread_id = 0;
static pthread_t icrw_thread_id = 0;

static int callback_signal_data = 0;	/* 0 no call back , 1 call back */
				   /*static int readcard_flag = 0 ; *//* 0 begin to read card , 1 success */

static callback_fn_t cb_ccr = NULL;
static ccr_trackData_t theTrackData;

unsigned long Port;
unsigned long Baudrate;

/*
*   These functions are used by the Card Reader Manager application to send command results to the
*   Kiosk application, and to receive and parse the command messages from the Kiosk application.
*   They are not required for the Kiosk Application library side of the fence
*/

int checkReaderConnected(unsigned long Port, unsigned long Baudrate,
			 int cmd)
{
    int nRetVal = SUCCESS;
    int retry_counter = 0;
    if (!bConnected) {
	bInitialized = false;
	bAuthorized = false;


	nRetVal = Connect(Port, Baudrate);
	bConnected = RS232C_8_IsConnected(Port);
	if (nRetVal || !bConnected) {
	    ccr_cardMgrResult_t CardMgrResult;

	    // Return success status to Kiosk application
	    CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;

	    CardMgrResult.command = cmd;
	    CardMgrResult.error = true;
	    CardMgrResult.status = CARD_MANAGER_CONNECT_ERROR;
	    CardMgrResult.readerStatus = 0;
	    ccr_callback_send_signal(&CardMgrResult);
	    return -1;
	}
    }				// End not connected
__init:
    // Read three encrypted tracks
    if (!bInitialized) {
	nRetVal = initCardReader(Port, Baudrate);
	if (nRetVal == _RS232C_8_NO_ERROR) {
	    bInitialized = true;
	    bAuthorized = false;

	    // Get the Model number and see if we have a Release or Development
	    // Reader.  Note the code currently is built to work only with
	    // Development readers, and only with the same Master Key for
	    // each reader.
	    nRetVal =
		getModelSerialNumber(Port, Baudrate, &gReaderModelSN);

	    if (nRetVal == SUCCESS
		&& strlen((char *) gReaderModelSN.modelNumber) != 0) {
		// See if we have a Development Reader
		if (strstr
		    ((char *) gReaderModelSN.modelNumber,
		     CCR_DEVELOPMENT_READER_MODEL) == NULL) {
		    // Houston we have a problem, this code is currently built to work only with
		    // development readers.
		    // Return the error code and do NOT attempt to do any other processing
		    ccr_Info_t ccrInfo;
		    memset(&ccrInfo, 0, sizeof(ccrInfo));
		    memcpy(&ccrInfo.modelSerialNumber,
			   &gReaderModelSN, sizeof(gReaderModelSN));
		    memcpy(&ccrInfo.versionNumbers,
			   &gReaderFirmwareVersions,
			   sizeof(gReaderFirmwareVersions));

		    // Return success status to Kiosk application
		    ccrInfo.cardMgrResult.protocolVersion =
			CCR_PROTOCOL_VERSION;

		    ccrInfo.cardMgrResult.command = cmd;
		    ccrInfo.cardMgrResult.error = true;
		    ccrInfo.cardMgrResult.status =
			CARD_MANAGER_RELEASE_READER;
		    ccrInfo.cardMgrResult.readerStatus = 0;

		}
	    } else {
		// Failed to get the Model number cannot tell if this is a Development or Release reader
		// so consider this an error.
		ccr_cardMgrResult_t CardMgrResult;
		// Return success status to Kiosk application
		bAcquireCardState = false;
		CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;

		CardMgrResult.command = cmd;
		CardMgrResult.error = true;
		CardMgrResult.status = CARD_MANAGER_MODELSN_ERROR;
		CardMgrResult.readerStatus = nRetVal;
		ccr_callback_send_signal(&CardMgrResult);
	    }

	} else {
	    // Send error to Kiosk application
	    {
	   	ALOGI("CCR", "Failed to init creadit card reader,tells apps");
		ccr_cardMgrResult_t CardMgrResult;
		// Return success status to Kiosk application
		bAcquireCardState = false;
		CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;

		CardMgrResult.command = cmd;
		CardMgrResult.error = true;
		CardMgrResult.status = CARD_MANAGER_INITIALIZE_ERROR;
		CardMgrResult.readerStatus = nRetVal;
		ccr_callback_send_signal(&CardMgrResult);
	    }
	    return -2;
	}			// End of failed to init
    }				// End of not initialized

    ALOGD("CCR", "bAuthorized begin\n");
    if (!bAuthorized) {
	nRetVal = authenticateReader(Port, Baudrate);
	if (nRetVal < 0) {
	   ALOGI("CCR", "Failed Device Authentication. Error: %02d\n\n", nRetVal);
	    if (retry_counter == 0)  {
		 ALOGI("CCR","reset card and try again %d",nRetVal);
	   	 resetReader(Port,Baudrate);
		 usleep(500*1000);   /* wait for power cycling */

                 bInitialized = false;
                 bAuthorized = false;
		 retry_counter ++;
		 goto __init;
	    }
	    blinkGreenLED(Port, Baudrate);

	    bInitialized = false;
	    bAuthorized = false;
	    // Send error to Kiosk Application
	   ALOGI("CCR", "Failed to Authen creadit card reader,tells apps");
	    {
		ccr_cardMgrResult_t CardMgrResult;
		// Return success status to Kiosk application
		CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;

		CardMgrResult.command = cmd;

		CardMgrResult.error = true;
		CardMgrResult.status = CARD_MANAGER_AUTHENTICATE_ERROR;
		CardMgrResult.readerStatus = nRetVal;
		ccr_callback_send_signal(&CardMgrResult);
	    }
	    return nRetVal;
	} else {
	    bAuthorized = true;
	}
    }
    // Here if we are initialized and authenticated.
    return SUCCESS;
}

/*
 * Maximum number of errors to allow when reading 3 tracks is called.
 * When this count is hit send the non-recoverable error message and
 * put the processing loop into a no operation mode waiting to be
 * killed off.  It is expected that Application will send the Stop
 * request and then a start request after the reader has been power
 * cycled to try a hard start recovery. If the error occurs again then
 * the application should log and notify that the reader is out of service.
 */
#define MAX_TRACK_READ_RETRIES 3

void *acquireCardThread(void *arg)
{
    int nRetVal, Ret;
    int bSentUnrecoverableErrorMsg = 0;
    int retryCounter = 0;

    /*
     * Kiosk application sends this command to put the
     * card reader into "acquire card" mode.  The Card
     * Manager will wait for a card to be presented, with
     * a n Second timeout.  On each timeout the Card Manager
     * checks to see if there is another command to process.
     * If there is another Kiosk request then that request is
     * serviced.  If not then the Card Manager resumes waiting
     * for Card Intake.
     *
     * When a card it presented, then the Card Manager moves to
     * Withdraw command, and when the card is removed the Card
     * Manager moves to Read the card data.
     * The results of the Card read is sent to the Kiosk application
     * and the Card Manager resumes waiting for the next card event.
     */


    while (1) {
	nRetVal = pthread_mutex_lock(&icrw_mutex);
	if (nRetVal != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	}

	while (!bAcquireCardState && !bShutdown) {
	    nRetVal = pthread_cond_wait(&icrw_cond, &icrw_mutex);
	    if (nRetVal != 0) {
		ALOGE("CCR","pthread_cond_wait err = %d\n",	nRetVal);
		continue;
	    }
	    break;
	}

	fprintf(stderr, "got a message from magr\n");

	nRetVal = pthread_mutex_unlock(&icrw_mutex);
	if (nRetVal != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	}

	if (bShutdown) {
	    break;
	}
	Ret = pthread_mutex_lock(&data_mutex);
	if (Ret != 0) {
	    //return        _RS232C_8_CRITICAL_SECTION_ERROR;
	    continue;
	}
	nCommandStep = 0;

	if (!bAcquireCardState) {
	    bAcquireCardPaused = true;
	    bSentUnrecoverableErrorMsg = 0;
	    retryCounter = 0;
	    Ret = pthread_mutex_unlock(&data_mutex);
	    if (Ret != 0) {
		//return        _RS232C_8_CRITICAL_SECTION_ERROR;
		continue;
	    }


	    continue;
	}

	if (bShutdown) {
	   ALOGD("CCR", "shutdown = %d", bShutdown);
	    Ret = pthread_mutex_unlock(&data_mutex);
	    if (Ret != 0) {
		//return        _RS232C_8_CRITICAL_SECTION_ERROR;
		continue;
	    }


	    break;
	}

	Ret = pthread_mutex_unlock(&data_mutex);
	if (Ret != 0) {
	    //return        _RS232C_8_CRITICAL_SECTION_ERROR;
	    continue;
	}


	{

	    if (bAcquireCardState && !bShutdown) {
		bAcquireCardPaused = false;

		if (!bConnected || !bInitialized || !bAuthorized) {
		    /*
		     * No point in processing if we are not connected,
		     * initialized or Authorized.
		     */
		    continue;
		}

		/*
		 * Here we need to send the intake command, with a 3 second timeout.
		 * Then check the return and if it is not timeout and not error
		 * then send the withdraw command and finally read the tracks.
		 */
		{


		    /*
		     * Issue the Intake command, which will do Init again
		     * per Card Reader docs.
		     */
		    if (bShutdown) {
			break;
		    }

		    Ret = pthread_mutex_lock(&data_mutex);
		    if (Ret != 0) {
			//return        _RS232C_8_CRITICAL_SECTION_ERROR;
			break;
		    }
		    nCommandStep = 1;

		    Ret = pthread_mutex_unlock(&data_mutex);
		    if (Ret != 0) {
			//return        _RS232C_8_CRITICAL_SECTION_ERROR;
			break;
		    }



		    nRetVal = intakeCommand(Port, Baudrate);
		    if (nRetVal == -9) {
#ifdef PRINT_MSGS
			printf
			    ("Acquire Card Timeout: No Card Presented\n");
#endif
			// Normal when sitting an monitoring.
			continue;
		    } else if (nRetVal == 30) {
#ifdef PRINT_MSGS
			printf("Acquire Card Intake returned success\n");
#endif
			// Card was properly inserted within the timeout interval
			if (!bAcquireCardState) {
			    bAcquireCardPaused = true;
			    continue;;
			}
		    } else {
#ifdef PRINT_MSGS
			printf
			    ("Acquire Card Intake returned %d\n", nRetVal);
#endif
			/*
			 * Some other error, likely do not want to try to read.
			 * But will let Withdraw catch it, just in case.
			 */
			if (!bAcquireCardState) {
			    bAcquireCardPaused = true;
			    continue;
			}
		    }

		    if (bShutdown) {
			break;
		    }

		    nRetVal = pthread_mutex_trylock(&callback_p_mutex);
		    if (nRetVal == EBUSY) {
			continue;
		    }
		    //ccr_trackData_t theTrackData;
		    memset(&theTrackData, 0, sizeof(ccr_trackData_t));

		    nRetVal = pthread_mutex_unlock(&callback_p_mutex);
		    if (nRetVal != 0) {
			continue;
		    }




		    nRetVal = withdrawCommand(Port, Baudrate);
		    if (nRetVal == -9) {
#ifdef PRINT_MSGS
			printf("Acquire Card Timeout: Card Not Removed\n");
#endif
			/* They pushed the card in and didn't pull it out. */
			theTrackData.cardMgrResult.protocolVersion
			    = CCR_PROTOCOL_VERSION;

			theTrackData.cardMgrResult.command =
			    nAcquireCardCmd;
			theTrackData.cardMgrResult.error = true;
			theTrackData.cardMgrResult.status =
			    ERR_CCR_CARD_NOT_REMOVED;
			theTrackData.cardMgrResult.readerStatus =
			    abs(nRetVal);
			ccr_callback_send_signal(NULL);
			continue;
		    } else if (nRetVal == -7) {
#ifdef PRINT_MSGS
			printf
			    ("Acquire Card Timeout: Card Withdrawn too slowly\n");
#endif
			/* Took more than a second to withdraw the card. */
			theTrackData.cardMgrResult.protocolVersion
			    = CCR_PROTOCOL_VERSION;
			theTrackData.cardMgrResult.command =
			    nAcquireCardCmd;

			theTrackData.cardMgrResult.error = true;
			theTrackData.cardMgrResult.status =
			    ERR_CCR_CARD_REMOVED_TOO_SLOWLY;
			theTrackData.cardMgrResult.readerStatus =
			    abs(nRetVal);
			ccr_callback_send_signal(NULL);
			continue;
		    } else if (nRetVal < 0) {
#ifdef PRINT_MSGS
			printf
			    ("Acquire Card Withdraw returned error: %d\n",
			     nRetVal);
#endif
			/* Some other error. Send an error report to Kiosk */
			theTrackData.cardMgrResult.protocolVersion
			    = CCR_PROTOCOL_VERSION;
			theTrackData.cardMgrResult.command =
			    nAcquireCardCmd;

			theTrackData.cardMgrResult.error = true;
			theTrackData.cardMgrResult.status =
			    ERR_CCR_CARD_WITHDRAW_ERROR;
			theTrackData.cardMgrResult.readerStatus =
			    abs(nRetVal);
			ccr_callback_send_signal(NULL);
			continue;
		    }

		    if (!bAcquireCardState) {
			bAcquireCardPaused = true;
			continue;
		    }

		    if (bShutdown) {
			break;
		    }

		    Ret = pthread_mutex_lock(&data_mutex);
		    if (Ret != 0) {
			//return        _RS232C_8_CRITICAL_SECTION_ERROR;
			break;
		    }
		    nCommandStep = 2;
		    if (bAcquireCardState) {
			nRetVal =
			    Read3Tracks(Port, Baudrate, &theTrackData);
		    } else {
			bAcquireCardPaused = true;
		    }
		    Ret = pthread_mutex_unlock(&data_mutex);
		    if (Ret != 0) {
			//return        _RS232C_8_CRITICAL_SECTION_ERROR;
			continue;
		    }

		    if (bAcquireCardPaused == true)
			continue;

		    if (nRetVal < 0) {
			int errorCode = ERR_CCR_CARD_READ_ERROR;

			/*
			 * If error is 02 then we had an error from key related operations
			 * for the track read operation(s).
			 * This may be a hiccup, so allow a retry for 'n' times through the
			 * loop.  If we succeed next time reset the retry counter.
			 * If we hit the retry failure point for now return the
			 * ERR_CCR_CARD_MANAGER_UNRECOVERABLE_ERROR later we will
			 * add doing a Card Reader Reset, Init, and Authentication followed
			 * by resuming normal operations.
			 */
			if (retryCounter >= MAX_TRACK_READ_RETRIES) {
			    errorCode = ERR_CCR_UNRECOVERABLE_ERROR;
			}

			if (bSentUnrecoverableErrorMsg == 0) {
			    /* Send error to Kiosk application */
			    bAcquireCardState = false;
			    theTrackData.cardMgrResult.protocolVersion
				= CCR_PROTOCOL_VERSION;
			    theTrackData.cardMgrResult.command
				= nAcquireCardCmd;
			    theTrackData.cardMgrResult.error = true;
			    theTrackData.cardMgrResult.status = errorCode;
			    theTrackData.cardMgrResult.readerStatus
				= abs(nRetVal);

			    if (retryCounter >= MAX_TRACK_READ_RETRIES)
				/* Only allow sending unrecoverable error one time. */
				bSentUnrecoverableErrorMsg = 1;
			    else
				++retryCounter;
			    ccr_callback_send_signal(NULL);
			}
			continue;
		    }

		    retryCounter = 0;

		    /* Here we should have track data, or track related errors,
		     * so return status to Kiosk application. Combine each track
		     * data, with data length and status. Send as one buffer with
		     * result structure first in the buffer.
		     */
		    theTrackData.cardMgrResult.protocolVersion =
			CCR_PROTOCOL_VERSION;
		    theTrackData.cardMgrResult.command = nAcquireCardCmd;
		    theTrackData.cardMgrResult.error = false;

		    /*
		     * See if we have track 1 or track 2, if both are MIA then
		     * we have a bad status and no card data.
		     */
		    if (!theTrackData.track1Status
			&& !theTrackData.track2Status) {
			theTrackData.cardMgrResult.status =
			    ERR_CCR_HAS_TRACK1AND2;
		    } else if (!theTrackData.track1Status) {
			theTrackData.cardMgrResult.status =
			    ERR_CCR_HAS_TRACK1;
		    } else if (!theTrackData.track2Status) {
			theTrackData.cardMgrResult.status =
			    ERR_CCR_HAS_TRACK2;
		    } else {
			theTrackData.cardMgrResult.status =
			    ERR_CCR_HAS_NOTRACKS;
		    }
		    theTrackData.cardMgrResult.readerStatus = 0;

		    if (bShutdown) {
			break;
		    }
		    ccr_callback_send_signal(NULL);

		}
	    }			// End of Acquire Card State is active

	    if (bShutdown) {
		break;
	    }
	}			// End of forever loop
    }				// End of the condition variable loop

    /* Kill thread, we are shutting down */

    printf("acuireCard Thread is exiting ...");
    bAcquireCardState = false;

    blinkGreenLED(Port, Baudrate);
    Disconnect(Port, Baudrate);

    bThreadExiting = true;
    return arg;
}

/*
 * 	manage the request commands
 */

void *msgrCardThread(void *arg)
{
    int nRet;
    int nRetVal = SUCCESS;
    ccr_cardMgrResult_t CardMgrResult;
    //theTrackData
    ccr_Info_t ccrInfo;
    ccr_card_manager_cmds command_recv;


    memset(&CardMgrResult, 0, sizeof(ccr_cardMgrResult_t));
    memset(&ccrInfo, 0, sizeof(ccr_Info_t));

    while (!bShutdown) {

	nRet = pthread_mutex_lock(&cmd_mutex);
	if (nRet != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	    continue;
	}

	while (cardMgrCmd.command == CARD_MANAGER_NO_CMD) {

	    nRet = pthread_cond_wait(&cmd_cond, &cmd_mutex);
	    if (nRet != 0) {
		ALOGE("CCR", "pthread_cond_wait err = %d\n",nRetVal);
		continue;
	    }

	    break;
	}


	command_recv = cardMgrCmd.command;


	/*
	 * if we have get the cmd ,
	 */
	if (command_recv == CARD_MANAGER_INIT_CMD) {
	    /*
	     *   Kiosk application has requested that the Card Manager
	     *   Initialize the Card reader.  This should be the first
	     *   command that the Kiosk application sends after
	     *   registering with the Card Manager.
	     */


	    memcpy(&ccrInfo.modelSerialNumber, &gReaderModelSN,
		   sizeof(gReaderModelSN));
	    memcpy(&ccrInfo.versionNumbers,
		   &gReaderFirmwareVersions,
		   sizeof(gReaderFirmwareVersions));

	    // Return success status to Kiosk application
	    ccrInfo.cardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;
	    ccrInfo.cardMgrResult.command = CARD_MANAGER_INIT_CMD;
	    ccrInfo.cardMgrResult.error = false;
	    ccrInfo.cardMgrResult.status = SUCCESS;
	    ccrInfo.cardMgrResult.readerStatus = 0;

	    if (!bAcquireCardState) {
		nRetVal =
		    checkReaderConnected(Port, Baudrate,
					 cardMgrCmd.command);
		if (nRetVal == SUCCESS) {
		    nAcquireCardCmd = CARD_MANAGER_ACQUIRE_CARD_CMD;
		    // Fill in the cached firmware versions
		    memset(&gReaderFirmwareVersions, 0,
			   sizeof(gReaderFirmwareVersions));

		    nRetVal =
			getFirmwareVersions(Port, Baudrate,
					    &gReaderFirmwareVersions);
		    if (nRetVal < 0) {
			// Error getting Version numbers
			/*
			   Punt for now, as we have already sent back the error from check Reader Connected.
			 */
			memset(&gReaderFirmwareVersions,
			       0, sizeof(gReaderFirmwareVersions));
		    }
		    memcpy(&ccrInfo.versionNumbers,
			   &gReaderFirmwareVersions,
			   sizeof(gReaderFirmwareVersions));

		    nRetVal =
			getModelSerialNumber(Port, Baudrate,
					     &gReaderModelSN);
		    if (nRetVal < 0) {
			// Error getting model and SN
			/*
			   Punt for now, as we have already sent back the error from check Reader Connected.
			 */
			memset(&gReaderModelSN, 0, sizeof(gReaderModelSN));
		    }
		    memcpy(&ccrInfo.modelSerialNumber,
			   &gReaderModelSN, sizeof(gReaderModelSN));


		    nRet = pthread_mutex_lock(&icrw_mutex);
		    if (nRet != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRet);
		    }
		    bAcquireCardState = true;	// Start processing for cards.

		    nRet = pthread_cond_signal(&icrw_cond);
		    if (nRet != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRet);
		    }

		    nRet = pthread_mutex_unlock(&icrw_mutex);
		    if (nRet != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRet);
		    }

		} else {
		    fprintf(stderr, "check connect = %d\n", nRetVal);
		    ALOGD("CCR",  "start CCR nRetVal = %d", nRetVal);
		    ccrInfo.cardMgrResult.error = true;
		    ccrInfo.cardMgrResult.status =
			CARD_MANAGER_CONNECT_ERROR;
		    ccrInfo.cardMgrResult.readerStatus = abs(nRetVal);

		}
	    }			// End of not in acquire card state
	} else if (command_recv == CARD_MANAGER_STANDBY_CMD) {
	    /*
	     *   Sent by the Kiosk application to request the Card Manager
	     *   to disconnect from the Card Reader so that it is ready
	     *   when the Card Reader is powered up for the next customer.
	     *   After the Card Manager returns the Kiosk application can
	     *   continue with powering down the Card Reader and placing the
	     *   SBC into standby mode.  Note that when the Kiosk application
	     *   wakes up it will need to send the Acquire Card command again
	     *   to inform the Card Manager that the Reader is powered up.
	     *
	     */

	    // Return success status to Kiosk application
	    CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;
	    CardMgrResult.command = cardMgrCmd.command;
	    CardMgrResult.error = false;
	    CardMgrResult.status = SUCCESS;
	    CardMgrResult.readerStatus = SUCCESS;


	    nRet = pthread_mutex_lock(&icrw_mutex);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }
	    if (bAcquireCardState) {
		bAcquireCardState = false;	// Tell thread to stop card reader processing
		cancelCommand(Port, Baudrate);
		blinkGreenLED(Port, Baudrate);
		Disconnect(Port, Baudrate);
		bConnected = RS232C_8_IsConnected(Port);
		bAuthorized = 0;
		bInitialized = 0;
	    }

	    nRet = pthread_cond_signal(&icrw_cond);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }

	    nRet = pthread_mutex_unlock(&icrw_mutex);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }


	} else if (command_recv == CARD_MANAGER_EXIT) {
	    // Request the Card Manager to shutdown
	    // Stop the background thread,
	    // and exit() for now later we will clean up.


	    nRet = pthread_mutex_lock(&icrw_mutex);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }
	    if (bAcquireCardState) {
		bAcquireCardState = false;	// Tell thread to stop card reader processing
		cancelCommand(Port, Baudrate);
		bShutdown = true;
	    }

	    nRet = pthread_cond_signal(&icrw_cond);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }

	    nRet = pthread_mutex_unlock(&icrw_mutex);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
	    }

	    CardMgrResult.protocolVersion = CCR_PROTOCOL_VERSION;
	    CardMgrResult.command = cardMgrCmd.command;
	    CardMgrResult.error = false;
	    CardMgrResult.status = SUCCESS;
	    CardMgrResult.readerStatus = SUCCESS;




	}
	cardMgrCmd.command = CARD_MANAGER_NO_CMD;
	nRetVal = pthread_mutex_unlock(&cmd_mutex);
	if (nRetVal != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	    continue;
	}
    }

    printf("msgrCard Thread exit\n");
    return arg;
}

/* send signal to callback thread */

void ccr_callback_send_signal(ccr_cardMgrResult_t * ccr_result)
{
    int nRet;
    nRet = pthread_mutex_lock(&callback_mutex);
    if (nRet != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRet);
    }
    callback_signal_data = 1;

    if (ccr_result != NULL) {
	memcpy(&theTrackData.cardMgrResult, ccr_result,
	       sizeof(ccr_cardMgrResult_t));
    }

    nRet = pthread_cond_signal(&callback_cond);
    if (nRet != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRet);
    }

    nRet = pthread_mutex_unlock(&callback_mutex);
    if (nRet != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRet);
    }


}

/* call back routine */

void *ccr_callback_thread(void *arg)
{

    int nRet;

    while (!bShutdown) {

	nRet = pthread_mutex_lock(&callback_mutex);
	if (nRet != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRet);
	    continue;
	}

	if (callback_signal_data == 0) {
	    nRet = pthread_cond_wait(&callback_cond, &callback_mutex);
	    if (nRet != 0) {
		ALOGE("CCR", "pthread_cond_wait err = %d\n",
			nRet);
		continue;
	    }
	}

	if (bShutdown) {
	    nRet = pthread_mutex_unlock(&callback_mutex);
	    if (nRet != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRet);
		continue;
	    }
	    break;
	}
	/* call callback function */
	if (cb_ccr != NULL) {
	    nRet = pthread_mutex_lock(&callback_p_mutex);
	    if (nRet != 0) {
		continue;
	    }
	    (cb_ccr) (0, 0, (uint8_t *) & theTrackData, 0);
	    nRet = pthread_mutex_unlock(&callback_p_mutex);
	    if (nRet != 0) {
		continue;
	    }
	}
	callback_signal_data = 0;
	nRet = pthread_mutex_unlock(&callback_mutex);
	if (nRet != 0) {
	    fprintf(stderr, "mutex lock err = %d\n", nRet);
	    continue;
	}

	if (bShutdown) {
	    break;
	}


    }
    return arg;
}

/* create thread ,initialize all parameter */

void ccr_init_routine(void)
{
    int nRetVal;
    setDefaults();
    ALOGD("CCR","readConfig ....");
    nRetVal = readConfig();
    if (nRetVal) {
	   
		ALOGE("CCR","Error processing configuration file...exiting.");
    }

   
    updateConfiguration();
    RS232C_SetBaseDevName(theConfiguration.szSerialPortBaseName);

    Port = 1;			// Dummy this as partial fix for Sankyo's messed up serial port naming.
    // atol(theConfiguration.szSerialPortNumber);
    Baudrate = 38400;
    cardMgrCmd.command = CARD_MANAGER_NO_CMD;
    if ((nRetVal =
	 pthread_create(&icrw_thread_id, NULL, msgrCardThread,
			(void *) NULL))) {
	ALOGE("CCR","Unable to spawn msgrCardThread thread.");
    }


    if ((nRetVal =
	 pthread_create(&cmd_thread_id, NULL, acquireCardThread,
			(void *) NULL))) {
		ALOGE("CCR","Unable to spawn acquirecard thread thread.");
    }


    if ((nRetVal =
	 pthread_create(&callback_thread_id, NULL, ccr_callback_thread,
			(void *) NULL))) {
	 ALOGE("CCR","Unable to spawn ccr callback thread.");
    }

}

/* init ccr module*/
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

/* start CCR */

int start_ccr(void)
{
    int nRetVal;
    nRetVal = pthread_mutex_lock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }

    cardMgrCmd.command = CARD_MANAGER_INIT_CMD;
    ALOGD("CCR", "start ccr cardMgrCmd.command =%d\n",
	    cardMgrCmd.command);
    nRetVal = pthread_cond_signal(&cmd_cond);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
    }
    nRetVal = pthread_mutex_unlock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }



    return 0;
}


/* Stop CCR */

int stop_ccr(void)
{

    int nRetVal;

    nRetVal = pthread_mutex_lock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }
    callback_signal_data = 0;
    cardMgrCmd.command = CARD_MANAGER_STANDBY_CMD;
    nRetVal = pthread_cond_signal(&cmd_cond);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
    }

    ALOGD("CCR","stop ccr....");
    nRetVal = pthread_mutex_unlock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }

    return 0;
}


/* exit thread */
int exit_ccr(void)
{

    int nRetVal;
    nRetVal = pthread_mutex_lock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }

    cardMgrCmd.command = CARD_MANAGER_EXIT;

    nRetVal = pthread_cond_signal(&cmd_cond);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
    }

    nRetVal = pthread_mutex_unlock(&cmd_mutex);
    if (nRetVal != 0) {
	fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	return -1;
    }

    return 0;
}

/* register callback function */
int ccr_callback_register(callback_fn_t callback)
{
    cb_ccr = callback;
    return 0;
}


/*
    Close the ccr power suplly
    If we cut the power within 1 second during the initilization process,
    It has risk  to break the CCR reader
    so there are some rules below:
    1. After the CCR shut down : winth 5 second , it's not allowed to power up
    2. In the initialization process (after CCR power up 1 second),
    it's not allowed to shut down the CCR

*/

static const int POWERON_DELAY = 5500; //ms
static const int POWERDOWN_DELAY = 2000; //ms

static pthread_mutex_t ccr_power_mutex = PTHREAD_MUTEX_INITIALIZER;
static int ccr_operation = 0;  // 0 does nothing , 1 close power, 2 power up
static struct timeval timestamp_on  ;  // powerup timestamp
static struct timeval timestamp_off;  // power down timestamp
static int timestamp_on_s = 0; // 0 unknown , 1 spec
static int timestamp_off_s = 0; // 0 unknown , 1 spec


static void ccr_power_handler(int sig)
{
    int rc;
    rc = pthread_mutex_lock(&ccr_power_mutex);
    fprintf(stderr,"ccr handler op=%d",ccr_operation);
    if (rc != 0) {
        ALOGE("CCR","ccr mutext lock failed");
    }
    if (ccr_operation == 1) {
        pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_OFF,1);
        gettimeofday(&timestamp_off,0);
    } else if (ccr_operation == 2) {
        pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_ON,1);
        gettimeofday(&timestamp_on,0);
    }

    rc = pthread_mutex_unlock(&ccr_power_mutex);

    if (rc != 0) {
        ALOGE("CCR","Fail to unlock mutex");
    }
}

long ccr_cal_tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec-older.tv_sec)*1000+
    (newer.tv_usec-older.tv_usec)/1000;
}


void set_op_state(int state)
{
   int rc;
   rc = pthread_mutex_lock(&ccr_power_mutex);
   if (rc != 0) {
        ALOGE("CCR","Fail to lock mutex");
    }
    ccr_operation = state;
    rc = pthread_mutex_unlock(&ccr_power_mutex);

    if (rc != 0) {
       ALOGE("CCR","Fail to unlock mutex");
    }

}

int ccr_power_on(void)
{

    int status = 0;
    struct timeval ts;
    int time_diff_val;

    int rc;
    rc = pthread_mutex_lock(&ccr_power_mutex);
    if (rc != 0) {
       ALOGE("CCR","Fail to lock mutext");
    }
    fprintf(stderr,"ccr_power on begin\n");



    ALOGI("CCR","start powering on.. ");

    pdm_get_state(PDM_CCR_DEV_POWER_PORT,&status);
    ALOGD("CCR","[%s]port status = %d\n",__func__,status);
   /* due to pdm pull off to excute the cmd from host , it has problem that */
   status = 0; 
    if (status == 0 ) {

       // ccr_operation = 2;
      //  set_op_state(2);

        if (timestamp_off_s <= 0) {
            gettimeofday(&timestamp_off,0);
            //delay 5 second to power on
              usleep(POWERON_DELAY * 1000);
              pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_ON,1);
              gettimeofday(&timestamp_on,0);
              timestamp_off_s = 1;
			  ALOGD("CCR","Delay 5 second, then power up the CCR");

        } else {
            gettimeofday(&ts,0);
            time_diff_val = ccr_cal_tvdiff(ts,timestamp_off);

            ALOGD("CCR","timer diff =%d\n",time_diff_val);
            if (time_diff_val >= POWERON_DELAY) {
                pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_ON,1);
                gettimeofday(&timestamp_on,0);
                timestamp_on_s = 1;
                ALOGD("CCR","CCR has powered off for a long time,power up immidirtly");

            } else {
                time_diff_val = POWERON_DELAY - time_diff_val;
                if (time_diff_val <= 0 || time_diff_val > POWERON_DELAY ) {
                    time_diff_val = POWERON_DELAY;
                }
                usleep(time_diff_val * 1000);
                pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_ON,1);
                gettimeofday(&timestamp_on,0);
                timestamp_on_s = 1;
                ALOGD("CCR", "CCR hasn't powered off more than 5 seconds");

            }
        }
    //calculate the time
    } else {

        if (timestamp_off_s == 0) {
            gettimeofday(&timestamp_on,0);
            timestamp_off_s = 1;
        }
    }
     rc = pthread_mutex_unlock(&ccr_power_mutex);

     if (rc != 0) {
        ALOGE("CCR","%s ","unlock thread failed");
     }


    return 0;
}

int ccr_power_off(void)
{

    int status = 0;
    struct timeval ts;
    int time_diff_val;


    int rc;
   rc = pthread_mutex_lock(&ccr_power_mutex);
   if (rc != 0) {
       ALOGE("CCR","pthread mutex lock failed");
    }

  

    ALOGD("CCR:","start powering off.. ");


    pdm_get_state(PDM_CCR_DEV_POWER_PORT,&status);
    //fprintf(stderr,"[%s]port status = %d\n",__func__,status);
    ALOGD("CCR","port status = %d\n",status);
    if (status == 1 ) {


        //ccr_operation = 1;
      //  set_op_state(1);

        if (timestamp_on_s <= 0) {
            gettimeofday(&timestamp_on,0);
            //delay 5 second to power on
            timestamp_on_s = 1;
            usleep(POWERDOWN_DELAY * 1000);
            pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_OFF,1);
            gettimeofday(&timestamp_off,0);
            timestamp_off_s = 1;
            ALOGD("CCR","ccr_power off over %d ms\n",time_diff_val);

          

        } else {
            gettimeofday(&ts,0);
            time_diff_val = ccr_cal_tvdiff(ts,timestamp_on);
            ALOGD("CCR","timer diff value=%d\n",time_diff_val);
            if (time_diff_val >= POWERDOWN_DELAY) {
                pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_OFF,1);
                 gettimeofday(&timestamp_off,0);
                 timestamp_off_s = 1;
                

            } else {
                time_diff_val = POWERDOWN_DELAY - time_diff_val;
                if (time_diff_val <= 0 || time_diff_val > POWERDOWN_DELAY ) {
                    time_diff_val = POWERDOWN_DELAY;
                }


                ALOGD("CCR","ccr_power off wait %d ms\n",time_diff_val);
                usleep(time_diff_val * 1000);
                pdm_set_state(PDM_CCR_DEV_POWER_PORT,PDM_POWER_DEV_OFF,1);
                gettimeofday(&timestamp_off,0);
                timestamp_off_s = 1;
                ALOGD("CCR","ccr_power off over %d ms\n",time_diff_val);

            }
        }
    //calculate the time
    } else {
        ALOGD("CCR"," calculate the time \n");
        if (timestamp_on_s == 0) {
            gettimeofday(&timestamp_off,0);
	    timestamp_on_s = 1;

        }
    }
    
    rc = pthread_mutex_unlock(&ccr_power_mutex);

     if (rc != 0) {
        ALOGE("CCR","pthread unlock error");
     }
    
    return 0;

}
