/*
 * Copyright (C) 2012 MapleLeaf Software, Inc
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
 *   svsDIO.h: FPGA/GPIO processing
 */

#ifndef SVS_DIO_H
#define SVS_DIO_H

#define DOOR_OPEN       0x01
#define SPARE_ACTIVE    0x02
#define TPB_ACTIVE      0x04

typedef enum
{
    dioTypeIdentity,
    dioTypeState,
    dioTypeChange,
   
    dioTypeEnd
} dio_msg_type_t;

typedef struct
{
    unsigned green_led:1;
    unsigned red_led:1;
    unsigned bdp_bus_a:1;
    unsigned bdp_bus_b:1;
    unsigned supply:1;
    unsigned modem:1;
    unsigned wim:1;
    unsigned spare:1;
} dio_power_state_t;

typedef struct
{
    unsigned door:1;    
    unsigned spare:1;    
    unsigned pb:1;
    unsigned reserved:5;
} dio_input_state_t;

typedef struct
{
    dio_msg_type_t type;
} dio_header_t;

typedef struct
{
    dio_header_t hdr;
    dio_power_state_t power;
    dio_input_state_t input;
} dio_state_t;

typedef struct
{
    dio_header_t hdr;
    dio_power_state_t power;
} dio_change_t;

typedef union
{
    dio_header_t control;
    dio_state_t state;
    dio_change_t change;
} dio_message_t;

extern int svsDIOServerInit(void);
extern int svsDIOServerUninit(void);
extern int svsDIOInit(void);
extern void svsDIOUninit(void);
extern int svsDIOGreenLEDSet(uint8_t enable);
extern uint8_t svsDIOGreenLEDGet(void);
extern int svsDIORedLEDSet(uint8_t enable);
extern uint8_t svsDIORedLEDGet(void);
extern int svsDIOBDPBusASet(uint8_t enable);
extern uint8_t svsDIOBDPBusAGet(void);
extern int svsDIOBDPBusBSet(uint8_t enable);
extern uint8_t svsDIOBDPBusBGet(void);
extern int svsDIO5VSet(uint8_t enable);
extern uint8_t svsDIO5VGet(void);
extern int svsDIOModemPowerSet(uint8_t enable);
extern uint8_t svsDIOModemPowerGet(void);
extern int svsDIOWIMPowerSet(uint8_t enable);
extern uint8_t svsDIOWIMPowerGet(void);
extern int svsDIOSparePowerSet(uint8_t enable);
extern uint8_t svsDIOSparePowerGet(void);

#endif /* SVS_DIO_H */
