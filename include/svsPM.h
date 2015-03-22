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

#ifndef _SVS_PM_H_
#define _SVS_PM_H_

#include <svsErr.h>
#include <pmdatadefs.h>

#define PM_DAEMON_NAME "/usr/scu/bin/powermanager"

extern int pdmFound;
extern int scmFound;

extern int svsPMInit(void);
extern int svsPMUninit(void);

extern int svsPMServerInit(void);
extern int svsPMServerUninit(void);

// PDM API
extern int   svsGetPDMInfo (void);
extern char *svsGetPDMFWVersion (void);
extern char *svsGetPDMSerialNumber (void);
extern int   svsGetPDMLoad (float *load_v, int *load_i);
extern float svsGetPDMPowerThreshold (void);
extern int   svsGetPDMPowerDevicePortState (pdm_power_device_t device);

extern void svsSetPDMPowerThreshold (float threshold);
extern void svsSetPDMPowerDevicePortState (pdm_power_device_t device, int state);

// SCM API
extern int   svsGetSCMInfo (void);
extern char *svsGetSCMFWVersion (void);
extern char *svsGetSCMSerialNumber (void);
extern int   svsGetSCMPowerStatus (float *load_v, int *load_i, float *battery_v, int *battery_i);

#endif // _SVS_UPGRADE_H_
