/*
 * Copyright (C) 2011, 2012 MapleLeaf Software, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _USBDEVICE_H_
#define _USBDEVICE_H_

#define VENDOR_ID	0x0403
#define PRODUCT_ID	0x6001

// Max sizes for character arrays
#define MAX_LEN 128

#define CMD_GET_DEV_MODULE_ID	"i"
#define CMD_GET_DEV_FW_VERSION	"v"
#define CMD_GET_DEV_SERIAL_NUM	"n"
#define CMD_GET_DEV_STATE		"s"
#define CMD_GET_DEV_LOAD		"l"
#define CMD_GET_DEV_POWER		"p"
#define CMD_GET_DEV_BATTERY		"b"
#define CMD_GET_DEV_RUNTIME		"t"

// Universal Response Message Headers
#define DEV_RESPONSE_MOID		"MOID"
#define DEV_RESPONSE_FVER		"FVER"
#define DEV_RESPONSE_SERN		"SERN"

// PDM Response Message Headers
#define PDM_RESPONSE_PDMS		"PDMS"
#define PDM_RESPONSE_LODV		"LODV"
#define PDM_RESPONSE_LODI		"LODI"
#define PDM_RESPONSE_PWR		"PWR"

// SCM Response Message Headers
#define SCM_RESPONSE_TIME		"TIME"
#define SCM_RESPONSE_SCMS		"SCMS"
#define SCM_RESPONSE_PV_V		"PV_V"
#define SCM_RESPONSE_PV_I		"PV_I"
#define SCM_RESPONSE_PV_W		"PV_W"
#define SCM_RESPONSE_BA_V		"BA_V"
#define SCM_RESPONSE_BA_I		"BA_I"
#define SCM_RESPONSE_BA_W		"BA_W"

#define SCM_RESPONSE_PWMC		"PWMC"
#define SCM_RESPONSE_MP_B		"MP_B"
#define SCM_RESPONSE_MP_S		"MP_S"
#define SCM_RESPONSE_BA_S		"BA_S"
#define SCM_RESPONSE_TEMP		"TEMP"
#define SCM_RESPONSE_REAS		"REAS"

#define PDM_NUM_POWER_PORTS		8

#define SERIAL_READ_TIMEOUT_USEC 800000
//#define SERIAL_READ_TIMEOUT_USEC 50000

#define COMPLETE	0
#define BUSY		1

#define BUSY_READING	-1000

// Set number of USB Devices the Power Manager will monitor
#define MAX_USB_DEVICES	2

//========== USB Device structure ==========//
// Enum for module board revisions to support conditional processing
typedef enum {
	REV_A = 0,
	REV_B
} board_rev_t;

struct USBDevice {
	board_rev_t board_rev;

	int serialPortHandle;

	char szDevPath[MAX_LEN];	// Device's Linux USB device path
	char szSerialDevice[MAX_LEN];	// Device's Linux serial device name

	char szMOID[MAX_LEN];	// module id
	char szFWVerNum[MAX_LEN];	// firmware version
	char szSerialNum[MAX_LEN];	// serial number
	char szState[MAX_LEN];	// current state
	char szRunTime[MAX_LEN];	// runtime

	float load_v;		// load in voltage
	int load_i;		// load in current
	int load_w;
	float battery_v;
	int battery_i;
	int battery_w;

	float threshold;

	// SCM State Details
	int pwmc;
	int temperature;
	char szMPB[MAX_LEN];
	char szMPS[MAX_LEN];
	char szBatteryStatus[MAX_LEN];
	int reason;

	int powerPortState[PDM_NUM_POWER_PORTS];
};

extern struct USBDevice *pdmDevice;
extern struct USBDevice *scmDevice;

extern void usbScan(struct USBDevice usbDeviceList[]);
extern int usbSendCommand(const char *Cmd, const int serialPortHandle);
extern int usbRecvResponse(const int serialPortHandle, char *pRecvBfr,
			   int nRecvBfrSize);
extern int usbProcessResponse(struct USBDevice *usbDevice, char *pRecvBfr);
extern void usbSaveResponse(struct USBDevice *usbDevice, char *response);

#endif				// _USBDEVICE_H_
