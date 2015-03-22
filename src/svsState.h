// --------------------------------------------------------------------------------------
// Module     : svsState
// Description: System state module
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#ifndef SVS_STATE_H
#define SVS_STATE_H

#define SHARED_MEMORY_OBJECT_NAME       "/svsState"




typedef struct
{
    uint8_t                 initFlag;   // set when memory has been initialized
    logInfo_t               logInfo;
//    svs_protocol_bdp_info_t svs_protocol_bdp_info;
    pthread_mutex_t         mutex;      // mutex used for atomic access on svsStateMemData_t
} svsStateMemControl_t;

typedef struct
{
    // BDP data
    uint8_t         bdpArpIndexLast;            // updated with every BDP ARP response
    uint32_t        bdpMsgSeqNum;               // incrementing sequence number with every message sent
    bdp_state_t     bdp_state[BDP_MAX];

//    stateBDP_t      BDP[MAX_BDP];
//    uint32_t        scuTemp;
//    uint32_t        auxTemp;
//    stateType_t     doorState; //SCU GPIO - 1=open
//    stateType_t     debugVal;
//    svsStatistics_t statistics;
} svsStateMemData_t;

typedef struct
{
    // Control structure
    // accessed by applications, should be thread safe
    svsStateMemControl_t   c;
    // Data structure (thread safe, accessed only through mutex and one thread at a time
    // NOTE: if access tends to become slow, then we could add mutexes for different areas
    svsStateMemData_t      d;
} svsStateMem_t;

int svsStateInit(void);
void svsStateUninit(void);
int svsStateGet(size_t field_offset, uint16_t size, void *data);
int svsStateSet(size_t field_offset, uint16_t size, void *data);
int svsStateControlGet(size_t field_offset, void **data);
int svsStateDataLockAndGet(svsStateMemData_t **data);
int svsStateDataUnlock(void);

int svsStateIsInit(void);

#endif // SVS_STATE_H
