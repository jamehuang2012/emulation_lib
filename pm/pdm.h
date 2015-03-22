
#ifndef __PDM_THREAD_H__
#define __PDM_THREAD_H__

#include "pmdatadefs.h"
#include "usbdevice.h"
#define DEFAULT_PDM_POLL_TIMER  60
extern struct USBDevice *pdmDevice;

#define DEFAULT_THRESHOLD       11.5	// 11.5V

void *pdm_thread_func(void *);

//void pdmProcessMessage( pm_message_t *message);
void pdmProcessMessage(power_message_t * message);

//========== PDM API ==========//
void pdmGetFWVersion(void);
void pdmGetSerialNum(void);
void pdmGetState(void);
void pdmGetLoad(void);
void pdmGetPowerPorts(void);	// reads all power ports
void pdmGetPowerPort(pdm_power_device_t port);	// reads specified power port
void pdmSetPowerPort(pdm_power_device_t port, int state);

#endif				// __PDM_THREAD_H__
