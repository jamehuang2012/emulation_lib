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

/*
 *   pmdatadefs.h - Data structures for PM related data structures
 *   used for sending and receiving data related to the PM.
 */

#ifndef _PM_DATA_DEFS_
#define _PM_DATA_DEFS_

#define PM_SERVER_IP			"127.0.0.1"
#define PM_SERVER_PORT			3360
#define PM_PDM_SERVER_PORT      3361
#define PM_SCM_SERVER_PORT      3362

#define PDM_SCU_DEV_POWER_PORT		0
#define PDM_KR_DEV_POWER_PORT		1
#define PDM_CCR_DEV_POWER_PORT		2
#define PDM_SCUEXP_DEV_POWER_PORT	3
#define PDM_PAYPASS_DEV_POWER_PORT	4
#define PDM_BDP1_DEV_POWER_PORT		5
#define PDM_BDP2_DEV_POWER_PORT		6
#define PDM_PRINTER_DEV_POWER_PORT	7

#define PDM_POWER_DEV_ON	1
#define PDM_POWER_DEV_OFF	0

#define MAX_LEN	128

extern int svsThreadSocket; // socket to use for callback

typedef enum
{
    PDM_SCU_DEVICE = 0,   // SCU Power
    PDM_KR_DEVICE,        // KR Power
    PDM_CCR_DEVICE,       // CCR Power
    PDM_SCU_EXP_DEVICE,   // SCU EXP Power
    PDM_PAYPASS_DEVICE,   // PayPass Power
    PDM_BDP1_DEVICE,      // BDP Bus #1
    PDM_BDP2_DEVICE,      // BDP Bus #2
    PDM_PRINTER_DEVICE    // Printer Power
} pdm_power_device_t;

typedef enum
{
    PM_EXIT = 0,
    PM_REGISTER_CB_SERVER,
    PM_REGISTER_PDM_THREAD,
    PM_REGISTER_SCM_THREAD,
    PM_LOW_POWER_WARNING,
    PM_DEVICE_READ_ERROR,

    // PDM Message Types
    PDM_GET_INFO,
    PDM_GET_PORT_STATE,
    PDM_SET_PORT_STATE,
    PDM_GET_POWER_THRESHOLD,
    PDM_SET_POWER_THRESHOLD,
    PDM_GET_POWER_STATUS,

    // SCM Message Types
    SCM_GET_INFO,
    SCM_GET_POWER_STATUS,
    SCM_GET_RUNTIME
} pm_message_type_t;

typedef struct
{
    pm_message_type_t type;
    int requestorSocket;            // return socket to the requesting application
    char fw_version[MAX_LEN];
    char serial_number[MAX_LEN];
} pm_info_t;

typedef struct
{
    pm_message_type_t type;
    int requestorSocket;
    pdm_power_device_t port;
    uint8_t state;
} pm_pdm_port_state_t;

typedef struct
{
    pm_message_type_t type;
    int requestorSocket;
    float load_v;   // voltage
    int load_i;     // current
    int load_w;     // power
} pm_pdm_power_status_t;

typedef struct
{
    pm_message_type_t type;
    int requestorSocket;
    float threshold;
} pm_pdm_power_threshold_t;

typedef struct
{
    pm_message_type_t type;
    int requestorSocket;
    float load_v;	// voltage
    int load_i;	    // current
    int load_w;	    // power
    float battery_v;
    int battery_i;
    int battery_w;
} pm_scm_power_status_t;

typedef union
{
    pm_message_type_t type;
    pm_info_t info;
    pm_pdm_port_state_t pdm_port_state;
    pm_pdm_power_status_t pdm_power_status;
    pm_pdm_power_threshold_t pdm_power_threshold;
    pm_scm_power_status_t scm_power_status;
} pm_message_t;

#endif // _PM_DATA_DEFS_
