
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <mqueue.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsApi.h>
#include <svsState.h>
#include <libSVS.h>
#include <svsMsg.h>

static void *msgHandlerThread(void *arg);
static int svsMsgBdpSend(int device_num, bdp_msg_id_t msgID, uint8_t *payload, uint16_t len, uint32_t *seq_num);
static void addMsToTimespec(int32_t ms, struct timespec *ts);

static svsAddr_t    scuAddr;
static pthread_t    msgHandlerID = 0;
static mqd_t        bdpPidQ;
static mqd_t        bdpTxQ;
static char         bdpPidQName[32];

// For debugging queues: http://linux.die.net/man/7/mq_overview

int svsMsgInit(int flagInit)
{
    int rc;
#if 0
    // define the SCU addresson the BDP bus
    memset(&scuAddr, 0, sizeof(scuAddr));

    // Create the PIDQ, this will be application dependent
    // once a message is sent from the application, the response will be sent back to the same application using this PIDQ
    sprintf(bdpPidQName, "/svsBdpPidQ%d", getpid());
    bdpPidQ = mq_open(bdpPidQName, O_RDONLY | O_CREAT, 0777, NULL);
    if(bdpPidQ == (mqd_t)-1)
    {
        logError("mq_open: %s %s", bdpPidQName, strerror(errno));
        return(-1);
    }
    // Create the BDPQ
    bdpTxQ = mq_open(MSG_Q_BDP_TX, O_WRONLY | O_CREAT, 0777, NULL);
    if(bdpTxQ == (mqd_t)-1)
    {
        logError("mq_open: %s", strerror(errno));
        return(-1);
    }
    // change the attribute to non blocking
    struct mq_attr attr;
    rc = mq_getattr(bdpTxQ, &attr);
    if(rc < 0)
    {
        logError("mq_getattr: %s", strerror(errno));
        return(-1);
    }
    attr.mq_flags |= O_NONBLOCK;
    rc = mq_setattr(bdpTxQ, &attr, NULL);
    if(rc < 0)
    {
        logError("mq_setattr: %s", strerror(errno));
        return(-1);
    }

    if(flagInit == 0)
    {   // Initialize the message handler thread
        // this thread will have one instance for all applications
        rc = pthread_create(&msgHandlerID, NULL, msgHandlerThread, NULL);
        if(-1 == rc)
        {
            logError("pthread_create: %s", strerror(errno));
        }
        else
        {
            logDebug("msgHandlerThread created");
        }

        // Each message ID will have two data types req and rsp
        // the req and rsp will always be copied into the STATE memory
        svsStateMemData_t *svsStateMemData;
        svsStateDataLockAndGet(&svsStateMemData);

        int i, j;

        for(i=0;i<BDP_MAX;i++)
        {
            for(j=0;j<BDP_MSG_ID_MAX;j++)
            {
                svsStateMemData->bdp_state[i].id_data_addr[j].req = 0;
                svsStateMemData->bdp_state[i].id_data_addr[j].rsp = 0;
            }
        }

        for(i=0;i<BDP_MAX;i++)
        {   // populate the req/rsp structure addresses for all message IDs

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_RED_LED_SET].req = &svsStateMemData->bdp_state[i].bdp_led_red_set_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_RED_LED_SET].rsp = &svsStateMemData->bdp_state[i].bdp_led_red_set_msg_rsp;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_GRN_LED_SET].req = &svsStateMemData->bdp_state[i].bdp_led_grn_set_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_GRN_LED_SET].rsp = &svsStateMemData->bdp_state[i].bdp_led_grn_set_msg_rsp;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_YLW_LED_SET].req = &svsStateMemData->bdp_state[i].bdp_led_ylw_set_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_YLW_LED_SET].rsp = &svsStateMemData->bdp_state[i].bdp_led_ylw_set_msg_rsp;

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_DOCK_GET].req = &svsStateMemData->bdp_state[i].bdp_dock_get_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_DOCK_GET].rsp = &svsStateMemData->bdp_state[i].bdp_dock_get_msg_rsp;

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_SN_SET].req = &svsStateMemData->bdp_state[i].bdp_sn_set_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_SN_SET].rsp = &svsStateMemData->bdp_state[i].bdp_sn_set_rsp;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_SN_GET].req = &svsStateMemData->bdp_state[i].bdp_sn_get_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_SN_GET].rsp = &svsStateMemData->bdp_state[i].bdp_sn_get_rsp;

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_POWER_SET].req = &svsStateMemData->bdp_state[i].bdp_power_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_POWER_SET].rsp = &svsStateMemData->bdp_state[i].bdp_power_msg_rsp;

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_BIKE_LOCK_SET].req = &svsStateMemData->bdp_state[i].bdp_bike_lock_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_BIKE_LOCK_SET].rsp = &svsStateMemData->bdp_state[i].bdp_bike_lock_msg_rsp;

            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_BUZZER_SET].req = &svsStateMemData->bdp_state[i].bdp_buzzer_set_msg_req;
            svsStateMemData->bdp_state[i].id_data_addr[BDP_MSG_ID_BUZZER_SET].rsp = &svsStateMemData->bdp_state[i].bdp_buzzer_set_msg_rsp;

        }
        svsStateDataUnlock();

    }

    //svsProtocolBdpInit(flagInit);
#endif
    return(0);
}

int svsMsgUninit(void)
{
    int rc;

    logDebug("");

    rc = mq_close(bdpPidQ);
    if(rc < 0)
    {
        logError("mq_close: %s", strerror(errno));
    }

    return(rc);
}

// Called when the whole libsvs needs to be stopped
int svsMsgUninitAll(void)
{
    int rc;

    logDebug("");

    //svsProtocolBdpUnInit();

    // Cleanup the application related data
    svsMsgUninit();

    rc = mq_unlink(bdpPidQName);
    if(rc < 0)
    {
        logError("mq_unlink: %s %s", bdpPidQName, strerror(errno));
    }
    rc = mq_unlink(MSG_Q_BDP_TX);
    if(rc < 0)
    {
        logError("mq_unlink: %s %s", MSG_Q_BDP_TX, strerror(errno));
    }

    // Cleanup libSVS
    // kill the handler thread
    logDebug("");
    rc = pthread_cancel(msgHandlerID);
    logDebug("");
    if(rc != 0)
    {
        logError("pthread_cancel: %s", strerror(errno));
    }
    else
    {
        logDebug("");
        rc = pthread_join(msgHandlerID, NULL);
        logDebug("");
        if(rc < 0)
        {
            logError("pthread_join: %s", strerror(errno));
        }
    }
    logDebug("");

    return(rc);

}

int svsMsgBdpProcess(int bdp_num, bdp_msg_id_t msgID, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len, int timeout_ms)
{
#if 0
    int                 rc;
    uint32_t            seq_num = 0;
    svsMsg_t            response;
    ssize_t             msgSize;
    struct timespec     timeout;
    svsStateMemData_t   *svsStateMemData;

    // Compute timeout
    clock_gettime(CLOCK_REALTIME, &timeout);
    if(timeout_ms <= TIMEOUT_INFINITE)
    {   // for a blocking call, set a large timeout, this will avoid blocking forever
        timeout_ms = TIMEOUT_MAX;
    }
    addMsToTimespec(timeout_ms, &timeout);

    // Critical section start XXX ADD MUTEX/SEMAPHORE to make it thread safe

    // Update request state
    svsStateDataLockAndGet(&svsStateMemData);
    if(svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].req != 0)
    {
        memcpy(svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].req, req_payload, req_len);
    }
    else
    {
        logError("req not initlized");
    }
    svsStateDataUnlock();

    // Send message (put it on the BDP TX Q), keep track of sequence number used with the message
    rc = svsMsgBdpSend(bdp_num, msgID, req_payload, req_len, &seq_num);
    if(rc < 0)
    {
        logError("svsMsgBdpSend");
        goto _svsMsgBdpProcess;
    }

    // Wait for notify (response from the BDP RX Q sent on PIDQ, based on timeout)
    msgSize = mq_timedreceive(bdpPidQ, (char *)&response, sizeof(svsMsg_t), NULL, &timeout);
    if(msgSize < 0)
    {
        if(errno != ETIMEDOUT)
        {
            rc = -1;
            logError("mq_timedreceive: %s", strerror(errno));
            goto _svsMsgBdpProcess;
        }
        else
        {
            if(timeout_ms != TIMEOUT_IMMEDIATE)
            {
                rc = ERR_TIMEOUT;
            }
        }
    }

    //
    // At this point the response should have been updated
    //

    // Verify if the message response had the same sequence number
    // if not then that was for another request (BDP too slow), log a warning
    if(response.msgHeader.msgCnt != seq_num)
    {
        rc = -1;
        logError("sequence number mismatch");
        goto _svsMsgBdpProcess;
    }

    // Verify if same message ID, if not then log a warning
    if(response.msgHeader.msgID != msgID)
    {
        rc = -1;
        logError("msgID mismatch");
        goto _svsMsgBdpProcess;
    }

    // Update the response field
    // in case we have a non-blocking call, we update with what is already in the response field
    svsStateDataLockAndGet(&svsStateMemData);
    if(svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].rsp != 0)
    {
        memcpy(rsp_payload, svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].rsp, rsp_len);
    }
    else
    {
        logError("rsp not initialized");
    }
    svsStateDataUnlock();

    _svsMsgBdpProcess:

    // Critical section end
    return(rc);
#endif
}


#if 0
/*   Helper function to display a svsMsg in ASCII format */
void svsMsgDump(FILE *stream, svsMsg_t *msg)
{
#ifdef API_DEVELOPMENT

    if((NULL != stream) && (NULL != msg))
    {
        int i;

        /* Emit msgFrom */
        for(i=0;i < sizeof(svsAddr_t)-1;i++)
        {
            fprintf(stream, "%02x:", msg->msgHeader.msgFrom.octet[i]);
        }
        fprintf(stream, "%02x -> ", msg->msgHeader.msgFrom.octet[i]);

        /* Emit msgTo */
        for(i=0;i < sizeof(svsAddr_t)-1;i++)
        {
            fprintf(stream, "%02x:", msg->msgHeader.msgTo.octet[i]);
        }
        fprintf(stream, "%02x : ", msg->msgHeader.msgTo.octet[i]);

        /* Emit msgID */
        fprintf(stream, "%08x : ", msg->msgHeader.msgID);

        /* Emit msgKey */
        fprintf(stream, "%08x : ", msg->msgHeader.msgKey);

        /* Emit msgCnt */
        fprintf(stream, "%02x : ", msg->msgHeader.msgCnt);

        /* Emit msgSize */
        fprintf(stream, "%04x :", msg->msgHeader.msgSize);


        for(i=0;i<msg->msgHeader.msgSize;i++)
        {
            fprintf(stream, " %02x", msg->payload[i]);
        }
        fprintf(stream, "\n");
    }
#endif /* API_DEVELOPMENT */
} /* end svsMsgDump() */
#endif

/*
int svsMsgSend(device_type_t device_type, int device_num, uint32_t msgID, uint8_t *payload, uint16_t len)
{
    int rc = 0;

    switch(device_type)
    {
        case DEVICE_TYPE_BDP:
            rc = svsMsgBdpSend(device_num, msgID, payload, len);
            break;

        default:
            logError("invalid device type");
            rc = -1;
            break;
    }

    return rc;
}
*/

// Send message to the BDP
static int svsMsgBdpSend(int device_num, bdp_msg_id_t msgID, uint8_t *payload, uint16_t len, uint32_t *seq_num)
{
#if 0
    int rc;
    svsMsg_t txMsg;
    svsAddr_t bdpAddr;

    if(len >= BDP_MSG_PAYLOAD_MAX)
    {
        logError("payload too large");
        return(-1);
    }
    // Get the actual BDP address
    rc = svsBdpAddrFromIndexGet(device_num, &bdpAddr);
    if(rc !=0)
    {
        logWarning("bdp %d not found", device_num);
        return(rc);
    }

    //memset(&txMsg, 0, sizeof(svsMsg_t));

    txMsg.msgHeader.msgCnt = svsMsgBdpSeqNumGet();
    txMsg.msgHeader.msgKey = getpid();
    memcpy(&txMsg.msgHeader.msgFrom, &scuAddr, sizeof(svsAddr_t));
    memcpy(&txMsg.msgHeader.msgTo, &bdpAddr, sizeof(svsAddr_t));
    memcpy(txMsg.payload, payload, len);
    txMsg.msgHeader.msgSize = len;
    txMsg.msgHeader.msgID   = msgID;

    *seq_num = txMsg.msgHeader.msgCnt;

    rc = mq_send(bdpTxQ, (char *)&txMsg, sizeof(txMsg.msgHeader) + len, 0);
    if(rc < 0)
    {
        logError("mq_send: %s", strerror(errno));
    }

    return(rc);
#endif
}

uint32_t svsMsgBdpSeqNumGet(void)
{
    uint32_t seq;
    svsStateMemData_t *svsStateMemData;

    svsStateDataLockAndGet(&svsStateMemData);

    seq = svsStateMemData->bdpMsgSeqNum++;

    svsStateDataUnlock();

    return(seq);
}

int svsMsgBdpRxHdlr(svsMsg_t *msg, int len)
{
#if 0
    int                 rc;
    int                 bdp_num;
    struct timespec     timeout;
    svsStateMemData_t   *svsStateMemData;

    // Check to see if that was an ARP request
    if(msg->msgHeader.msgID == BDP_MSG_ID_ADDR_GET)
    {
        rc = svsMsgBdpArpTableUpdate(&msg->msgHeader.msgFrom);
        if(rc < 0)
        {
            logError("ARP table not updated");
            return(rc);
        }
    }

    // Get the BDP number from the BDP address
    rc = svsMsgBdpNumGet(&msg->msgHeader.msgFrom, &bdp_num);
    if(rc < 0)
    {
        logError("BDP number not found in ARP table");
        return(rc);
    }

    uint8_t msgID = msg->msgHeader.msgID;

    // Update response state
    svsStateDataLockAndGet(&svsStateMemData);
    if(svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].rsp != 0)
    {
        memcpy(svsStateMemData->bdp_state[bdp_num].id_data_addr[msgID].rsp, msg->payload, len);
    }
    else
    {
        logError("rsp not initialized");
    }
    svsStateDataUnlock();

    clock_gettime(CLOCK_REALTIME, &timeout);
    addMsToTimespec(100, &timeout);

    // Notify waiting PID Q, by putting the message on the queue
    // there is a possibility that the application is no longer available and thus its PIDQ
    // we just ignore it
    rc = mq_timedsend(bdpPidQ, (char *)msg, len, 0, &timeout);
    if(rc < 0)
    {   // log an error but ignore if PIDQ no longer exists
        logError("mq_send: %s", strerror(errno));
    }

    // Notify callback Q XXX

    return(rc);
#endif
}

static void *msgHandlerThread(void *arg)
{
    int         rc;
    int         messageSize;
    mqd_t       bdpRxQ;
//    mqd_t       scmRxQ;
    svsMsg_t    msg;

    logDebug("thread started");

    // Create RX queue for all devices
    bdpRxQ = mq_open(MSG_Q_BDP_RX, O_RDONLY | O_CREAT, 0777, NULL);
    //scmRxQ = mq_open(MSG_Q_SCM_RX, O_RDONLY| O_CREAT, 0777, NULL);
    if(bdpRxQ == (mqd_t)-1)
    {
        logError("mq_open: %s", strerror(errno));
        exit(1);
    }

    while(1)
    {
        // XXX change this so we wait on a semaphore when using multiple queues
        // then make queues non blocking

        rc = mq_receive(bdpRxQ, (char *)&msg, sizeof(msg), NULL);
        if(rc < 0)
        {
            logError("mq_receive: %s", strerror(errno));
        }
        else
        {   // Call the message handler
            messageSize = rc;
            svsMsgBdpRxHdlr(&msg, messageSize);
        }
    }
}

/*   Helper routine to add millisecond delays to timespec structs */
void addMsToTimespec(int32_t ms, struct timespec *ts)
{
    long work;

    /*   Carve off any whole seconds */
    work = ms/1000;
    ts->tv_sec += work;
    work = ms - (work*1000);

    /*   Adjust the delay to nanoseconds and add it to the timespec */
    ts->tv_nsec += (work * 1000000);

    /*   "Rationalize" the timespec */
    work = ts->tv_nsec/1000000000; /* extract whole seconds */

    ts->tv_sec += work;
    ts->tv_nsec -= work*1000000000;

} /* end addMsToTimespec() */


