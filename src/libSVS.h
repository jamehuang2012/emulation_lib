// --------------------------------------------------------------------------------------
// Module     : libSVS
// Description: Shared vehicle system service provider
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#ifndef LIBSVS_H
#define LIBSVS_H

#include <svsLog.h>
#include <svsErr.h>

typedef enum
{
    SVS_MSG_ID_ECHO,
    SVS_MSG_ID_BDP_AVAILABLE,
    SVS_MSG_ID_LOG_VERBOSITY_SET,
    SVS_MSG_ID_BDP_POWER_GET,

    SVS_MSG_ID_KR_AVAILABLE,
    SVS_MSG_ID_KR_POWER_GET,

    SVS_MSG_ID_BDP_MSN_GET,
    SVS_MSG_ID_BDP_MSN_FLUSH,

} svs_msg_id_t;

typedef struct
{
    uint16_t id;
    int16_t status; // return error code svs_err_code_t
} svsMsgSvsHeader_t;

int svsInit(const char *applicationName);
void svsUninit(void);

int svsCommonInit(const char *applicationName);
void svsCommonUninit(void);

int svsServerInit(const char *appName, const char *logfile);
int svsServerUninit(void);

int svsSvsServerInit(void);

const char *svsAppNameGet(void);
int  svsBdpSockFdGet(void);
void svsBdpSockFdSet(int);
int  svsKrSockFdGet(void);
void svsKrSockFdSet(int);
int  svsWimSockFdGet(void);
int  svsLogSockFdGet(void);
void svsLogSockFdSet(int);
int  svsSvsSockFdGet(void);
void svsSvsSockFdSet(int);
int  svsCallbackSockFdGet(void);

int64_t svsTimeGet_us(void);
int64_t svsTimeGet_ms(void);


#endif // LIBSVS_H
