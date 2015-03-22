
/**************************************************************************

    scm.c

    SCM board related processing

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>

#include "scm.h"
#include "pmdatadefs.h"
#include "logger.h"
#include "usbdevice.h"

extern struct USBDevice *scmDevice;
extern int scmThreadActive;
extern pthread_mutex_t power_mutex;	/* Protects access to value */
extern pthread_mutex_t data_mutex;	/* Protects access to value */
extern pthread_cond_t scm_cond;
extern power_message_t power_message;
void *scm_thread_func(void *prt)
{
	int result;

	pthread_detach(pthread_self());

	unsigned int threshold_violations = 0;
	unsigned int warning_sent = 0;
	unsigned int poll_timer = DEFAULT_SCM_POLL_TIMER;

	//struct timeval selectTimeout;   /* Used for select() timeout */
	struct timespec timeout;

	ALOGI("SCM","Retrieving SCM FW Version and Serial...");

	scmGetFWVersion();
	scmGetSerialNum();
	scmGetRuntime();
	scmGetPower();
	scmGetBattery();
	if (scmDevice->board_rev == REV_B) {
		scmGetState();
	} else {
		strcpy(scmDevice->szState, "N/A");
	}

	ALOGI("SCM","	SCM Device Initialized");
	ALOGI("SCM", "	FW Version = %s", scmDevice->szFWVerNum);
	ALOGI("SCM", "	Serial Number = %s", scmDevice->szSerialNum);
	ALOGI("SCM", "	Runtime = %s", scmDevice->szRunTime);
	ALOGI("SCM", "	State = %s", scmDevice->szState);
	ALOGI("SCM", " Power(V) = %.2f Current(mA) = %d | Battery(V) = %.2f Current(mA) = %d",
		scmDevice->load_v, scmDevice->load_i, scmDevice->battery_v,
		scmDevice->battery_i);




	result = pthread_mutex_lock(&data_mutex);
	if (result != 0) {
		fprintf(stderr,"Can not lock %d\n",result);
	}

	scmThreadActive = 1;
	result = pthread_mutex_unlock(&data_mutex);
	if (result != 0) {
		fprintf(stderr,"Can not lock %d\n",result);
	}



	while (scmThreadActive) {
		result = pthread_mutex_lock(&power_mutex);
		if (result != 0) {
			ALOGE("SCM", "mutex lock err = %d\n",result);
		}

		if (power_message.request_type != PDM_REQUEST
		    && (power_message.type != PM_EXIT && scmThreadActive != 0))
		{
			timeout.tv_sec = time(NULL) + poll_timer;	/* delay 1 min */
			timeout.tv_nsec = 0;
			result =
			    pthread_cond_timedwait(&scm_cond, &power_mutex,
						   &timeout);
			if (result == ETIMEDOUT) {
				/* time out  get voltage value */

				result = pthread_mutex_unlock(&power_mutex);
				if (result != 0) {
					ALOGE("SCM","mutex unlock err = %d\n",result);
				}

				if (scmDevice->board_rev == REV_B
				    && scmThreadActive == 1) {
					scmGetState();
				}
				if (scmThreadActive == 1)
					scmGetPower();
				if (scmThreadActive == 1)
					scmGetBattery();

   				ALOGI("SCM","Power(V) = %.2f Current(mA) = %d | Battery(V) = %.2f Current(mA) = %d",
						 scmDevice->load_v, scmDevice->load_i, scmDevice->battery_v, scmDevice->battery_i);


				continue;
				/* don't need to execute the command */
				/* Get the version once per minute */
			}

		}

		if (power_message.type == PM_EXIT || scmThreadActive == 0) {
			result = pthread_mutex_unlock(&power_mutex);
			if (result != 0) {
				ALOGE("SCM","mutex unlock err = %d\n", result);
			}
			break;
		}

		/* parse the cmd , and process */
		//pdmProcessMessage(&power_message.pdm_message);
		power_message.request_type = PDM_RESPONSE;

		result = pthread_mutex_unlock(&power_mutex);
		if (result != 0) {
			ALOGE("SCM", "mutex unlock err = %d\n",	result);
		}
	}

	ALOGD("SCM", "scm Thread exit %d\n", scmThreadActive);

	return NULL;
}

/*********************************************************
 * NAME: scmGetFWVersion
 * DESCRIPTION: Get the currently installed firmware
 * version number
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
void scmGetFWVersion(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;
		    ALOGD("SCM", "Requesting FW Version from %s (%d)", scmDevice->szDevPath, scmDevice->serialPortHandle);
	nRetVal =
	    usbSendCommand(CMD_GET_DEV_FW_VERSION, scmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		// Wait for the response over serial
		do {
			scmBusyReading =
			    usbProcessResponse(scmDevice, scmDataBfr);
		}
		while (scmBusyReading);
	} else {
		ALOGE("SCM","Unable to retrieve SCM firmware version. Returned %d", nRetVal);
	}
}

/*********************************************************
 * NAME: scmGetSerialNum
 * DESCRIPTION: Get the serial number of the device
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
void scmGetSerialNum(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_SERIAL_NUM, scmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		// Wait for the response over serial
		do {
			scmBusyReading =
			    usbProcessResponse(scmDevice, scmDataBfr);
		}
		while (scmBusyReading);
	} else {
		ALOGE("SCM","Unable to retrieve SCM serial number. Returned %d",nRetVal);
	}
}

/*********************************************************
 * NAME: scmGetRuntime
 * DESCRIPTION: Get the device runtime
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
void scmGetRuntime(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_RUNTIME, scmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		// Wait for the response over serial
		do {
			scmBusyReading =
			    usbProcessResponse(scmDevice, scmDataBfr);
		}
		while (scmBusyReading);
	} else {
		ALOGE("SCM","Unable to retrieve SCM runtime. Returned %d", nRetVal);
	}
}

/*********************************************************
 * NAME: getSCMState
 * DESCRIPTION: Get the current state of the device
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
void scmGetState(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;

	/* Can only retrieve SCM state on rev B boards */
	if (scmDevice->board_rev == REV_B) {
		nRetVal =
		    usbSendCommand(CMD_GET_DEV_STATE,
				   scmDevice->serialPortHandle);
		if (nRetVal >= 0) {
			/* Wait for the response over serial */
			do {
				scmBusyReading =
				    usbProcessResponse(scmDevice, scmDataBfr);
			}
			while (scmBusyReading);

			/* Log the data read */
			ALOGI("SCM", "State %s", scmDevice->szState);
			if (strcmp(scmDevice->szState, "NCHG") == 0) {
				ALOGI("SCM", "Reason: %d", 	scmDevice->reason);
			} else if (strcmp(scmDevice->szState, "CHRG") == 0) {
				ALOGI("SCM", "PWMC = %d", scmDevice->pwmc);
				ALOGI("SCM", "MP B = %s", scmDevice->szMPB);
				ALOGI("SCM", "MP S = %s", scmDevice->szMPS);
				ALOGI("SCM", "Battery Status = %s",scmDevice->szBatteryStatus);
				ALOGI("SCM", "Temperature = %d",scmDevice->temperature);
			}
		} else {
			ALOGE("SCM","Unable to retrieve SCM state. Returned %d",nRetVal);
		}
	} else {
		ALOGI("SCM","State %s", scmDevice->szState);
	}
}

/*********************************************************
 * NAME: scmGetPower
 * DESCRIPTION: Get the current device power status
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void scmGetPower(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_POWER, scmDevice->serialPortHandle);

	if (nRetVal >= 0) {
		/* Wait for the response over serial */
		do {
			scmBusyReading =
			    usbProcessResponse(scmDevice, scmDataBfr);
		}
		while (scmBusyReading);

		if (scmDevice->board_rev == REV_A) {
			/*
			 * This is a three part message
			 * Rev A requires us to read two more times
			 */
			do {
				scmBusyReading =
				    usbProcessResponse(scmDevice, scmDataBfr);
			}
			while (scmBusyReading);

			do {
				scmBusyReading =
				    usbProcessResponse(scmDevice, scmDataBfr);
			}
			while (scmBusyReading);
		}
	} else {
		ALOGE("SCM","Unable to retrieve SCM power status. Returned %d",nRetVal);
	}
}

/*********************************************************
 * NAME: scmGetBattery
 * DESCRIPTION: Get the current device battery status
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void scmGetBattery(void)
{
	int nRetVal;
	char scmDataBfr[512];
	unsigned int scmBusyReading = 0;

	if (scmDevice == NULL)
		return;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_BATTERY, scmDevice->serialPortHandle);

	if (nRetVal >= 0) {
		/* Wait for the response over serial */
		do {
			scmBusyReading =
			    usbProcessResponse(scmDevice, scmDataBfr);
		}
		while (scmBusyReading);

		if (scmDevice->board_rev == REV_A) {
			/*
			 * This is a three part message
			 * Rev A requires us to read two more times
			 */
			do {
				scmBusyReading =
				    usbProcessResponse(scmDevice, scmDataBfr);
			}
			while (scmBusyReading);

			do {
				scmBusyReading =
				    usbProcessResponse(scmDevice, scmDataBfr);
			}
			while (scmBusyReading);
		}
	} else {
		ALOGE("SCM","Unable to retrieve SCM battery status. Returned %d",nRetVal);
	}
}
