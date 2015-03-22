
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
#include <sys/time.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>

#include "pdm.h"
#include "pmdatadefs.h"
#include "logger.h"
#include "usbdevice.h"
#include "pdmqueue.h"

extern power_message_t power_message;
extern int pdmThreadActive;

extern pthread_mutex_t power_mutex;	/* Protects access to value */
extern pthread_mutex_t pm_mutex;	/* Protects access to value */
extern pthread_mutex_t data_mutex;	/* Protects access to value */

extern pthread_cond_t pdm_cond;
extern pthread_cond_t power_cond;
extern struct MessageList pdm_list;
static struct timeval timestamp_ccr_on  ;  // powerup timestamp
static struct timeval timestamp_ccr_off;  // power down timestamp

long cal_tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec-older.tv_sec)*1000+
    (newer.tv_usec-older.tv_usec)/1000;
}


//extern power_message_t resp_message;
void *pdm_thread_func(void *prt)
{
	int result;
	int nRetVal;
	power_message_t pdm_message;
	unsigned int poll_timer = DEFAULT_PDM_POLL_TIMER;

	//struct timeval selectTimeout;   /* Used for select() timeout */
	struct timespec timeout;
	struct MessageEntry *entry;

	pthread_detach(pthread_self());

	/* get the information first */
	pdmGetFWVersion();
	pdmGetSerialNum();
	pdmGetState();
	pdmGetLoad();
	pdmGetPowerPorts();
    /* setup the timestamp of CCR port*/
    if (pdmDevice->powerPortState[PDM_CCR_DEV_POWER_PORT] == PDM_POWER_DEV_ON) {
        gettimeofday(&timestamp_ccr_on,0);
        gettimeofday(&timestamp_ccr_off,0);
    } else {
        gettimeofday(&timestamp_ccr_on,0);
        gettimeofday(&timestamp_ccr_off,0);
    }
	/* Initialize the low power threshold */
	pdmDevice->threshold = DEFAULT_THRESHOLD;

	nRetVal = pthread_mutex_lock(&pm_mutex);
	power_message.type = PDM_ACTIVITY;
	nRetVal = pthread_cond_signal(&power_cond);
	if (nRetVal != 0) {
		ALOGE("PDM","mutex lock err = %d\n", nRetVal);
	}

	nRetVal = pthread_mutex_unlock(&pm_mutex);
	if (nRetVal != 0) {
		ALOGE("PDM","mutex lock err = %d\n", nRetVal);
	}

	nRetVal = pthread_mutex_lock(&data_mutex);
	if (nRetVal != 0) {
		ALOGE("PDM","Can not lock %d\n",nRetVal);
	}

	pdmThreadActive = 1;
	nRetVal = pthread_mutex_unlock(&data_mutex);
	if (nRetVal != 0) {
		ALOGE("PDM","Can not lock %d\n",nRetVal);
	}


	while (pdmThreadActive) {
		result = pthread_mutex_lock(&power_mutex);
		if (result != 0) {
			ALOGE("PDM", "mutex lock err = %d\n",result);
		}

		if (pdm_entry_empty(&pdm_list) == 0
		    && (power_message.type != PM_EXIT || pdmThreadActive != 0))
		{
			timeout.tv_sec = time(NULL) + poll_timer;	/* delay 1 min */
			timeout.tv_nsec = 0;
			result =
			    pthread_cond_timedwait(&pdm_cond, &power_mutex,
						   &timeout);
			if (result == ETIMEDOUT) {
				/* time out  get voltage value */
				pdm_message.type = PDM_GET_POWER_STATUS;
				/* push the message into queue */

			entry = pdm_add_list_entry(&pdm_list, &pdm_message);

			}
		}


		if (power_message.type == PM_EXIT || pdmThreadActive == 0) {

			result = pthread_mutex_unlock(&power_mutex);
			if (result != 0) {
				ALOGE("PDM","mutex unlock err = %d\n", result);
			}
			break;
		}



		entry = pdm_get_first_entry(&pdm_list);


		if (entry != NULL) {
			memcpy(&pdm_message, entry->message,
			       sizeof(power_message_t));
			pdm_del_first_entry(&pdm_list);
			entry = NULL;
		} else {
			result = pthread_mutex_unlock(&power_mutex);
			if (result != 0) {
				ALOGE("PDM","mutex unlock err = %d\n", result);
			}
			fprintf(stderr, "entry is NULL \n");
			continue;
		}

		result = pthread_mutex_unlock(&power_mutex);
		if (result != 0) {
			ALOGE("PDM", "mutex unlock err = %d\n",	result);
		}

		/* parse the cmd , and process */
		if (pdmThreadActive == 1) {
			pdmProcessMessage(&pdm_message);
			power_message.request_type = PDM_RESPONSE;
		}

	}

	ALOGE("PDM","PDM Thread exit %d\n", pdmThreadActive);
	return 0;
}

int pdm_send_response(pthread_cond_t *cond,pthread_mutex_t *mutex)
{
    int rc;
    rc = pthread_mutex_lock(mutex);
    if (rc != 0) {
        ALOGE("PDM","error %d",errno);
        return rc;
    }


    rc = pthread_cond_signal(cond);
    if (rc != 0) {
        ALOGE("PDM","error %d",errno);
        return rc;
    }

    rc = pthread_mutex_unlock(mutex);
    if (rc != 0) {
        ALOGE("PDM","error %d",errno);
        return rc;
    }

    return 0;

}

static const int CCR_POWERON_DELAY = 5000; //ms
static const int CCR_POWERDOWN_DELAY = 2000; //ms


/*********************************************************
 * NAME: pdmProcessMessage
 * DESCRIPTION: This function is called when a message is
 *              needs to be received and processed. It will
 *              call the appropriate function to proccess
 *              the message type
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void pdmProcessMessage(power_message_t * message)
{
    struct timeval ts;
    int result =1;
	switch (message->type) {
	case PDM_GET_PORT_STATE:
		{

			pdmGetPowerPort(message->message.pdm_port_state.port);
		}
		break;

	case PDM_SET_PORT_STATE:
		{
            result =1;
            gettimeofday(&ts,0);
            if (pdmDevice->powerPortState[message->message.pdm_port_state.port] == PDM_CCR_DEV_POWER_PORT) {
                 if (message->message.pdm_port_state.state == 1 &&
                        pdmDevice->powerPortState[message->message.pdm_port_state.port] == 0) {
                    if (cal_tvdiff(ts,timestamp_ccr_off) >= CCR_POWERON_DELAY) {
                        result = 1;
                    } else {
                        result = 0;
                    }

                 } else if (message->message.pdm_port_state.state == 0 &&
                        pdmDevice->powerPortState[message->message.pdm_port_state.port] == 1) {
                       if (cal_tvdiff(ts,timestamp_ccr_on) >= CCR_POWERDOWN_DELAY) {
                            result = 1;
                        } else {
                            result = 0;
                        }
                 } else {
                    result = 0;  // dont need to update
                 }

            }
            if (result == 1) {
                pdmSetPowerPort(message->message.pdm_port_state.port,
                        message->message.pdm_port_state.state);
                pdmDevice->powerPortState[message->message.pdm_port_state.port] = message->message.pdm_port_state.state;

                if (pdmDevice->powerPortState[message->message.pdm_port_state.port] == PDM_CCR_DEV_POWER_PORT)
                {
                    if (message->message.pdm_port_state.state == 1) {
                        gettimeofday(&timestamp_ccr_on,0);
                    } else {
                        gettimeofday(&timestamp_ccr_off,0);
                    }
                }
            }
           if (message->mutex != NULL && message->cond != NULL) {
                pdm_send_response(message->cond,message->mutex);
            }

		ALOGD("PDM","set state type =%d\n",message->type);
			pdmSetPowerPort(message->message.pdm_port_state.port,
					message->message.pdm_port_state.state);
            if (message->mutex != NULL && message->cond != NULL) {
                pdm_send_response(message->cond,message->mutex);
            }
		}
		break;

	case PDM_GET_POWER_STATUS:
		{
			pdmGetLoad();
		}
		break;

	default:
		printf("unknown\n");
		break;
	}
}

/*********************************************************
 * NAME: pdmGetFWVersion
 * DESCRIPTION: Get the currently installed firmware
 *              version number
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
void pdmGetFWVersion(void)
{
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	int nRetVal;
	unsigned int pdmBusyReading = 0;

	ALOGD("PDM", "Requesting FW Version from %s (%d)",pdmDevice->szDevPath, pdmDevice->serialPortHandle);
	nRetVal =
	    usbSendCommand(CMD_GET_DEV_FW_VERSION, pdmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		do {
			pdmBusyReading =
			    usbProcessResponse(pdmDevice, pdmDataBfr);
		}
		while (pdmBusyReading);
	} else {
		ALOGE("PDM","Unable to retrieve PDM firmware version. Returned %d",nRetVal);
	}
}

/*********************************************************
 * NAME: pdmGetSerialNum
 * DESCRIPTION: Get the serial number of the device
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void pdmGetSerialNum(void)
{
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	int nRetVal;
	unsigned int pdmBusyReading = 0;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_SERIAL_NUM, pdmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		do {
			pdmBusyReading =
			    usbProcessResponse(pdmDevice, pdmDataBfr);
		}
		while (pdmBusyReading);
	} else {
		ALOGE("PDM","Unable to retrieve PDM serial number. Returned %d", nRetVal);
	}
}

/*********************************************************
 * NAME: getPDMState
 * DESCRIPTION: Get the current state of the device
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void pdmGetState(void)
{
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	int nRetVal;
	unsigned int pdmBusyReading = 0;

	nRetVal =
	    usbSendCommand(CMD_GET_DEV_STATE, pdmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		do {
			pdmBusyReading =
			    usbProcessResponse(pdmDevice, pdmDataBfr);
		}
		while (pdmBusyReading);

		ALOGD("PDM","retrieve PDM state. Returned %d",nRetVal);
	} else {
		ALOGE("PDM","Unable to retrieve PDM state. Returned %d",nRetVal);
	}
}

/*********************************************************
 * NAME: pdmGetLoad
 * DESCRIPTION: Get the current device load
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void pdmGetLoad(void)
{
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	int nRetVal;
	unsigned int pdmBusyReading = 0;

	nRetVal = usbSendCommand(CMD_GET_DEV_LOAD, pdmDevice->serialPortHandle);
	if (nRetVal >= 0) {
		do {
			pdmBusyReading =
			    usbProcessResponse(pdmDevice, pdmDataBfr);
		}
		while (pdmBusyReading);

		if (pdmDevice->board_rev == REV_A) {
			/* This is a two part message...Rev A requires us to read again */
			do {
				pdmBusyReading =
				    usbProcessResponse(pdmDevice, pdmDataBfr);
			}
			while (pdmBusyReading);
		}

		ALOGI("PDM"," Load Voltage(V) = %.2f | Current(mA) = %d", pdmDevice->load_v, pdmDevice->load_i);
	} else {
		ALOGE("PDM","Unable to retrieve PDM load. Returned %d",nRetVal);
	}
}

/*********************************************************
 * NAME: pdmGetPowerPorts
 * DESCRIPTION: Get the current state of all ports
 *
 * INPUT:  None
 * OUTPUT: None
 *
 **********************************************************/
void pdmGetPowerPorts(void)
{
	/*
	 * Note: peculiar behavior reading from the PDM when
	 * attempting to read using a loop. Only the first port
	 * is read. Manually reading each port.
	 */
	pdmGetPowerPort(PDM_SCU_DEVICE);
	pdmGetPowerPort(PDM_KR_DEVICE);
	pdmGetPowerPort(PDM_CCR_DEVICE);
	pdmGetPowerPort(PDM_SCU_EXP_DEVICE);
	pdmGetPowerPort(PDM_PAYPASS_DEVICE);
	pdmGetPowerPort(PDM_BDP1_DEVICE);
	pdmGetPowerPort(PDM_BDP2_DEVICE);
	pdmGetPowerPort(PDM_PRINTER_DEVICE);
}

/*********************************************************
 * NAME: pdmGetPowerPort
 * DESCRIPTION: Get a specified power port state
 *
 * INPUT:  int port - the port to get
 * OUTPUT: None
 *
 **********************************************************/
void pdmGetPowerPort(pdm_power_device_t port)
{
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	unsigned int pdmBusyReading = 0;
	int nRetVal;
	char cmd[1];

	sprintf(cmd, "%d", (int)port);
	nRetVal = usbSendCommand(cmd, pdmDevice->serialPortHandle);
	if (nRetVal < 0) {
		ALOGE("PDM","Unable to set PDM port %d. Returned %d", port, nRetVal);
	}

	/* Read from the device */
	do {
		pdmBusyReading = usbProcessResponse(pdmDevice, pdmDataBfr);
	}
	while (pdmBusyReading);
}

/*********************************************************
 * NAME: pdmSetPowerPort
 * DESCRIPTION: Set a specified power port state
 *
 * INPUT:  int port - the port to set
 *         int state - the state 0 = OFF | 1 = ON
 * OUTPUT: None
 *
 **********************************************************/
void pdmSetPowerPort(pdm_power_device_t port, int state)
{
	int nRetVal;
	char pdmDataBfr[512];

	if (pdmDevice == NULL)
		return;

	char cmd[6];		// format {#=x} | where # is 0-7 and x = 0 or 1
	unsigned int pdmBusyReading = 0;

	sprintf(cmd, "{%d=%d}", (int)port, state);
	nRetVal = usbSendCommand(cmd, pdmDevice->serialPortHandle);
	if (nRetVal < 0) {
		ALOGE("PDM","Unable to set PDM port %d. Returned %d", port, nRetVal);
	}

	do {
		pdmBusyReading = usbProcessResponse(pdmDevice, pdmDataBfr);
	}
	while (pdmBusyReading);

	if (pdmDevice->board_rev == REV_A) {
		/* This is a two part message...Rev A requires us to read again */
		do {
			pdmBusyReading =
			    usbProcessResponse(pdmDevice, pdmDataBfr);
		}
		while (pdmBusyReading);
	}
}
