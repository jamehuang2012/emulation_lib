#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <libudev.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>

#include "usbdevice.h"
#include "logger.h"

struct USBDevice *pdmDevice;
struct USBDevice *scmDevice;

static void usbGetTTYNode(struct USBDevice *usbDevice);
static int usbAddTTYDevice(struct USBDevice *usbDevice);
static char *usbGetModuleID(struct USBDevice *usbDevice);

extern pthread_mutex_t data_mutex;	/* Protects access to value */

/* due to PDM , SCM offten return error result
 * sometimes it returns the result like below:
 * PDiM , SCiMa SiCMa etc ...
 * so , try to remove the command 'i'
 * */
void getModuleName(char *scr, char *target)
{
	int i, j;
	for (i = 0, j = 0; scr[i] != 0x00; i++) {
		if ((scr[i] >= 'a' && scr[i] <='z') || ( scr[i] >='A' && scr[i] <='Z')) {
			if (scr[i] == 'i')
				continue;
			target[j++] = scr[i];
		}
	}
	target[j] = 0x00;
}

/*********************************************************
 * NAME: usbScan
 * DESCRIPTION: Called by the the USB device manager
 * 				application.
 *
 * IN:	vendor_id - vendor code
 * 		product_id - product code
 * OUT:	pointer to the USB device created
 *
 **********************************************************/
void usbScan(struct USBDevice usbDeviceList[])
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;

	char *pWork;
	char *pEnd;
	char szWorkBuffer[256];
	char szUSBDevPath[256];
	int nRetVal;
	int i;
	int count = 0;
	int devicesFound = 0;	// number of valid USB devices found

	// Create the udev object
	udev = udev_new();
	if (!udev) {
		ALOGE("PM","Can't create udev");
		return;
	}
	// Create a list of the devices in the 'tty' subsystem
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;

		// Get the filename of the /sys entry for the device
		// and create a udev_device object (dev) representing it
		path = udev_list_entry_get_name(dev_list_entry);

		// Path has our DEVPATH string
		dev = udev_device_new_from_syspath(udev, path);

		dev =
		    udev_device_get_parent_with_subsystem_devtype(dev, "usb",
								  "usb_device");
		if (!dev) {
			// Not a USB device continue.
			++count;
			continue;
		}
		// Check to see if this device is one that we are looking for
		// Make sure we are still under our max devices
		if ((strtoul
		     (udev_device_get_sysattr_value(dev, "idVendor"), &pEnd,
		      16) == VENDOR_ID
		     || strtoul(udev_device_get_sysattr_value(dev, "idProduct"),
				&pEnd, 16) == PRODUCT_ID)
		    && (devicesFound < MAX_USB_DEVICES)) {
			ALOGI("PM",
				"found device matching vendor and product IDs");

			// We need to get the device node
			strcpy(szWorkBuffer, path);
			ALOGD("PM", "path = %s",  path);

			// Find the 3rd '/' from the end and terminate string there.
			for (i = 0; i < 3; i++) {
				pWork = strrchr(szWorkBuffer, '/');
				if (pWork) {
					*pWork = 0;	// Truncate
				}
			}

			// Now find where "/devices" starts in the string.
			pWork = strstr(szWorkBuffer, "/devices");
			if (pWork) {
				// Finally have the udev USB add Device Path
				strcpy(szUSBDevPath, pWork);
				ALOGD("PM", "szUSBDevPath = %s", szUSBDevPath);
			} else {
				ALOGD("PM","Error parsing %s for USB Device Path",
					szWorkBuffer);
				udev_device_unref(dev);
				continue;
			}

			// Have the Device Path that we will need for adding and processing
			strcpy(usbDeviceList[devicesFound].szDevPath,
			       udev_device_get_devpath(dev));

			ALOGI("PM","usbDeviceList[%d].szDevPath = %s",
				 devicesFound,usbDeviceList[devicesFound].szDevPath);

			// Start saving off the information, we're good to go
			// Init to unopened value
			usbDeviceList[devicesFound].serialPortHandle = -1;

			++devicesFound;
		}

		udev_device_unref(dev);
	}			// End of foreach device found.

	ALOGD("PM","%d devices found matching vendor information. Processing...", count);

	// Now we need to go through our devices and figure out what they are
	for (i = 0; i < devicesFound; i++) {
		char module_id[128];
		char tmp_id[128];

		memset(module_id, 0, 128);
		memset(tmp_id, 0, 128);

		// Get our tty node
		usbGetTTYNode(&usbDeviceList[i]);

		// Add the serial device
		nRetVal = usbAddTTYDevice(&usbDeviceList[i]);
		if (nRetVal < 0) {
			fprintf(stderr,"PM fail %d\n",nRetVal);
			ALOGE("PM","usbAddTTYDevice failed. Error: %d",	nRetVal);

			break;
		}

		sprintf(tmp_id, "%s", usbGetModuleID(&usbDeviceList[i]));
		ALOGD("PM", "tmp id = %s",tmp_id);
		getModuleName(tmp_id, module_id);
		ALOGD("PM", "module id = %s",module_id);

		if (module_id[0] == 0x00) {
			sprintf(module_id, "%s",
				usbGetModuleID(&usbDeviceList[i]));
		}

		ALOGI("PM", "Found '%s' device", module_id);
		if (strstr(module_id, "PDM") != NULL) {
			ALOGD("PM","Assigning '%s' device to list index %d", module_id, i);
			pdmDevice = &usbDeviceList[i];

			if (strstr(module_id, "b") != NULL) {
				// Rev B board
				pdmDevice->board_rev = REV_B;
			} else {
				// Assume it's a Rev A board
				pdmDevice->board_rev = REV_A;
			}
		} else if (strstr(module_id, "SCM") != NULL) {
			ALOGD("PM","Assigning '%s' device to list index %d",module_id, i);
			scmDevice = &usbDeviceList[i];

			if (strstr(module_id, "b") != NULL) {
				// Rev B board
				scmDevice->board_rev = REV_B;
			} else {
				// Assume it's a Rev A board
				scmDevice->board_rev = REV_A;
			}
		} else {
			close(usbDeviceList[i].serialPortHandle);
			ALOGD("PM","close usb device %s %d",module_id, i);
		}
	}

	// Cleanup memory
	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	// failure, we didn't find the PDM
	if (pdmDevice == NULL) {
		ALOGE("PM","Error occured in processing. Device not configured",
			count);
	} else if (scmDevice == NULL) {
		ALOGE("PM","Error occured in SCM processing. Device not configured",count);
	}
}

/*********************************************************
 * NAME:   getDiskNode
 * DESCRIPTION: Scan for the disk device associated with
 *              the currently discovered device.
 * IN:  pszDeviceName pointer to buffer to receive the disk
 *                  device name if found.
 *      pszUSBDevPath pointer to the Device Path that is
 *                  used in the Pod Entry.
 * OUT: 0 on success or error code.
 *********************************************************/
static void usbGetTTYNode(struct USBDevice *usbDevice)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	char *pWork;
	int i;

	// Create the udev object
	udev = udev_new();
	if (!udev) {
		ALOGE("PM", "Can't create udev object.");
		return;
	}
	// Create a list of the devices in the 'tty' subsystem
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		const char *phyDevPath;
		char szWorkBuffer[256];

		// Get the filename of the /sys entry for the device
		// and create a udev_device object (dev) representing it
		path = udev_list_entry_get_name(dev_list_entry);

		//if (strcmp(path, "/sys/class/tty/ttyUSB0") != 0)
		//      continue;

		// Path has our DEVPATH string
		dev = udev_device_new_from_syspath(udev, path);

		phyDevPath = udev_device_get_property_value(dev, "PHYSDEVPATH");
		if (phyDevPath == NULL) {
			// This isn't our device
			continue;
		}
		// This is a tty device. First let's compare the path to see
		// if it matches our usb device.
		strcpy(szWorkBuffer,
		       udev_device_get_property_value(dev, "PHYSDEVPATH"));

		// Find the 3rd '/' from the end and terminate string there
		for (i = 0; i < 2; i++) {
			pWork = strrchr(szWorkBuffer, '/');
			if (pWork) {
				*pWork = 0;	// Truncate
			}
		}

		if (strcmp(szWorkBuffer, usbDevice->szDevPath) == 0) {
			// Found tty device
			sprintf(usbDevice->szSerialDevice, "%s",
				udev_device_get_devnode(dev));

			udev_device_unref(dev);

			ALOGD("PM","Found tty device %s",usbDevice->szSerialDevice);

			break;
		} else {
			// Not our device
			continue;
		}
	}

	// Free the enumerator object
	udev_enumerate_unref(enumerate);

	udev_unref(udev);
	return;
}

/*********************************************************
 * NAME: usbAddTTYDevice
 * DESCRIPTION: Updates the previously deiscovered USB
 *              device with the USB serial device
 *              name.
 *
 * IN:
 * OUT:	SUCCESS (0) or error code
 *
 **********************************************************/
static int usbAddTTYDevice(struct USBDevice *usbDevice)
{

	if (!usbDevice) {
			
		ALOGE("PM","PDM Device not found by devpath %s", usbDevice->szSerialDevice);
		return -1;
	}
	// Try Closing the current serial device if it's already opened
	if (usbDevice->serialPortHandle > 0) {
		// Serial device is already added, log and ignore
		ALOGI("PM", "Unexpected tty add event.");

		// Close the serial connection
		ALOGI("PM","Closing PDM. Current serial device: %s.",usbDevice->szSerialDevice);
		close(usbDevice->serialPortHandle);
	}

	ALOGI("PM", "Opening Serial Device: %s", usbDevice->szSerialDevice);

	// Here we need to open a serial connection to theDevice.szDevNode
	struct termios tio;

	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_cflag = CS8 | CREAD | CLOCAL;
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 128;

	// Make sure number of bits set
	tio.c_cflag &= ~CSIZE;
	tio.c_cflag |= CS8;

	// Make sure 8N1 is set
	tio.c_cflag &= ~PARENB;
	tio.c_cflag &= ~CSTOPB;
	tio.c_cflag &= ~CSIZE;
	tio.c_cflag |= CS8;

	tio.c_cflag &= ~CRTSCTS;	// No HW handshake

	tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);	// Raw input
	tio.c_oflag &= ~OPOST;	// Raw output
	usbDevice->serialPortHandle =
	    open(usbDevice->szSerialDevice, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (usbDevice->serialPortHandle > 0) {
		int n;

		// Cancel the O_NDELAY flag
		n = fcntl(usbDevice->serialPortHandle, F_GETFL, 0);
		fcntl(usbDevice->serialPortHandle, F_SETFL, n & ~O_NDELAY);

		// Make sure O_NONBLOCKING is set
		fcntl(usbDevice->serialPortHandle, F_SETFL, n & O_NONBLOCK);

		cfsetospeed(&tio, B4800);	// Baud of 4800
		cfsetispeed(&tio, B4800);	// Baud of 4800
		tcsetattr(usbDevice->serialPortHandle, TCSANOW, &tio);
		ALOGI("PM","Serial Device %s Opened and configured. (%d)",usbDevice->szSerialDevice,
			usbDevice->serialPortHandle);
	} else {
		ALOGE("PM","Failed to open Device Node: |%s| Reason: %s",
			usbDevice->szSerialDevice, strerror(errno));
		return 1;
	}

	return 0;
}

/*********************************************************
 * NAME: usbSendCommand
 * DESCRIPTION: Send command to Device. Process return status
 *              or value.
 *
 * IN: Cmd pointer to command to send.
 *     serialPortHandle fd/handle of the serial device.
 *     pBuffer Pointer to buffer to receive result.
 *     nBufSize Size of buffer.
 * OUT:  Number of bytes read or negative error code.
 *
 **********************************************************/
int usbSendCommand(const char *Cmd, const int serialPortHandle)
{
	int nBytesWritten;
	int nBytesToWrite;
	int nRetVal;

	if (!Cmd || serialPortHandle < 0) {
		if (!Cmd) {
			ALOGE("PM","Cmd is NULL");
		}

		if (serialPortHandle < 0) {
			ALOGE("PM" ,"Serial handle is invalid");
		}

		return -1;
	}

	tcflush(serialPortHandle, TCIOFLUSH);
	nBytesToWrite = strlen(Cmd);
	nBytesWritten = write(serialPortHandle, Cmd, nBytesToWrite);
	nRetVal = errno;
	if (nBytesWritten < 0) {
		ALOGE("PM","erial Write Error %s", strerror(nRetVal));
	}

	if (nBytesWritten != nBytesToWrite) {
		if (nBytesWritten < 0) {
			ALOGE("PM","Returning error from write: errno is %d",nRetVal);

			// No sense in attempting to read
			return -1;
		}

		ALOGE("PM","Only wrote %d of %d command bytes",nBytesWritten, nBytesToWrite);
		return -1;
	}

	ALOGD("PM", "%d bytes written",  nBytesWritten);

	return 0;
}

/*********************************************************
 * NAME: usbGetModuleID
 * DESCRIPTION: Get the module ID
 *
 * INPUT:	None
 * OUTPUT:	None
 *
 **********************************************************/
static char *usbGetModuleID(struct USBDevice *usbDevice)
{
	int nRetVal;
	unsigned int busyReading = 0;
	char dataBfr[256];

	if (usbDevice != NULL) {
		nRetVal =
		    usbSendCommand(CMD_GET_DEV_MODULE_ID,
				   usbDevice->serialPortHandle);

		if (nRetVal >= 0) {
			// Wait for the response over serial
			ALOGD("PM", "usbGetModuleID ...");
			do {
				busyReading =
				    usbProcessResponse(usbDevice, dataBfr);
			}
			while (busyReading);

			return usbDevice->szMOID;
		} else {
			ALOGE("PM","Unable to retrieve PDM firmware version. Returned %d", nRetVal);

			return NULL;
		}
	}

	return NULL;
}

/*********************************************************
 * NAME: usbSerialTimeoutCheck
 * DESCRIPTION: Checks to see if we've passed our timeout
 *
 * INPUT:	NONE
 * OUTPUT:	Returns TRUE (1) if our timeout has expired
 *
 **********************************************************/
int usbSerialTimeoutCheck(struct timeval end_time, struct timeval start_time)
{
	struct timeval diff;
	long long msec;

	diff.tv_sec = end_time.tv_sec - start_time.tv_sec;
	diff.tv_usec = end_time.tv_usec - start_time.tv_usec;

	while (diff.tv_usec < 0) {
		diff.tv_usec += 1000000;
		diff.tv_sec -= 1;
	}

	msec = 1000000LL * diff.tv_sec + diff.tv_usec;

	if (msec > SERIAL_READ_TIMEOUT_USEC) {
		return 1;	// we've hit our timeout
	}

	return 0;
}

/*********************************************************
 * NAME: usbRecvResponse
 * DESCRIPTION: Receives data from the serial bus
 *
 * IN:	NONE
 * OUT:	Number of bytes read
 *
 **********************************************************/
#define MAX_SERIAL_RESPONSE_SIZE 128
int usbRecvResponse(const int serialPortHandle, char *pRecvBfr, int nRecvBfrSize)
{
	int nbytes = 0;
	int res;
	char ibfr[MAX_SERIAL_RESPONSE_SIZE];
	struct timeval serialTimeout;	// serial comm timeout
	struct timeval currentTime;
	fd_set readfs;		/* file descriptor set */
	int maxfd;

	while (1) {
		// Timeout every 500ms

		serialTimeout.tv_usec = SERIAL_READ_TIMEOUT_USEC;
		serialTimeout.tv_sec = 0;	/* seconds */

		FD_ZERO(&readfs);
		maxfd = serialPortHandle + 1;
		FD_SET(serialPortHandle, &readfs);	/* set testing for source 1 */
		//      fprintf(stderr,"maxfd = %d serial fs = %d\n",maxfd,serialPortHandle);
		res = select(maxfd, &readfs, NULL, NULL, &serialTimeout);

		if (res == 0) {
			/* time out */
			//      fprintf(stderr,"time out ... \n");
			return COMPLETE;
		}
		//      fprintf(stderr,"select return %d\n",res);
		memset(ibfr, 0, strlen(ibfr));	// Make sure the buffer is clean
		nbytes = read(serialPortHandle, ibfr, MAX_SERIAL_RESPONSE_SIZE);
		if (nbytes <= 0) {
			continue;
		} else {
			// Valid character
			strcat(pRecvBfr, ibfr);

			if (strstr(ibfr, "}") != NULL) {
				// Have our delimiter
				break;
			}
		}
	}

	

	return nbytes;
}

/*********************************************************
 * NAME: usbProcessResponse
 * DESCRIPTION: Will send a command to the device and
 * 				retrieve and parse the response
 *
 * INPUT:	pDevice pointer to device
 * OUTPUT:	Returns SUCCESS (0) if processing is complete
 * 			otherwise returns BUSY (1)
 *
 **********************************************************/
int usbProcessResponse(struct USBDevice *usbDevice, char *pRecvBfr)
{
	char *pData;
	int nRetVal;

	nRetVal =
	    usbRecvResponse(usbDevice->serialPortHandle, pRecvBfr,
			    strlen(pRecvBfr) - 1);

	//fprintf(stderr,"%s",pRecvBfr);
	if (nRetVal == BUSY_READING) {
		// Still data there to read... block writes
		return BUSY;
	} else if (nRetVal == COMPLETE) {
		// Nothing to read
		return COMPLETE;
	} else if (nRetVal < 0 || strstr(pRecvBfr, "Error")) {
		ALOGE("PM","Receive buffer = %s",pRecvBfr);

		memset(pRecvBfr, 0, strlen(pRecvBfr));

		return COMPLETE;
	}
	// Check to see if it's a complete message
	if (strstr(pRecvBfr, "}")) {
		// There is data we can parse...
		// Retrieve the data between { and }

		pData = strtok(pRecvBfr, "{}");
		if (pData == NULL) {
			ALOGE("PM", "Unable to parse buffer");
		}

		while (pData != NULL) {
			// If the data contains a '=' character this is my data
			if (strstr(pData, "=") != NULL) {

				nRetVal = pthread_mutex_lock(&data_mutex);
				if (nRetVal != 0) {
					ALOGE("PM",	"mutex unlock err = %d\n",nRetVal);
				}

				usbSaveResponse(usbDevice, pData);

				nRetVal = pthread_mutex_unlock(&data_mutex);
				if (nRetVal != 0) {
					ALOGE("PM","mutex unlock err = %d",	nRetVal);
				}
			} else if (strstr(pRecvBfr, "ACK")) {
				// End of message
				ALOGD("PM","ACK response");

				if (usbDevice->board_rev == REV_B) {
					return COMPLETE;
				}
			} else if (strstr(pRecvBfr, "NACK")) {
				ALOGD("PM","NACK response");
			}

			pData = strtok(NULL, "{}");
		}

		// Clear the input data bfr should be empty anyhow
		memset(pRecvBfr, 0, strlen(pRecvBfr));
	}

	if (usbDevice->board_rev == REV_A) {
		return COMPLETE;
	}
	// Otherwise assume we're working on Rev B and return busy
	return BUSY;
}

void filter_invalid_char(char *in,char *out)
{
	int i,j;
	for (i = 0,j= 0; in[i] != 0x00; i++) {
		if ((in[i] >= 'a' && in[i] <='z') ||( in[i] >='A' && in[i] <='Z')
			|| in[i] == '_' || (in[i] <= '9' && in[i] >= '0')) {
			out[j++] = in[i];
		}

	}
	out[j] = 0x00;
}

/*********************************************************
 * NAME: usbSaveResponse
 * DESCRIPTION:
 *
 * IN:	pDevice pointer to device
 * OUT: SUCCESS (0) or error code.  On success Device Entry
 *      is updated with Firmware Version.
 *
 **********************************************************/
void usbSaveResponse(struct USBDevice *usbDevice, char *response)
{
	char *pHead;
	char *pData;
	int i;
	pHead = strtok(response, "=");
	filter_invalid_char(pHead,pHead);

	if (pHead != NULL) {
		pData = strtok(NULL, "=");	// Get the other side of the token

		if (pData == NULL) {
			
			return;	// We were not able to process the data
		} else if (strcmp(pHead, DEV_RESPONSE_MOID) == 0 ) {
			strcpy(usbDevice->szMOID, pData);
			ALOGD("PM","<< %s", usbDevice->szMOID);
		} else if (strcmp(pHead, DEV_RESPONSE_FVER) == 0) {
			strcpy(usbDevice->szFWVerNum, pData);
			ALOGD("PM","<< %s",	usbDevice->szFWVerNum);
		} else if (strcmp(pHead, DEV_RESPONSE_SERN) == 0) {
			strcpy(usbDevice->szSerialNum, pData);
			ALOGD("PM","<< %s",usbDevice->szSerialNum);
		}
		// PDM Responses
		else if (strcmp(pHead, PDM_RESPONSE_PDMS) == 0) {
			strcpy(usbDevice->szState, pData);
			ALOGD("PM","<< %s",	usbDevice->szState);
		} else if (strcmp(pHead, PDM_RESPONSE_LODV) == 0) {
			// see if the length of the data received is under 5
			for (i = 0; i < strlen(pData); i++) {
				if (!isdigit(pData[i])) {
					ALOGE("PM", "%s :%s", pData,
						response);
					return;
				}
			}
			if (strlen(pData) < 5) {
				// if so, identify the MSB. If this is under 3 there was
				// probably an error in the firmware

				ALOGE("PM", "%s :%s", pData, response);
				if (pData[0] > 51 && strlen(pData) == 4)	// 51 is 3V
				{
					// process normally, the voltage is just < 10V
					usbDevice->load_v = atof(pData) / 1000;
				} else {
					// the board didn't send us a byte
					//              usbDevice->load_v = atof(pData) / 100;
					return;
				}
			} else {
				// we received a normal 5 byte length message
				usbDevice->load_v = atof(pData) / 1000;
			}
			ALOGD("PM","<< Voltage %.2f",usbDevice->load_v);
		} else if (strcmp(pHead, PDM_RESPONSE_LODI) == 0) {
			usbDevice->load_i = atoi(pData);
			ALOGD("PM","<< Voltage %.2f",usbDevice->load_i);
		} else if (strstr(pHead, PDM_RESPONSE_PWR) != NULL) {
			// This is a power port, get the number
			int port = atoi(pHead + 3);	// format PWR# so our digit is pHead + 3

			usbDevice->powerPortState[port] = atoi(pData);
			ALOGD("PM", "<< Port %d | State %d", port,
				usbDevice->powerPortState[port]);
		}
		// SCM Responses
		else if (strcmp(pHead, SCM_RESPONSE_SCMS) == 0) {
			// Clear data from previous state
			memset(usbDevice->szState, 0,
			       strlen(usbDevice->szState));

			strcpy(usbDevice->szState, pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_TIME) == 0) {
			strcpy(usbDevice->szRunTime, pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_PV_V) == 0) {
			// see if the length of the data received is under 5
			if (strlen(pData) < 5) {
				// if so, identify the MSB. If this is under 3 there was
				// probably an error in the firmware
				if (pData[0] > 51)	// 51 is 3V
				{
					// process normally, the voltage is just < 10V
					usbDevice->load_v = atof(pData) / 1000;
				} else {
					// the board didn't send us a byte
					usbDevice->load_v = atof(pData) / 100;
				}
			} else {
				// we received a normal 5 byte length message
				usbDevice->load_v = atof(pData) / 1000;
			}
			
		} else if (strcmp(pHead, SCM_RESPONSE_PV_I) == 0) {
			usbDevice->load_i = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_PV_W) == 0) {
			usbDevice->load_w = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_BA_V) == 0) {
			// see if the length of the data received is under 5
			if (strlen(pData) < 5) {
				// if so, identify the MSB. If this is under 3 there was
				// probably an error in the firmware
				if (pData[0] > 51)	// 51 is 3V
				{
					// process normally, the voltage is just < 10V
					usbDevice->battery_v =
					    atof(pData) / 1000;
				} else {
					// the board didn't send us a byte
					usbDevice->battery_v =
					    atof(pData) / 100;
				}
			} else {
				// we received a normal 5 byte length message
				usbDevice->battery_v = atof(pData) / 1000;
			}
			
		} else if (strcmp(pHead, SCM_RESPONSE_BA_I) == 0) {
			usbDevice->battery_i = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_BA_W) == 0) {
			usbDevice->battery_w = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_PWMC) == 0) {
			usbDevice->pwmc = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_MP_B) == 0) {
			strcpy(usbDevice->szMPB, pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_MP_S) == 0) {
			strcpy(usbDevice->szMPS, pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_BA_S) == 0) {
			strcpy(usbDevice->szBatteryStatus, pData);
		
		} else if (strcmp(pHead, SCM_RESPONSE_TEMP) == 0) {
			usbDevice->temperature = atoi(pData);
			
		} else if (strcmp(pHead, SCM_RESPONSE_REAS) == 0) {
			usbDevice->reason = atoi(pData);
			
		} else if (strstr(pData,"SCM") != NULL){
			strcpy(usbDevice->szMOID, pData);
		//	PDEBUG_LOG("other ");
		//	PDEBUG_LOG(pData);

		} else if (pHead[0] >= '0' && pHead[0]<='7') {
            /* auto update port status */
            int port = atoi(pHead);	// format PWR# so our digit is pHead + 3
            usbDevice->powerPortState[port] = atoi(pData);
			ALOGD("PM", "pHead = %s << Port %d | State %d",pHead, port,
				usbDevice->powerPortState[port]);
		}
	}
}
