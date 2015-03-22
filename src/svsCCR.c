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
 *   svsCCR.c Card Manager API Implementation for svs library
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <pthread.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsSocket.h>

#include "svsCCR.h"

#define CARD_MANAGER_DAEMON_NAME "/usr/scu/bin/paymentmanager"

#define ACQUIRE_THREAD_SELECT_TIMEOUT_SECS 1

static int cardManagerCmdSocket = -1;
static int bAcquireCardMode = 0;
static int bAcquireCardModeStopped = 0;
static int bAcquireCardPaused = 0;

static ccr_Info_t gCCRInfo;

static int sendCardManagerCommand(int socketHandle, int cmd, int dataLength, void *commandData);
static int recvCardManagerResult(int *socketHandle, void *resultData, int *resultDataSize);
static void *acquireCardThread(void *arg);
static int recvTCPBuffer(int *socketHandle, void *buffer, int *bytesReceived);

typedef struct _thread_arg
{
    int cbSocketHandle;
    svs_err_t theError;
} thread_arg_t;

svs_err_t ccr_err[] =
{
    CCR_ERROR_STR
};

static svs_err_t ccr_err_unknown = {ERR_UNKNOWN, "UNKNOWN"};

#define ERR_MAX     (sizeof(ccr_err) / sizeof(svs_err_t))

svs_err_t *ccr_errUpdate(ccr_card_manager_errors_t code)
{
    unsigned int i;

    for (i = 0; i < ERR_MAX; i++)
    {
        if (ccr_err[i].code == (err_t)code)
        {
            return(&ccr_err[i]);
        }
    }
    return(&ccr_err_unknown);
}

/*********************************************************
 *   NAME: openTCPConnectionToCardManager
 *   DESCRIPTION: Opens the socket connection to the Card
 *               Manager.
 *   IN: Nothing.
 *   OUT:  Socket handle or negative error code.
 *
 **********************************************************/
static int openTCPConnectionToCardManager(void)
{
    struct sockaddr_in AppToServerAddress;
    int cardManagerSocket = -1;

    /* Create the Pod to Web Server socket */
    cardManagerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (cardManagerSocket < 0)
    {
        logError("[%s] socket create failed\n", __func__);
        return -1;
    }

    memset(&AppToServerAddress, 0, sizeof(AppToServerAddress));
    AppToServerAddress.sin_family = AF_INET;
    AppToServerAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    AppToServerAddress.sin_port = htons(SOCKET_PORT_PAYMENT_MGR);

    if (connect(cardManagerSocket, (struct sockaddr *)&AppToServerAddress, sizeof(AppToServerAddress)) < 0)
    {
        perror("connect to Card Manager Server failed: ");
        return -2;
    }

    return cardManagerSocket;
}

/*********************************************************
 *   NAME: svsCCRInit
 *   DESCRIPTION: Called by svsInit() which is called by
 *                the application at startup.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsCCRInit(void)
{
    int rc;
    svs_err_t *pError;


    ccr_connection_type_t connectionType = ccr_Command;
    int dataLength = sizeof(ccr_Command);

    /* Create client socket and connect to Card Manager */
    cardManagerCmdSocket = openTCPConnectionToCardManager();
    if (cardManagerCmdSocket < 0)
    {
        return ERR_FAIL;
    }

    /* First send Connection Type to server */
    rc = sendCardManagerCommand(cardManagerCmdSocket, CARD_MANAGER_CONN_TYPE, dataLength, &connectionType);
    if (rc < 0)
    {
        return ERR_FAIL;
    }

    /*
    *   Here we will need to power up each card reader (and PIN Pad) device, request the device information,
    *   and then power the devices down again.  We will be caching the device information for future client
    *   requests.  For now we are just getting the information.
    */

    memset(&gCCRInfo, 0, sizeof(gCCRInfo));

    pError = svsCCRGetMagCardReaderInfo();
    if (pError->code != ERR_PASS)
	{
		logError("[%s] get mag card reader info failed\n", __func__);
	}

    pError = svsCCRGetPayPassCardReaderInfo();
    if (pError->code != ERR_PASS)
	{
		logError("[%s] get pay pass reader info failed\n", __func__);
	}

    return ERR_PASS;
}

/*********************************************************
 *   NAME: svsCCRUninit
 *   DESCRIPTION: Called by svsUnInit() which is called by
 *                the application at shutdown.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsCCRUninit(void)
{
    close(cardManagerCmdSocket);
    cardManagerCmdSocket = -1;
    return ERR_PASS;
}

/*********************************************************
 *   NAME: svsCCRServerInit
 *   DESCRIPTION: Called when application starts.  Forks a
 *               process and exec's the Card Manager daemon
 *               Waits 5 seconds to allow Card Manager to
 *               start.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsCCRServerInit(void)
{
    int nRetVal = ERR_PASS;

    pid_t pid;

    static thread_arg_t threadArg;
    pthread_t thread_id;
    pthread_attr_t thread_attr;
    ccr_connection_type_t connectionType = ccr_Command;
    int dataLength = sizeof(ccr_Command);


    /* Fork our cardrdrmgr application */
    pid = fork();
    switch (pid)
    {
        case -1:
            logError("[%s] fork of cardmanager failed\n", __func__);
        break;

        case 0:
        {
            char *prog1_argv[4];
            prog1_argv[0] = CARD_MANAGER_DAEMON_NAME;
            prog1_argv[1] = NULL;
            execvp(prog1_argv[0], prog1_argv);
        }
        break;

        default:
            sleep(5); // Allow time for the card manager to startup
        break;
    }



    /* Create client socket and connect to Card Manager */
    cardManagerCmdSocket = openTCPConnectionToCardManager();
    if (cardManagerCmdSocket < 0)
    {
        return -1;
    }

    // First send Connection Type to server
    nRetVal = sendCardManagerCommand(cardManagerCmdSocket, CARD_MANAGER_CONN_TYPE, dataLength, &connectionType);
    if (nRetVal < 0)
    {
        return -1;
    }

    nRetVal = pthread_attr_init(&thread_attr);
    if (nRetVal)
    {
        return -1;
    }

    nRetVal = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    if (nRetVal)
    {
        return -1;
    }

    nRetVal = pthread_create(&thread_id, &thread_attr, acquireCardThread, &threadArg);
    if (nRetVal)
    {
        return -1;
    }

    return nRetVal;
}

/* Kill off the Card Manager, we are finished with it. */
/*********************************************************
 *   NAME: svsCCRServerUninit
 *   DESCRIPTION: Called on shutdown.  Sends Card Manager
 *               an exit command, waits for return status
 *               from Card Manager.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsCCRServerUninit(void)
{
    ccr_cardMgrResult_t CardMgrResult;
    unsigned char commandData[32];
    int rc;
    int dataSize;

    rc = sendCardManagerCommand(cardManagerCmdSocket, CARD_MANAGER_EXIT, 0, commandData);
    if (rc < 0)
    {
        /* Some kind of error */
        return -1;
    }

    dataSize = sizeof(ccr_cardMgrResult_t);
    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &dataSize);

    /* Kill our Acquire Card processing thread */
    bAcquireCardMode = 0;
    sleep(2); /* Will replace with more efficient handling later. */

    close(cardManagerCmdSocket);
    cardManagerCmdSocket = -1;

    return ERR_PASS;
}

/*********************************************************
 * NAME:   svsReadyPaymentDevices(void)
 * DESCRIPTION: Called to power on the available payment
 *              devices on the system. 
 *
 * IN:  NONE
 * OUT: svs_err_t error / status structure pointer.
 *********************************************************/
svs_err_t *svsReadyPaymentDevices(void)
{
    int rc = ERR_PASS;
    int resultDataSize;
    ccr_cardMgrResult_t CardMgrResult;
    int theCommand;

    /* Allow the thread to run again */
    static svs_err_t theError;

    /* Default to Mag Card for now */
    theCommand = CARD_MANAGER_POWER_READY_CMD;

    /* Send command */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, NULL);
    if (rc < 0)
    {
        logError("[%s] send failed", __func__);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(CardMgrResult);

    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &resultDataSize);
    if (rc)
    {
        logError("[%s] receive failed: rc is %d", __func__, rc);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if(CardMgrResult.error)
    {
        logError("CardMgrResult.error is %d\n", CardMgrResult.error);
        return &ccr_err_unknown;
    }

    /* Devices should now be powered on */
    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsStandbyPaymentDevices(void)
 * DESCRIPTION: Called to power off the available payment
 *              devices on the system. 
 *
 * IN:  NONE
 * OUT: svs_err_t error / status structure pointer.
 *********************************************************/
svs_err_t *svsStandbyPaymentDevices(void)
{
    int rc = ERR_PASS;
    int resultDataSize;
    ccr_cardMgrResult_t CardMgrResult;
    int theCommand;

    /* Allow the thread to run again */
    static svs_err_t theError;

    /* Default to Mag Card for now */
    theCommand = CARD_MANAGER_POWER_STANDBY_CMD;

    /* Send command */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, NULL);
    if (rc < 0)
    {
        logError("[%s] send failed", __func__);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(CardMgrResult);

    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &resultDataSize);
    if (rc)
    {
        logError("[%s] receive failed: rc is %d", __func__, rc);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if(CardMgrResult.error)
    {
        logError("CardMgrResult.error is %d\n", CardMgrResult.error);
        return &ccr_err_unknown;
    }

    /* Devices should now be powered on */
    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRStart()
 * DESCRIPTION: Called to initialize the Card Manager and
 *              Credit Card Reader.  Also sets the callback
 *              function the application uses to process
 *              Magnetic Track data.
 *              Card Manager will also return the Model
 *              Serial and FW version numbers which will
 *              be cached here for the get versions and
 *              model serial number requests.
 *
 * IN:   readerType     - MAG_CARD, CHIP_AND_PIN, PAY_PASS.
 *       timeoutSeconds - Number of seconds to wait for
 *                          Pay Pass card swipe to happen.
 * OUT: svs_err_t error / status structure pointer.
 *********************************************************/
svs_err_t *svsCCRStart(ccr_readerType readerType, int timeoutSeconds)
{
    int rc = ERR_PASS;
    int resultDataSize;
    ccr_cardMgrResult_t CardMgrResult;
    unsigned char commandData[32];
    int theCommand;

    /* Allow the thread to run again */
    static svs_err_t theError;

    bAcquireCardPaused = 0;
    /* Just in case for now. */
    sleep(2);

    memset(commandData, 0, sizeof(commandData));

    /* Default to Mag Card for now */
    theCommand = CARD_MANAGER_INIT_CMD;
    commandData[0] = timeoutSeconds;

    if(readerType == PAY_PASS)
    {
        theCommand = CARD_MANAGER_VIVO_INIT_CMD;
    }
    else if(readerType == CHIP_AND_PIN)
    {
        theCommand = CARD_MANAGER_CHIP_AND_PIN_INIT_CMD;
    }

    /* Send INIT_CMD for requested device */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, commandData);
    if (rc < 0)
    {
        logError("[%s] send failed", __func__);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(CardMgrResult);

    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &resultDataSize);
    if (rc)
    {
        logError("[%s] receive failed: rc is %d", __func__, rc);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if(CardMgrResult.error)
    {
        logError("CardMgrResult.error is %d\n", CardMgrResult.error);
        if (CardMgrResult.status == CARD_MANAGER_DEVICE_NOT_PRESENT)
        {
            logError("[%s] - Requested Device Is Not Present on Kiosk", __func__);
        }
        return &ccr_err_unknown;
    }

    /* Should now be in Acquire Card processing mode. */
    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRStop()
 * DESCRIPTION: Called to place the Card Manager and
 *              Credit Card Reader into standby mode
 *              in preparation for Card Reader power down.
 *
 * IN:   readerType - MAG_CARD, CHIP_AND_PIN, PAY_PASS.
 * OUT: svs_err_t error / status structure pointer.
 *********************************************************/
svs_err_t *svsCCRStop(ccr_readerType readerType)
{
    int rc = ERR_PASS;
    static svs_err_t theError;
    unsigned char commandData[32];
    ccr_cardMgrResult_t CardMgrResult;
    int dataSize;
    int theCommand;

    memset(commandData, 0, sizeof(commandData));

    /* Pause, do NOT kill our Acquire Card processing thread */
    bAcquireCardPaused = 1; // bAcquireCardMode = 0;
    sleep(2); /* Will replace with more efficient handling later. */
    theCommand = CARD_MANAGER_STANDBY_CMD; // Default to Mag Card for now

    if(readerType == PAY_PASS)
    {
        theCommand = CARD_MANAGER_VIVO_STANDBY_CMD;
    }
    else if(readerType == CHIP_AND_PIN)
    {
        theCommand = CARD_MANAGER_CHIP_AND_PIN_STANDBY_CMD;
    }

    /* Send STANDBY_CMD for requested reader type */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, commandData);
    if (rc < 0)
    {
        /* Some kind of error */
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return &theError;
    }

    dataSize = sizeof(ccr_cardMgrResult_t);
    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &dataSize);
    if (rc)
    {
        logError("[%s] back from response to standby cmd. rc is %d", __func__, rc);
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRGetLogs()
 * DESCRIPTION: Called to request that the device logs
 *              are written.
 *
 * IN:   readerType - MAG_CARD, CHIP_AND_PIN, PAY_PASS.
 * OUT: svs_err_t error / status structure pointer.
 *********************************************************/
svs_err_t *svsCCRGetLogs(int deviceType, int clearPerformanceLog)
{

    int rc = ERR_PASS;
    int resultDataSize;
    ccr_cardMgrResult_t CardMgrResult;
    unsigned char commandData[32];
    int theCommand;

    static svs_err_t theError;

    memset(commandData, 0, sizeof(commandData));

    if (clearPerformanceLog == 1)
    {
        theCommand = CARD_MANAGER_GET_LOG_INFO_CLEAR_CMD;
    }
    else
    {
        theCommand = CARD_MANAGER_GET_LOG_INFO_CMD;
    }

    /* MAG_CARD PAY_PASS PINPAD_DEVICE */
    commandData[0] = deviceType;
    commandData[1] = clearPerformanceLog;

    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, commandData);
    if (rc < 0)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(CardMgrResult);

    rc = recvCardManagerResult(&cardManagerCmdSocket, &CardMgrResult, &resultDataSize);
    if (rc)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if(CardMgrResult.error)
    {
#ifdef DEBUG
        printf("CardMgrResult.error is %d\n", CardMgrResult.error);
#endif /* DEBUG */
        return &ccr_err_unknown;
    }

    /* Should have processed the log request and the log files were generated */
    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRFWVersions()
 * DESCRIPTION: Returns the firmware version numbers
 *              in a ccr_VersionNumbers_t data structure
 *              Supervisor, User and EMV2000 engine
 *              firmware versions are returned.
 *
 * IN:   pVersionNumbers pointer to ccr_VersionNumbers_t
 *              data structure that will receive the FW
 *              version numbers for Sankyo reader.
 *
 *       pPayPassVersions pointer to ccr_ViVOPayVersionNumbers_t
 *              data structure that will receive the FW
 *              version numbers for Pay Pass reader.
 *
 * OUT: svs_err_t error / status structure pointer.
 *              On succes the passed in ccr_VersionNumbers_t
 *              data structure will be filled in.
 *********************************************************/
svs_err_t *svsCCRFWVersions(ccr_VersionNumbers_t *pVersionNumbers)
{
    static svs_err_t theError;

    if (pVersionNumbers)
    {
        memcpy(pVersionNumbers, &gCCRInfo.versionNumbers, sizeof(gCCRInfo.versionNumbers));
    }

    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRGetMagCardReaderInfo()
 * DESCRIPTION: Gets the Model and Serial and FW numbers
 *              and places them in the global data structure.
 *
 * IN:   Nothing.
 *
 * OUT: svs_err_t error / status structure pointer.
 *              On succes the passed in ccr_modelSerialNumber_t
 *              data structure will be filled in.
 *********************************************************/
svs_err_t *svsCCRGetMagCardReaderInfo(void)
{
    int rc = ERR_PASS;
    int resultDataSize;
    unsigned char commandData[32];
    int theCommand;

    static svs_err_t theError;

    memset(commandData, 0, sizeof(commandData));

    theCommand = CARD_MANAGER_GET_MAG_READER_INFO;
    /* Send Cmd for requested device */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, commandData);
    if (rc < 0)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(gCCRInfo);
    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &gCCRInfo, &resultDataSize);
    if (rc)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if (gCCRInfo.cardMgrResult.error)
    {
        if(gCCRInfo.cardMgrResult.status == CARD_MANAGER_DEVELOPMENT_READER)
        {
            theError.code = ERR_CCR_DEVELOPMENT_READER;
            theError.str = "INIT FAILED: Development Reader Attached";
        }
        else if(gCCRInfo.cardMgrResult.status == CARD_MANAGER_RELEASE_READER)
        {
            theError.code = ERR_CCR_RELEASE_READER;
            theError.str = "INIT FAILED: Release Reader Attached";
        }
        else
        {
            return &ccr_err_unknown;
        }
        return (&theError);
    }

    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRGetPayPassCardReaderInfo()
 * DESCRIPTION: Gets the Model and Serial and FW numbers
 *              and places them in the global data structure.
 *
 * IN:   Nothing.
 *
 * OUT: svs_err_t error / status structure pointer.
 *              On succes the passed in ccr_modelSerialNumber_t
 *              data structure will be filled in.
 *********************************************************/
svs_err_t *svsCCRGetPayPassCardReaderInfo(void)
{
    int rc = ERR_PASS;
    int resultDataSize;

    unsigned char commandData[32];
    int theCommand;

    static svs_err_t theError;

    memset(commandData, 0, sizeof(commandData));

    theCommand = CARD_MANAGER_GET_PAYPASS_READER_INFO;
    /* Send Cmd for requested device */
    rc = sendCardManagerCommand(cardManagerCmdSocket, theCommand, 0, commandData);
    if (rc < 0)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "SOCKET FAILURE";
        return (&theError);
    }

    resultDataSize = sizeof(gCCRInfo);
    /* recv() with timeout to get status */
    rc = recvCardManagerResult(&cardManagerCmdSocket, &gCCRInfo, &resultDataSize);
    if (rc)
    {
        theError.code = ERR_SOCK_FAIL;
        theError.str = "CCR_ERR_INIT 1";
        return (&theError);
    }

    if (gCCRInfo.cardMgrResult.error)
    {
        if(gCCRInfo.cardMgrResult.status == CARD_MANAGER_DEVELOPMENT_READER)
        {
            theError.code = ERR_CCR_DEVELOPMENT_READER;
            theError.str = "INIT FAILED: Development Reader Attached";
        }
        else if(gCCRInfo.cardMgrResult.status == CARD_MANAGER_RELEASE_READER)
        {
            theError.code = ERR_CCR_RELEASE_READER;
            theError.str = "INIT FAILED: Release Reader Attached";
        }
        else
        {
            return &ccr_err_unknown;
        }
        return (&theError);
    }

    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   svsCCRModelSerial()
 * DESCRIPTION: Returns the Model and Serial numbers
 *              in a ccr_modelSerialNumber_t data structure
 *              The global data structure will be filled in
 *              when each reader type is started the first time
 *              then is cached for future reference.
 *
 * IN:   pModelSerial pointer to ccr_modelSerialNumber_t
 *              data structure that will receive the Model
 *              and Serial numbers.
 *
 * OUT: svs_err_t error / status structure pointer.
 *              On succes the passed in ccr_modelSerialNumber_t
 *              data structure will be filled in.
 *********************************************************/
svs_err_t *svsCCRModelSerial(ccr_modelSerialNumber_t *pModelSerial)
 {
    static svs_err_t theError;

    if (pModelSerial)
    {
        memcpy(pModelSerial, &gCCRInfo.modelSerialNumber, sizeof(gCCRInfo.modelSerialNumber));
    }

    theError.code = ERR_PASS;
    theError.str = "PASS";
    return (&theError);
}

/*********************************************************
 * NAME:   acquireCardThread()
 * DESCRIPTION: Thread function that processes track data
 *              notifications received from the Card
 *              Manager.
 *
 * IN:   arg - Pointer to data structure containing the
 *              socket handle and callback function
 *              pointer.
 * OUT: void pointer to the passed in arg pointer.
 *********************************************************/
static void *acquireCardThread(void *arg)
{
    ccr_connection_type_t connectionType = ccr_Async;
    int dataLength = sizeof(ccr_Async);
    int CardManagerAsyncSocket;
    int cardManagerCBSocket = -1;
    int rc = ERR_PASS;

    bAcquireCardMode = 1;

    CardManagerAsyncSocket = openTCPConnectionToCardManager();
    if (CardManagerAsyncSocket < 0)
    {
        logError("[%s] failed to open Async socket", __func__);
        bAcquireCardMode = 0; // Exit the thread, can't do anything.
        return arg;
    }

    /* Register our socket with the card manager */
    sendCardManagerCommand(CardManagerAsyncSocket, CARD_MANAGER_CONN_TYPE, dataLength, &connectionType );

    /*
     * Create the callback client, this will send the messages
     * to the callback server.
     */
    rc = svsSocketClientCreateCallback(&cardManagerCBSocket, 0);
    if (rc)
    {
        logError("[%s]failed to create callback", __func__);
        bAcquireCardMode = 0; // Exit the thread, can't do anything.
        close(CardManagerAsyncSocket);
        CardManagerAsyncSocket = -1;
        return arg;
    }

    /* While thread is alive process async notifications from Card Manager */
    while (bAcquireCardMode)
    {
        struct timeval selectTimeout;
        fd_set read_fds;
        int status;
        int track_fd;
        int nRetVal;

        if(bAcquireCardPaused)
        {
            sleep(1);
            continue; // Keep waiting for thread to be resumed.
        }

        FD_ZERO(&read_fds);

        if (CardManagerAsyncSocket < 0)
        {
            sleep(1);
            /*
             * Try to reconnect, in case the Card Manager dropped and
             * has restarted.
             */
            CardManagerAsyncSocket = openTCPConnectionToCardManager();
            if (CardManagerAsyncSocket > 0)
            {
                /* Register with the card manager */
                sendCardManagerCommand(CardManagerAsyncSocket, CARD_MANAGER_CONN_TYPE, dataLength, &connectionType );
            }
            continue; // No point, socket is invalid
        }

        track_fd = CardManagerAsyncSocket;
        FD_SET(CardManagerAsyncSocket, &read_fds);

        /* Always reset the timeout value here. */
        selectTimeout.tv_sec = ACQUIRE_THREAD_SELECT_TIMEOUT_SECS;
        selectTimeout.tv_usec = 0;

        if (!bAcquireCardMode)
        {
            continue;  // We are being asked to stop processing so do so before the select()
        }

        /* wait for activity on the file descriptors */
        status = select(track_fd + 1, &read_fds, (fd_set *)0, (fd_set *)0, &selectTimeout);
        if (status < 0)
        {
            nRetVal = errno;
            if (errno == EBADF)
            {
                /*
                 * We got a bad file descriptor error...see which one it is.
                 * Most likely the Web Server went away. Try that one first.
                 */
                if (CardManagerAsyncSocket > 0)
                {
                    nRetVal = fcntl(CardManagerAsyncSocket, F_GETFL);
                    if (nRetVal < 0)
                    {
                        /* File descriptor is bad...pobably disconnected. */
                        close(CardManagerAsyncSocket);
                        CardManagerAsyncSocket = -1;
                    }
                }

            } /* End error EBADF */
            continue;
        } /* End of error case from select() */

        if (status == 0)
        {
            /* select() timeout happened. */
            continue;
        }

        if (FD_ISSET(CardManagerAsyncSocket, &read_fds))
        {
            /*
             * Receive the length of the protocol buffer and then the
             * protocol buffer itself and parse the received buffer.
             */
            ccr_trackData_t theTrackDataMsg;
            int dataSize;

            dataSize = sizeof(ccr_trackData_t);

            nRetVal = recvCardManagerResult(&CardManagerAsyncSocket, &theTrackDataMsg, &dataSize);
            if (nRetVal)
            {
                logError("[%s] AQ Thred recv error\n", __func__);
                continue;
            }

            /*
             * Verify successful read and that we have data for
             * Acquire Card or Reaquire Card command.
             */
            if (theTrackDataMsg.cardMgrResult.command == CARD_MANAGER_ACQUIRE_CARD_CMD ||
               theTrackDataMsg.cardMgrResult.command == CARD_MANAGER_VIVO_INIT_CMD)
            {
                uint16_t dev_num = 0;
                uint16_t msg_id = 0;
                callback_fn_t callback = NULL;
                if (theTrackDataMsg.cardMgrResult.error == 0)
                {
                    rc = svsSocketSendCallback(cardManagerCBSocket, MODULE_ID_CCR, dev_num, msg_id, callback, (uint8_t *)&theTrackDataMsg, sizeof(theTrackDataMsg));
                    if(rc != ERR_PASS)
                    {
                        logError("[%s] svsSocketSendCallback failed", __func__);
                    }
                }
                else
                {
                    theTrackDataMsg.track1Status = ccr_no_data_on_track;
                    theTrackDataMsg.track2Status = ccr_no_data_on_track;
                    theTrackDataMsg.track3Status = ccr_no_data_on_track;

                    rc = svsSocketSendCallback(cardManagerCBSocket, MODULE_ID_CCR, dev_num, msg_id, callback, (uint8_t *)&theTrackDataMsg, sizeof(theTrackDataMsg));
                    if(rc != ERR_PASS)
                    {
                        logError("[%s] svsSocketSendCallback", __func__);
                    }
                }
            } /* We have an Acquire Card related notification */
        }
    } /* While in card acquire mode */

    close(CardManagerAsyncSocket);
    svsSocketClientDestroyCallback(cardManagerCBSocket);
    CardManagerAsyncSocket = -1;
    bAcquireCardModeStopped = 1;
    /* Clean up tread and exit thread function, kills thread */
    return arg;
}

/* Helper functions here, for sending commands and parsing command responses. */

/*********************************************************
*   NAME: sendTCPBuffer
*   DESCRIPTION: Sends the passed in buffer to Kiosk
*                application.
*
*   IN: socketHandle - TCP socket handle.
*       buffer - pointer to buffer to send.
*       bufferSize - size of the buffer.
*   OUT:    SUCCESS (0) or error code.
*
**********************************************************/
static int sendTCPBuffer(
    int socketHandle,
    void *buffer,
    int bufferSize)
{
    if (socketHandle < 0)
    {
#ifdef DEBUG
        printf("[%s] Handle invalid\n", __func__);
#endif /* DEBUG */
        return -1;
    }

    /*
     * Now send the actual protocol buffer, which should include the
     * total size as first number of bytes
     */
    if (send(socketHandle, buffer, bufferSize, MSG_NOSIGNAL) != bufferSize )
    {
        int nRetVal = errno;
#ifdef DEBUG
        printf("[%s] Sent different number of bytes than expected\n", __func__);
        perror("sendTCP error: ");
#endif /* DEBUG */
        return (nRetVal * -1);
    }

    return ERR_PASS;
}

/*********************************************************
*   NAME: recvTCPBuffer
*   DESCRIPTION: Receives the command request from the Kiosk
*                application.
*
*   IN: socketHandle - TCP socket handle.
*       buffer - pointer to buffer to hold command and any
*               data.
*       bytesReceived - pointer to int to receive size of
*               the received data.  Set to the number of
*               bytes to read on entry.
*   OUT:    ERR_PASS (0) or error code.  *bytesReceived
*               is set to the number of bytes read.
*
**********************************************************/
static int recvTCPBuffer(
    int *socketHandle,
    void *buffer,
    int *byteCount )
{
    int bytesRcvd;
    struct timeval tv;

    if (*socketHandle < 0)
    {
#ifdef DEBUG
        printf("[%s] Invalid handle\n", __func__);
#endif /* DEBUG */
        *byteCount = 0;
        return -2;
    }

    tv.tv_sec = 30; // 30 seconds max should be more than enough
    tv.tv_usec = 0;

    setsockopt(*socketHandle, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    /*
     * We will always read the entire ccr_cardMgrResult_t structure
     * Then we will see what kind of response it is and then read
     * the appropriate data structure
     */
    if ((bytesRcvd = recv(*socketHandle, buffer, *byteCount, 0)) < 0)
    {
        while (errno == EINTR)
        {
            // Retry if we got EINTR, if still fails then real error.
            if ((bytesRcvd = recv(*socketHandle, buffer, *byteCount, 0)) <= 0)
            {
                if ((bytesRcvd == 0) || (errno != EINTR))
                    break;
            }
        }
    }

    if (bytesRcvd < 0)
    {
        *byteCount = 0;
        logError("[%s] recv() failed: %s\n", __func__, strerror(errno));
        return -1;
    }
    else if (bytesRcvd == 0)
    {
        /* Remote connection closed...clean up... */
        logError("[%s] recv() failed: remote connection closed\n", __func__);
        close(*socketHandle);
        *socketHandle = -1;
        *byteCount = 0;
        return -2;
    }

    *byteCount = bytesRcvd;

    return 0;
}

/*
*   These two functions are used by the Kiosk application to send commands to the
*   Card Manager, and to receive and parse the result messages from the Reader Manager
*/
/*********************************************************
*   NAME: sendCardManagerCommand
*   DESCRIPTION: Sets up default configuration values.
*                For the case of no configuration file.
*   IN: Nothing.
*   OUT:  Nothing.
*
**********************************************************/
static int sendCardManagerCommand(
    int socketHandle,
    int cmd,
    int dataLength,
    void *commandData)
{
    ccr_cardMgrCmd_t *pCardMgrCmd;
    unsigned char sendBuffer[1024];
    int nRetVal = 0;

    if (socketHandle < 0)
    {
        logError("[%s] Invalid handle. datalength: %d", __func__, dataLength);
        return -1;
    }

    memset(sendBuffer, 0, sizeof(sendBuffer));

    pCardMgrCmd = (ccr_cardMgrCmd_t *)sendBuffer;
    pCardMgrCmd->connType = ccr_Command;
    pCardMgrCmd->protocolVersion = CCR_PROTOCOL_VERSION;

    /*
     * Filter for known commands
     * Also if we need to add special data for future commands
     * that will be handled here.
     */
    switch(cmd)
    {
        case CARD_MANAGER_CANCEL_CMD:
        case CARD_MANAGER_INIT_CMD:
        case CARD_MANAGER_POWER_READY_CMD:
        case CARD_MANAGER_POWER_STANDBY_CMD:
        case CARD_MANAGER_CHIP_AND_PIN_INIT_CMD:
        case CARD_MANAGER_ACQUIRE_CARD_CMD:
        case CARD_MANAGER_STANDBY_CMD:
        case CARD_MANAGER_CHIP_AND_PIN_STANDBY_CMD:
        case CARD_MANAGER_EXIT:
        case CARD_MANAGER_VIVO_CANCEL_CMD:
        case CARD_MANAGER_VIVO_STANDBY_CMD:
        case CARD_MANAGER_GET_MAG_READER_INFO:
        case CARD_MANAGER_GET_PAYPASS_READER_INFO:
            pCardMgrCmd->command = cmd;
        break;

        case CARD_MANAGER_VIVO_INIT_CMD:
            /* Send the timeout delay in seconds. */
            pCardMgrCmd->commandValue = *(ccr_connection_type_t *)commandData;
            pCardMgrCmd->command = cmd;
            break;

        case CARD_MANAGER_CONN_TYPE:
            /* Send type of connection */
            pCardMgrCmd->connType = *(ccr_connection_type_t *)commandData;
            pCardMgrCmd->command = cmd;
        break;

        case CARD_MANAGER_GET_LOG_INFO_CMD:
            /*
            * Send the device type for the logs
            * This will write the log files
            */
            pCardMgrCmd->commandValue = *(unsigned char *)commandData;
            pCardMgrCmd->command = cmd;
        break;

        case CARD_MANAGER_GET_LOG_INFO_CLEAR_CMD:
            /*
            * Send the device type for the logs
            * This will write the log files and then
            * issue the command to reset any logs that
            * can be reset in the context of the specified device
            */
            pCardMgrCmd->commandValue = *(unsigned char *)commandData;
            pCardMgrCmd->command = cmd;
        break;

        default:
            return ERR_PASS;
        break;
    }

    nRetVal = sendTCPBuffer(socketHandle, sendBuffer, sizeof(ccr_cardMgrCmd_t));
    if (nRetVal)
    {
        logError("[%s] - Could not send TCP message.  Socket not open", __func__);

    }

    return nRetVal;
}

/*********************************************************
 *   NAME: recvCardManagerResult
 *   DESCRIPTION: Sets up default configuration values.
 *                For the case of no configuration file.
 *   IN: socketHandle
 *       Pointer to result / payload data structure
 *       Pointer to int to receive bytes read and holds
 *       the total number of bytes to read on entry.
 *   OUT:  SUCCESS (0) or error code.
 *
 **********************************************************/
static int recvCardManagerResult(
    int *socketHandle,
    void *resultData,
    int *resultDataSize)
{
    int nRetVal = ERR_PASS;
    int byteCount = 0;

    ccr_cardMgrResult_t *pCardManagerResult = resultData;

    if (*socketHandle < 0)
    {
        return -1;
    }

    /*
     * Read the data into the passed in resultData buffer,
     * This should be the entire structure for the request
     * Each command response structure includes a result
     * structure as the first member.
     * So we cast a pointer to a result struct so we can
     * do some validation if needed and get the command
     * that the response is associated with so we can do
     * any special processing before returning.
     */

    memset(resultData, 0, sizeof(ccr_cardMgrResult_t));

    byteCount = *resultDataSize;
    nRetVal = recvTCPBuffer(socketHandle, pCardManagerResult, &byteCount);
    if (nRetVal)
    {
        printf("[%s] Failed to receive card manager response\n", __func__);
        return nRetVal;
    }

    if (byteCount == 0)
    {
        // To do probably is error of unknown cause if we get here
        // Do we need to do anything to recover?
    }

    switch (pCardManagerResult->command)
    {
        case CARD_MANAGER_CANCEL_CMD:
        case CARD_MANAGER_ACQUIRE_CARD_CMD:
        case CARD_MANAGER_INIT_CMD:
        case CARD_MANAGER_POWER_READY_CMD:
        case CARD_MANAGER_POWER_STANDBY_CMD:
        case CARD_MANAGER_CHIP_AND_PIN_INIT_CMD:
        case CARD_MANAGER_CHIP_AND_PIN_STANDBY_CMD:
        case CARD_MANAGER_EXIT:
        case CARD_MANAGER_GET_LOG_INFO_CMD:
        case CARD_MANAGER_GET_PIN_CMD:
        case CARD_MANAGER_CANCEL_PIN_CMD:
            /*
             * Don't need to do anything for these commands
             * Fall though to default for now.
             */
        default:
        break;
    }

    return 0;
}
