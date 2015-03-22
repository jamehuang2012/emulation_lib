/**************************************************************************

pm.c

Power Manager Application main processing

***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>

#include "pdm.h"
#include "scm.h"

#include "usbdevice.h"

#include "pmdatadefs.h"
#include "logger.h"
#include "pm.h"
#include "pdmqueue.h"

// Thread active flags
int pdmThreadActive = 0;
int scmThreadActive = 0;
int pmThreadActive = 0;
pthread_t pdm_thread = 0;
pthread_t scm_thread = 0;
pthread_t pm_thread = 0;

pthread_mutex_t pm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t power_mutex = PTHREAD_MUTEX_INITIALIZER;	/* Protects access to value */
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;	/* Protects access to value */

pthread_cond_t pdm_cond = PTHREAD_COND_INITIALIZER;;	/* Signals change to value */
pthread_cond_t power_cond = PTHREAD_COND_INITIALIZER;;	/* Signals change to value */
pthread_cond_t scm_cond = PTHREAD_COND_INITIALIZER;;	/* Signals change to value */
static pthread_once_t init_block = PTHREAD_ONCE_INIT;


static pthread_cond_t   sync_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  sync_mutex = PTHREAD_MUTEX_INITIALIZER;


power_message_t power_message;
//power_message_t resp_message;
struct MessageList pdm_list;

int pdm_activity = 0;		/* 0 not activity , 1 actived */

void *pm_thread_func(void *prt)
{
	int nRetVal;
	int pdmRetVal;
	int scmRetVal;
	void *nResult;
	int i = 0;
	struct USBDevice usbDeviceList[MAX_USB_DEVICES];
	struct MessageEntry *entry;

	/* initialize and create pdm thread */

	pthread_detach(pthread_self());

	// Scan the USB bus for the devices (PDM & SCM)
	
	for(i=0; i < MAX_USB_DEVICES; i++)
		memset(&usbDeviceList[i],0,sizeof(struct USBDevice));
	usbScan(usbDeviceList);

	if (pdmDevice != NULL) {
		// Spawn the PDM thread
		if ((pdmRetVal =
		     pthread_create(&pdm_thread, NULL, pdm_thread_func,
				    (void *)NULL))) {
			ALOGE("PM","Unable to spawn PDM thread.");
		}
	} else {
		ALOGE("PM","PDM device not found. Thread not spawned.");
	}

	if (scmDevice != NULL) {
		// Spawn the SCM thread
		if ((scmRetVal =
		     pthread_create(&scm_thread, NULL, scm_thread_func,
				    (void *)scmDevice))) {
			ALOGE("PM", "Unable to spawn SCM thread.");
		}
	} else {
		ALOGE("PM","SCM device not found. Thread not spawned.");
	}

	ALOGI("PM","Initialization complete. Kicking off main loop");

	pmThreadActive = 1;
	for (; pmThreadActive != 0;) {

		nRetVal = pthread_mutex_lock(&pm_mutex);
		if (nRetVal != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRetVal);
		}

		while (power_message.type == PM_NO_CMD) {
			nRetVal = pthread_cond_wait(&power_cond, &pm_mutex);
			if (nRetVal != 0) {
				ALOGE("PM","pthread_cond_wait err = %d\n",nRetVal);
			}
			break;
		}

		if (power_message.type == PM_EXIT) {
			nRetVal = pthread_mutex_lock(&power_mutex);
			if (nRetVal != 0) {
				fprintf(stderr, "mutex lock err = %d\n",
					nRetVal);
			}

			pdmThreadActive = 0;
			scmThreadActive = 0;

			nRetVal = pthread_cond_signal(&pdm_cond);
			if (nRetVal != 0) {
				fprintf(stderr, "mutex lock err = %d\n",
					nRetVal);
			}
			nRetVal = pthread_cond_signal(&scm_cond);
			if (nRetVal != 0) {
				fprintf(stderr, "mutex lock err = %d\n",
					nRetVal);
			}

			nRetVal = pthread_mutex_unlock(&power_mutex);
			if (nRetVal != 0) {

			}
			nRetVal = pthread_mutex_unlock(&pm_mutex);
			if (nRetVal != 0) {

			}

			break;
		}

		if (power_message.type == PDM_ACTIVITY) {
			pdm_activity = PDM_ACTIVED;
			power_message.type  = PM_NO_CMD;
			nRetVal = pthread_mutex_unlock(&pm_mutex);
			if (nRetVal != 0) {
				fprintf(stderr, "mutex lock err = %d\n",
					nRetVal);
			}

			continue;
		}

		/* if message type : PDM_GET_PORT_STATE,PDM_SET_PORT_STATE,PDM_GET_POWER_STATUS */
		/* put into queue */
		nRetVal = pthread_mutex_lock(&data_mutex);
		entry = pdm_add_list_entry(&pdm_list, &power_message);
		nRetVal = pthread_mutex_unlock(&data_mutex);
		if (entry == NULL) {
			ALOGE("PM",	"pdm_add_list_entry insert failure");
			power_message.type = PM_NO_CMD;
			nRetVal = pthread_mutex_unlock(&pm_mutex);
			if (nRetVal != 0) {
				fprintf(stderr, "mutex lock err = %d\n",
					nRetVal);
			}
			continue;
		}

		nRetVal = pthread_mutex_lock(&power_mutex);
		if (nRetVal != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRetVal);
		}

		nRetVal = pthread_cond_signal(&pdm_cond);
		if (nRetVal != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRetVal);
		}
		nRetVal = pthread_mutex_unlock(&power_mutex);
		if (nRetVal != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRetVal);
		}

		power_message.type = PM_NO_CMD;
		nRetVal = pthread_mutex_unlock(&pm_mutex);
		if (nRetVal != 0) {
			fprintf(stderr, "mutex lock err = %d\n", nRetVal);
		}

	}

	fprintf(stderr, "pm Thread exit\n");
	return 0;
}

/*********************************************************
 * NAME:   pm_init_routinue
 * DESCRIPTION: should only run once ...
 *
 *********************************************************/

void pm_init_routinue(void)
{
	int nRetVal;

	pdm_init_list(&pdm_list);
	if ((nRetVal =
	     pthread_create(&pm_thread, NULL, pm_thread_func, (void *)NULL))) {
		ALOGE("PM","Unable to spawn PDM thread.");
	}
}

/*********************************************************
 * NAME:   initialize
 * DESCRIPTION:
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pm_init(void)
{
	int status;

	status = pthread_once(&init_block, pm_init_routinue);
	if (status != 0) {
		ALOGE("PM","Unable to spawn pm thread.");
	}

	return 0;

}

/*********************************************************
 * NAME:   pm_exit
 * DESCRIPTION:
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pm_exit(void)
{
	int nRetVal;

	nRetVal = pthread_mutex_lock(&pm_mutex);
	power_message.type = PM_EXIT;
	pdmThreadActive = 0;
	pmThreadActive = 0;
	scmThreadActive = 0;

	nRetVal = pthread_cond_signal(&power_cond);
	if (nRetVal != 0) {
		fprintf(stderr, "mutex lock err = %d\n", nRetVal);
	}

	nRetVal = pthread_mutex_unlock(&pm_mutex);

	// Close serial devices
	if (pdmDevice != NULL) {
		if (pdmDevice->serialPortHandle > 0) {
			close(pdmDevice->serialPortHandle);
			pdmDevice->serialPortHandle = -1;
		}
	}

	if (scmDevice != NULL) {

		if (scmDevice->serialPortHandle > 0) {
			close(scmDevice->serialPortHandle);
			scmDevice->serialPortHandle = -1;
		}
	}
	return 0;
}

int pdm_sync_read_signal(pthread_cond_t *cond,pthread_mutex_t *mutex,int timeout_ms)
{
    static int rev_count = 0;
    struct timespec timeout;
    int rc;
    rc = pthread_mutex_lock(mutex);
    if (-1 == rc) {
        ALOGE("PM","sync_read_form_rx pthread_mutex_lock err = %d",rc);
    }

    timeout.tv_sec = time(NULL) + timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000000;

    rc = pthread_cond_timedwait(cond,mutex,&timeout);
    if ( rc == ETIMEDOUT) {

        pthread_mutex_unlock(mutex);
        return rc;
    }
    rc = pthread_mutex_unlock(mutex);
    if (-1 == rc) {
        ALOGE("PM","sync_read_form_rx pthread_mutex_lock err = %d",rc);
    }
    rev_count++;
    ALOGD("PM","get signal from pdm thread count = %d",rev_count);

    return 0;

}

/*********************************************************
 * NAME:   set_pdm_port
 * DESCRIPTION: Set.   0 close , 1 open
 * wait_flag   0 , not wait for response, 1 wait for response
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/

int pdm_set_state(int port, char state,int wait_flag)
{
    static int send_count = 0;
	int status;
	int result = 0;
	status = pthread_mutex_lock(&power_mutex);
	if (status != 0) {
		ALOGE("PM"," mutex lock err = %d",status);
		return -1;
	}

	power_message.type = PDM_SET_PORT_STATE;
	power_message.message.pdm_port_state.port = port;
	power_message.message.pdm_port_state.state = state;

	if (wait_flag == 1) {
        send_count++;
        ALOGD("PM","send sync = %d",send_count);
        power_message.cond = &sync_cond;
        power_message.mutex = &sync_mutex;
	} else {
        power_message.cond = NULL;
        power_message.mutex = NULL;
	}

	pdm_add_list_entry(&pdm_list, &power_message);

	status = pthread_cond_signal(&pdm_cond);
	if (status != 0) {
		fprintf(stderr, "mutex lock err = %d\n", status);
	}

	status = pthread_mutex_unlock(&power_mutex);
	if (status != 0) {
		ALOGE("PM", "mutex unlock err = %d", status);
		return -1;
	}

	// wait the response , sem_timewait
    result = pdm_sync_read_signal(&sync_cond,&sync_mutex,1000);



	return result;
}

/*********************************************************
 * NAME:   get_pdm_port
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/

int pdm_get_state(int port, int  *state)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (pdmDevice != NULL) {
		if (port < PDM_NUM_POWER_PORTS && port >= 0) {
			*state = pdmDevice->powerPortState[port];
		} else {
			result = -1;
		}

	} else {
		result = -2;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	return result;
}

/*********************************************************
 * NAME:   get status
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_get_info(char *fm_ver, char *serial_no)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (pdmDevice != NULL && pdmThreadActive == 1) {
		if (fm_ver != NULL)
			strcpy(fm_ver, pdmDevice->szFWVerNum);
		if (serial_no != NULL)
			strcpy(serial_no, pdmDevice->szSerialNum);

	} else {
		result = -1;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}
	return 0;
}

/*********************************************************
 * NAME:   get firm version
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_get_firmware_version(char *fm_version)
{
	int status;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM"," create mutex lock error = %d", status);
		return status;
	}

	if (pdmDevice != NULL && pdmThreadActive == 1) {
		if (fm_version != NULL)
			strcpy(fm_version, pdmDevice->szFWVerNum);

	} else {
		status = pthread_mutex_unlock(&data_mutex);
		if (status != 0) {
			ALOGE("PM"," create mutex lock error = %d",
				status);
			return status;
		}
		return -1;	/* the pdm device error */
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}
	return 0;
}

/*********************************************************
 * NAME:   get serial number
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_get_serial_number(char *serial_number)
{
	int status;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (pdmDevice != NULL && pdmThreadActive == 1) {

		if (serial_number != NULL)
			strcpy(serial_number, pdmDevice->szSerialNum);

	} else {
		status = pthread_mutex_unlock(&data_mutex);
		if (status != 0) {
			ALOGE("PM"," create mutex lock error = %d",
				status);
			return status;
		}
		return -1;	/* the pdm device error */
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}
	return 0;
}

/*********************************************************
 * NAME:   pdm_set threshold
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_set_threshold(void)
{
	return 0;
}

/*********************************************************
 * NAME:   pdm_get threshold
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_get_threshold(void)
{
	return 0;
}

/*********************************************************
 * NAME:   pdm load
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int pdm_get_load(float *load_v, int *load_i)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM"," create mutex lock error = %d", status);
		return status;
	}

	if (pdmDevice != NULL && pdmThreadActive == 1) {
		if (load_v != NULL)
			*load_v = pdmDevice->load_v;
		if (load_i != NULL)
			*load_i = pdmDevice->load_i;

	} else {
		result = -2;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM"," create mutex lock error = %d", status);
		return status;
	}

	return result;
}

/*********************************************************
 * NAME:   scm get info
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int scm_get_info(char *fm_ver, char *serial_no)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (scmDevice != NULL) {
		if (fm_ver != NULL)
			strcpy(fm_ver, scmDevice->szFWVerNum);
		if (serial_no != NULL)
			strcpy(serial_no, scmDevice->szSerialNum);

	} else {
		result = -1;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM"," create mutex lock error = %d", status);
		return status;
	}

	return result;
}

/*********************************************************
 * NAME:   scm get firmware vdersion
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int scm_get_firmware_version(char *fm_version)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (scmDevice != NULL) {
		if (fm_version != NULL)
			strcpy(fm_version, scmDevice->szFWVerNum);

	} else {
		result = -1;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	return result;
}

/*********************************************************
 * NAME:   scm get serial number
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int scm_get_serial_number(char *serial_no)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (scmDevice != NULL) {

		if (serial_no != NULL)
			strcpy(serial_no, scmDevice->szSerialNum);

	} else {
		result = -1;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	return result;
}

/*********************************************************
 * NAME:   scm get load
 * DESCRIPTION: Set.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int scm_get_load(float *load_v, int *load_i,float *battery_v,int *battery_i)
{
	int status;
	int result = 0;
	status = pthread_mutex_lock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	if (scmDevice != NULL) {
		if (load_v != NULL)
			*load_v = scmDevice->load_v;
		if (load_i != NULL)
			*load_i = scmDevice->load_i;
		if (battery_v)
			*battery_v = scmDevice->battery_v;
		if (battery_i)
			*battery_i = scmDevice->battery_i;
		fprintf(stderr,"load_v =%f load_i =%d bat_v=%f bat_i=%d",
			*load_v,*load_i,*battery_v,*battery_i);
			
		ALOGD("SCM","scm get load ok");
	} else {
		
		ALOGD("SCM","scm get load failure");
		result = -2;
	}

	status = pthread_mutex_unlock(&data_mutex);
	if (status != 0) {
		ALOGE("PM","create mutex lock error = %d", status);
		return status;
	}

	return result;
}
