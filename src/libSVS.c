// --------------------------------------------------------------------------------------
// Module     : libSVS
// Description: Shared vehicle system service provider
// Author     :
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#include <svsErr.h>
#include <svsCommon.h>
#include <svsCallback.h>
#include <svsLog.h>
#include <svsApi.h>
#include <svsSocket.h>
#include <svsBdp.h>
#include <svsCCR.h>
#include <svsWIM.h>
#include <svsDIO.h>
#include <svsConfig.h>
#include <svsUpgrade.h>
#include <svsPM.h>
#include <libSVS.h>

static const char *svsAppName   = 0;
static int svsBdpSockFd         = -1;
static int svsKrSockFd          = -1;
static int svsSvsSockFd         = -1;
static int svsLogSockFd         = -1;
static int svsCallbackSockFd    = -1;

pthread_mutex_t mutexSocketClientBdp;   // used to protect the socket on the client from being accessed by multiple threads
pthread_mutex_t mutexSocketClientKr;    // used to protect the socket on the client from being accessed by multiple threads
pthread_mutex_t mutexSocketClientSvs;   // used to protect the socket on the client from being accessed by multiple threads

pthread_mutex_t mutexSocketRecv;   // used to protect the socket receive from being accessed from multiple threads with different sockFd
pthread_mutex_t mutexSocketSend;   // used to protect the socket send from being accessed from multiple threads with different sockFd

static int svsSocketClientSvsHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);

//
// Called to initialize the client side of SVSD by applications requiring access to all devices
//
int svsInit(const char *appName)
{
    int rc;

    rc = svsCommonInit(appName);
    if (rc != ERR_PASS)
        return(rc);
    rc = svsWIMInit();
    if (rc != ERR_PASS)
        return(rc);
/*
    rc = svsCCRInit();
    if (rc != ERR_PASS)
        return(rc);

    rc = svsDIOInit();
    if (rc != ERR_PASS)
        return(rc);
*/
    rc = svsUpgradeInit();
    if (rc != ERR_PASS)
        return(rc);
/*
    rc = svsPMInit();
    if (rc != ERR_PASS)
       return(rc);
*/
    return(rc);
}

//
// Called to initialize the client side of SVSD by applications requiring access to a common set of devices
//
int svsCommonInit(const char *appName)
{
    int rc;

    svsAppName = appName;

    svsCallbackInit();

    //
    // Create the clients to connect with the servers
    //
    rc = svsSocketClientCreateLog(&svsLogSockFd);
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsSocketClientCreateSvs(&svsSvsSockFd);
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsSocketClientCreateBdp(&svsBdpSockFd);
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsSocketClientCreateKr(&svsKrSockFd);
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    // add robust attribute in case the calling thread dies while inside the critical section
    // this will prevent other processes from being locked, but will generate errors and exit
    // XXX could find a way to recover, but at least other apps will not block
    // see: http://www.embedded-linux.co.uk/tutorial/mutex_mutandis
    pthread_mutexattr_setrobust_np(&mutexAttr, PTHREAD_MUTEX_ROBUST_NP);
    pthread_mutex_init(&mutexSocketRecv, &mutexAttr);
    pthread_mutex_init(&mutexSocketSend, &mutexAttr);
    pthread_mutex_init(&mutexSocketClientBdp, &mutexAttr);
    pthread_mutex_init(&mutexSocketClientKr, &mutexAttr);
    pthread_mutex_init(&mutexSocketClientSvs, &mutexAttr);

    return(rc);
}

void svsCommonUninit(void)
{
    svsSocketClientDestroyKr(svsKrSockFd);
    svsSocketClientDestroyBdp(svsBdpSockFd);
    svsSocketClientDestroySvs(svsSvsSockFd);
    svsSocketClientDestroyLog(svsLogSockFd);
    svsSocketClientDestroyCallback(svsCallbackSockFd);

    pthread_mutex_destroy(&mutexSocketClientKr);
    pthread_mutex_destroy(&mutexSocketClientBdp);
    pthread_mutex_destroy(&mutexSocketClientSvs);

    pthread_mutex_destroy(&mutexSocketRecv);
    pthread_mutex_destroy(&mutexSocketSend);
}

void svsUninit(void)
{
    logDebug("");

    svsUpgradeUninit();
    //svsPMUninit();
    //svsDIOUninit();
    //svsCCRUninit();
    svsWIMUninit();

    svsCommonUninit();
}

//
// Called by the server application to initialize all the server modules
//
int svsServerInit(const char *appName, const char *logfile)
{
    int rc;

    svsAppName = appName;

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutexattr_setrobust_np(&mutexAttr, PTHREAD_MUTEX_ROBUST_NP);

    pthread_mutex_init(&mutexSocketRecv, &mutexAttr);
    pthread_mutex_init(&mutexSocketSend, &mutexAttr);

    // Start CONFIG server
    rc = svsConfigServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    // Start LOG server
    rc = svsLogServerInit(logfile);
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsSvsServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsCallbackServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsBdpServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsKrServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    
    
    rc = svsWIMServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    /*
    rc = svsDIOServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    rc = svsPMServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    */
    
    rc = svsSWUServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
    /*
    rc = svsCCRServerInit();
    if(rc != ERR_PASS)
    {
        return(rc);
    }
     
    */
	
    //logDebug("Done");

    return(rc);
}

//
// Called by the server application to cleanup all server modules
//
int svsServerUninit(void)
{
    int rc;

    rc = svsSWUServerUninit();
    //rc = svsDIOServerUninit();
    //rc = svsCCRServerUninit();
    rc = svsWIMServerUninit();
    rc = svsKrServerUninit();
    rc = svsBdpServerUninit();
    //rc = svsPMServerUninit();
    rc = svsConfigServerUninit();
    rc = svsCallbackServerUninit();

    pthread_mutex_destroy(&mutexSocketRecv);
    pthread_mutex_destroy(&mutexSocketSend);

    return(rc);
}

static socket_thread_info_t socket_thread_info;

int svsSvsServerInit(void)
{
    int rc = ERR_PASS;

    memset(&socket_thread_info, 0, sizeof(socket_thread_info_t));

    // configure server info
    socket_thread_info.name        = "SVS";
    socket_thread_info.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info.port        = SOCKET_PORT_SVS;
    socket_thread_info.devFd1      = -1;
    socket_thread_info.devFd2      = -1;
    socket_thread_info.fnServer    = 0;
    socket_thread_info.fnClient    = svsSocketClientSvsHandler;
    socket_thread_info.fnDev1      = 0;
    socket_thread_info.fnDev2      = 0;
    socket_thread_info.fnThread    = svsSocketServerThread;
    socket_thread_info.len_max     = 1024;

    // create the server
    rc = svsSocketServerCreate(&socket_thread_info);
    if(rc != ERR_PASS)
    {   // XXX
        return(rc);
    }

    return(rc);
}

//
// Description:
// This handler is called when the server receives request from client
//
static int svsSocketClientSvsHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc;
    void *rsp = 0;
    svs_bdp_available_rsp_t         svs_bdp_available_rsp;
    svs_kr_available_rsp_t          svs_kr_available_rsp;
    svs_log_verbosity_set_req_t     *svs_log_verbosity_set_req;
    bdp_power_set_msg_req_t         *bdp_power_set_msg_req;
    bdp_power_get_msg_rsp_t         bdp_power_get_msg_rsp;
    bdp_address_get_msg_req_t       *bdp_address_get_msg_req;
    bdp_address_get_msg_rsp_t       bdp_address_get_msg_rsp;

    hdr->u.svshdr.status = ERR_PASS;

    switch(hdr->u.svshdr.id)
    {
        case SVS_MSG_ID_BDP_AVAILABLE:
            hdr->len = sizeof(svs_bdp_available_rsp);
            svs_bdp_available_rsp.bdp_available = svsBdpMaxGet();
            rsp = &svs_bdp_available_rsp;
            break;

        case SVS_MSG_ID_KR_AVAILABLE:
            hdr->len = sizeof(svs_kr_available_rsp);
            svs_kr_available_rsp.kr_available = svsKrMaxGet();
            rsp = &svs_kr_available_rsp;
            break;

        case SVS_MSG_ID_ECHO:
            rsp = payload;
            break;

        case SVS_MSG_ID_LOG_VERBOSITY_SET:
            hdr->len = 0;
            svs_log_verbosity_set_req = (svs_log_verbosity_set_req_t *) payload;
            logVerbositySet(svs_log_verbosity_set_req->verb);
            break;

        case SVS_MSG_ID_BDP_POWER_GET:
            rsp = &bdp_power_get_msg_rsp;
            // get the local state, in the request field
            rc = svsBdpPowerGetLocal(hdr->dev_num, &bdp_power_set_msg_req);
            if(rc != ERR_PASS)
            {
                hdr->u.svshdr.status = rc;
                break;
            }
            // update the response
            bdp_power_get_msg_rsp.bdp_power_state = bdp_power_set_msg_req->bdp_power_state;
            bdp_power_get_msg_rsp.status = ERR_PASS;

            hdr->len = sizeof(bdp_power_get_msg_rsp_t);
            break;

        case SVS_MSG_ID_BDP_MSN_GET:
            bdp_address_get_msg_req = (bdp_address_get_msg_req_t *)payload;
            rsp = &bdp_address_get_msg_rsp;
            hdr->len = sizeof(bdp_address_get_msg_rsp_t);
            // get the MSN and update the response
            rc = svsBdpVirtualToPhysical(hdr->dev_num, &bdp_address_get_msg_rsp.bdp_addr);
            if(rc != ERR_PASS)
            {
                hdr->u.svshdr.status = rc;
                break;
            }
            bdp_address_get_msg_rsp.status = rc;
            break;

        case SVS_MSG_ID_BDP_MSN_FLUSH:
            hdr->len = 0;
            svsArpTableFlush();
            rc = ERR_PASS;
            hdr->u.svshdr.status = rc;
            break;

        default:
            hdr->len = 0;
            hdr->u.svshdr.status = ERR_INV_MSG_ID;
            break;
    }

    rc = svsSocketSend(sockFd, hdr, rsp);

    return(rc);
}

const char *svsAppNameGet(void)
{
    return(svsAppName);
}

int svsBdpSockFdGet(void)
{
    return(svsBdpSockFd);
}

void svsBdpSockFdSet(int value)
{
    svsBdpSockFd = value;
}

int svsKrSockFdGet(void)
{
    return(svsKrSockFd);
}

void svsKrSockFdSet(int value)
{
    svsKrSockFd = value;
}

int svsLogSockFdGet(void)
{
    return(svsLogSockFd);
}

void svsLogSockFdSet(int value)
{
    svsLogSockFd = value;
}

int svsCallbackSockFdGet(void)
{
    return(svsCallbackSockFd);
}

int svsSvsSockFdGet(void)
{
    return(svsSvsSockFd);
}

void svsSvsSockFdSet(int value)
{
    svsSvsSockFd = value;
}

int64_t svsTimeGet_ms(void)
{   // convert us to ms
    return(svsTimeGet_us() / 1000);
}

int64_t svsTimeGet_us(void)
{
    int rc;
    struct timespec curr_time;
    int64_t time = 0;

    rc = clock_gettime(CLOCK_MONOTONIC, &curr_time);
    if(rc < 0)
    {
        logError("clock_gettime: %s", strerror(errno));
    }
    time = ((int64_t)curr_time.tv_sec * 1e9 + (int64_t)curr_time.tv_nsec) / 1e3; // convert ns to us

    return(time);
}


