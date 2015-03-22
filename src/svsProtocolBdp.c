
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
#include <svsProtocolBdp.h>

static void *protocolBdpRxThread(void *arg);
static void *protocolBdpTxThread(void *arg);

static svs_protocol_bdp_info_t *svs_protocol_bdp_info;

int svsProtocolBdpInit(int flagInit)
{
    int rc;
    int devFd;

    // Get the shared info
    svsStateControlGet(offsetof(svsStateMemControl_t, svs_protocol_bdp_info), (void **)&svs_protocol_bdp_info);

    if(flagInit)
    {
        strcpy(svs_protocol_bdp_info->devName[0], "/dev/sTTY0");
        strcpy(svs_protocol_bdp_info->devName[1], "/dev/sTTY0");

        // Open and configure the serial ports
        devFd = svsProtovolBdpPortOpen(svs_protocol_bdp_info->devName[0], svs_protocol_bdp_info->baudRate);
        if(devFd < 0)
        {
            return(rc);
        }
        svs_protocol_bdp_info->devFd[0] = devFd;

        devFd = svsProtovolBdpPortOpen(svs_protocol_bdp_info->devName[1], svs_protocol_bdp_info->baudRate);
        if(devFd < 0)
        {
            close(svs_protocol_bdp_info->devFd[0]);
            return(rc);
        }
        svs_protocol_bdp_info->devFd[1] = devFd;

        // Create the threads
        rc = pthread_create(&svs_protocol_bdp_info->pthreadRx, NULL, protocolBdpRxThread, (void *)svs_protocol_bdp_info);
        if(-1 == rc)
        {
            logError("pthread_create: %s", strerror(errno));
        }
        rc = pthread_create(&svs_protocol_bdp_info->pthreadTx, NULL, protocolBdpTxThread, (void *)svs_protocol_bdp_info);
        if(-1 == rc)
        {
            logError("pthread_create: %s", strerror(errno));
        }
    }

    return(rc);
}

int svsProtocolBdpUnInit(void)
{

    return(0);
}


static void *protocolBdpRxThread(void *arg)
{
    int status;
    uint16_t count;
    uint16_t wireSize;
    uint8_t  *wirePtr;
    uint8_t  *hostPtr;
    svsMsg_t hostMsg;
    rs232Message_t wireMsg;
    svs_protocol_bdp_info *tc = (svs_protocol_bdp_info *)arg;

    while(1)
    {
        wirePtr = (uint8_t *)&wireMsg;

        /*   Read one byte from the serial port until the start of message
         * byte is read.
         */
        do
        {
            count = read(tc->portd, &(wireMsg.startOfMessage),
                         sizeof(wireMsg.startOfMessage));
            if(-1 == count)
            {
                perror("rs232ComponentReceiveThread SOM read() ");
            }
            else if(0 == count)
            {
                fprintf(stderr,
                        "rs232ComponentReceiveThread : EOF on read()");
            }
        } while(*wirePtr != RS232_START_OF_MESSAGE);

        /* Read and discard the spare byte */
        count = read(tc->portd, &(wireMsg.spare), sizeof(wireMsg.spare));
        if(-1 == count)
        {
            perror("rs232ComponentReceiveThread spare read() ");
        }
        else if(0 == count)
        {
            fprintf(stderr,
                    "rs232ComponentReceiveThread : EOF on read()");
        }

        /*   Messsage start detected.  Get the size of the message */
        status = readAllBytes(tc->portd, (uint8_t *)&(wireMsg.messageSize),
                              sizeof(wireMsg.messageSize));
        if(status != sizeof(wireMsg.messageSize))
        {
            /*   If an error occurs there's nothing to be done in the thread */
            fprintf(stderr, "rs232ComponentReceiveThread : ");
            fprintf(stderr, "failed to read mesage size");
            continue;
        }

        /*   Got message size, read the rest of the message */
        wireSize = wireMsg.messageSize - sizeof(wireMsg.startOfMessage);
        wireSize -= (sizeof(wireMsg.messageSize) + sizeof(wireMsg.spare));

        wirePtr = wireMsg.cookedPayload;
        status = readAllBytes(tc->portd, wirePtr, wireSize);
        if(status != wireSize)
        {
            /*   If an error occurs there's nothing to be done in the thread */
            /*   TODO : add robustness */
            fprintf(stderr, "rs232ComponentReceiveThread : ");
            fprintf(stderr, "failed to read entire mesage");
            continue;
        }

        /*  Got the entire message, convert to host format */
        wirePtr = wireMsg.cookedPayload;
        hostPtr = (uint8_t *)&hostMsg;
        for(count = 0;count < wireSize;count++)
        {
            *hostPtr = *wirePtr;
            /*   Check for the start of message byte and verify that
             * it occurs twice in the serial stream.  Discard the
             * second occurance.
             */
            if(RS232_START_OF_MESSAGE == *wirePtr)
            {
                if(RS232_START_OF_MESSAGE != *(wirePtr + 1))
                {
                    fprintf(stderr, "rs232ComponentReceiveThread : ");
                    fprintf(stderr, "missing escape sequence");
                }
                else
                {
                    wirePtr++;
                }
            }

            /* Advance the pointers and the total wire message size */
            wirePtr++;
            hostPtr++;
        }

        /*  Put the converted message onto the message queue */
        hostPtr = (uint8_t *)&hostMsg;
        count = hostMsg.msgHeader.msgSize + sizeof(hostMsg.msgHeader);

        if(-1 == (status = mq_send(tc->rxQ, hostPtr, count, 0)))
        {
            perror("rs232ComponentReceiveThread : mq_send() ");
        }
    } /* end while(1) */
}

static void *protocolBdpTxThread(void *arg)
{
    int rc;
    uint16_t count;
    uint16_t wireSize;
    uint8_t  *wirePtr;
    uint8_t  *hostPtr;
    svsMsg_t hostMsg;
    rs232Message_t wireMsg;
    svs_protocol_bdp_info_t *tc = (svs_protocol_bdp_info_t *)arg;

    while(1)
    {
        hostPtr  = (uint8_t *)&hostMsg;

        logDebug("mq_receive()");
        /*   Block on reading a message off the transmit queue */
        rc = mq_receive(tc->txQ, hostPtr, sizeof(svsMsg_t), NULL);
        if(rc < 0)
        {
            logError("write: %s", strerror(errno));
        }
        else
        {
            count = status;

            wireMsg.startOfMessage = RS232_START_OF_MESSAGE;
            wireMsg.spare = 0;
            wireSize = sizeof(wireMsg.startOfMessage) +
                       sizeof(wireMsg.spare) +
                       sizeof(wireMsg.messageSize);
            wirePtr = (uint8_t *)wireMsg.cookedPayload;
            for(;count > 0;count--)
            {
                *wirePtr = *hostPtr;
                /*   Check for the start of message byte and send it
                 * twice if it's part of the host message payload.
                 */
                if(RS232_START_OF_MESSAGE == *hostPtr)
                {
                    *++wirePtr = *hostPtr;
                    wireSize++;
                }
                /* Advance the pointers and the total wire message size */
                wirePtr++;
                hostPtr++;
                wireSize++;
            }

            wireMsg.messageSize = wireSize;
            /*   Now that the host message is cooked into a wire message
             * write it out.
             */
            count = write(tc->portd, &wireMsg, wireSize);
            if(-1 == count)
            {
                logError("write: %s", strerror(errno));
            }
            else if(count != wireSize)
            {
                logError("incomplete write");
            }
        }
    } /* end while(1) */
}
