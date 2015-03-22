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
 *   svsUpgrade.c Upgrade Manager API Implementation for svs library
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

#include <xmlparser.h>
#include <svsUpgrade.h>

#include <upgradedatadefs.h>
#include "ports.h"

#define SWU_MANAGER_DAEMON_NAME "/usr/scu/bin/upgrademanager"

int upgradeManagerCmdSocket = -1;

svs_err_t swu_err[] =
{
    UPGRADE_ERROR_STR
};

static svs_err_t swu_err_unknown = {ERR_UNKNOWN, "UNKNOWN"};

#define ERR_MAX     (sizeof(swu_err) / sizeof(svs_err_t))

svs_err_t *swu_errUpdate(SwUpdateError_t code)
{
    int i;

    for (i = 0; i < ERR_MAX; i++)
    {
        if (swu_err[i].code == code)
        {
            return(&swu_err[i]);
        }
    }
    return(&swu_err_unknown);
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
    int *byteCount)
{
    int bytesRcvd;
    int nRetVal = 0;
    struct timeval tv;

    if (*socketHandle < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Invalid handle\n", __func__);
#endif
        *byteCount = 0;
        return -2;
    }

    tv.tv_sec = 30; // 30 seconds max should be more than enough
    tv.tv_usec = 0;

    setsockopt(*socketHandle, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    /*
    *   We will always read the entire ccr_cardMgrResult_t structure
    *   Then we will see what kind of response it is and then read
    *   the appropriate data structure
    */
    if ((bytesRcvd = recv(*socketHandle, buffer, *byteCount, 0)) <= 0)
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
#ifdef DEBUG
        printf("[%s] recv() failed: %s\n", __func__, strerror(errno));
#endif
        return -1;
    }
    else if (bytesRcvd == 0)
    {
        /* Remote connection closed...clean up... */
#ifdef DEBUG
        printf("[%s] recv() failed: remote connection closed\n", __func__);
#endif
        close(*socketHandle);
        *socketHandle = -1;
        *byteCount = 0;
        return -2;
    }

    *byteCount = bytesRcvd;

    return nRetVal;
}

/*********************************************************
 *   NAME: recvUpgradeManagerResult
 *   DESCRIPTION: Receive the result and data.
 *   IN: socketHandle
 *       Pointer to result / payload data structure
 *       Pointer to int to receive bytes read and holds
 *       the total number of bytes to read on entry.
 *   OUT:  ERR_PASS (0) or error code.
 *
 **********************************************************/
static int recvUpgradeManagerResult(
    int *socketHandle,
    void *resultData,
    int *resultDataSize)
{
    int nRetVal = ERR_PASS;
    int byteCount = 0;

    swu_upgradeMgrResult_t *pUpgradeManagerResult = resultData;

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

    memset(resultData, 0, sizeof(swu_upgradeMgrResult_t));

    byteCount = *resultDataSize;
    nRetVal = recvTCPBuffer(socketHandle, pUpgradeManagerResult, &byteCount);
    if (nRetVal)
    {
        printf("[%s] Failed to receive upgrade manager response\n", __func__);

        return nRetVal;
    }

    if (byteCount == 0)
    {
        // To do probably is error of unknown cause if we get here
        // Do we need to do anything to recover?
    }

    switch (pUpgradeManagerResult->command)
    {
        case UPGRADE_MANAGER_CANCEL_CMD:
        case UPGRADE_MANAGER_EXIT:
            /*
             * Don't need to do anything for these commands
             * Fall though to default for now.
             */
        default:
        break;
    }

    return ERR_PASS;
}

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
#ifdef PRINT_MSGS
        printf("[%s] Handle invalid\n", __func__);
#endif
        return -1;
    }

    /*
     * Now send the actual protocol buffer, which should include the
     * total size as first number of bytes
     */
    if (send(socketHandle, buffer, bufferSize, MSG_NOSIGNAL) != bufferSize )
    {
#ifdef PRINT_MSGS
        printf("[%s] Sent different number of bytes than expected\n", __func__);
#endif
        int nRetVal = errno;
        perror("sendTCP error: ");
        return nRetVal * -1;
    }

    return ERR_PASS;
}

/*********************************************************
*   NAME: sendUpgradeManagerCommand
*   DESCRIPTION: Sends command to Upgrade Manager.
*   IN: socketHandle - socket handle
*       cmd - Command to send
*       dataLength - Size of data to send
*       commandData - Pointer to data to be sent.
*   OUT:  SUCCESS (0) or error code.
*
**********************************************************/
static int sendUpgradeManagerCommand(
    int socketHandle,
    int cmd,
    int dataLength,
    void *commandData)
{
    swu_upgradeMgrCmd_t *pUpgradeMgrCmd;
    swu_upgradeOTACmd_t *pOTACmd;
    unsigned char sendBuffer[1024];
    int nRetVal = 0;
    int commandSize;

    if (socketHandle < 0)
    {
        printf("[%s] Invalid handle\n", __func__);
        return -1;
    }

    memset(sendBuffer, 0, sizeof(sendBuffer));

    pOTACmd = (swu_upgradeOTACmd_t *)sendBuffer;
    pUpgradeMgrCmd = (swu_upgradeMgrCmd_t *)sendBuffer;
    pUpgradeMgrCmd->connType = swu_Command;
    pUpgradeMgrCmd->protocolVersion = UPGRADE_PROTOCOL_VERSION;

    commandSize = sizeof(swu_upgradeMgrCmd_t);
    /*
     * Filter for known commands
     * Also if we need to add special data for future commands
     * that will be handled here.
     */
    switch(cmd)
    {
        case UPGRADE_MANAGER_CANCEL_CMD:
            pUpgradeMgrCmd->command = cmd;
        break;

        case UPGRADE_MANAGER_OTA_UPGRADE_CMD:
            pOTACmd->swu_command.command = cmd;
            strcpy(pOTACmd->upgradePkgPathname, (char *)commandData);
            commandSize = sizeof(swu_upgradeOTACmd_t);
        break;

        case UPGRADE_MANAGER_EXIT:
            pUpgradeMgrCmd->command = cmd;
        break;

        case UPGRADE_MANAGER_CONN_TYPE:
            pUpgradeMgrCmd->connType = *(swu_connection_type_t *)commandData;
            pUpgradeMgrCmd->command = cmd;
        break;

        default:
            return ERR_PASS;
        break;
    }

    nRetVal = sendTCPBuffer(socketHandle, sendBuffer, commandSize);
    if (nRetVal)
    {
        // TODO: Add test for error code returned here and close socket if appropriate

    }

    return nRetVal;
}

/*********************************************************
 *   NAME: openTCPConnectionToUpgradeManager
 *   DESCRIPTION: Opens the socket connection to the Upgrade
 *               Manager.
 *   IN:    Nothing.
 *   OUT:   Socket handle or negative error code.
 *
 **********************************************************/
static int openTCPConnectionToUpgradeManager(void)
{
    struct sockaddr_in AppToServerAddress;
    int upgradeManagerSocket = -1;

    /* Create the Pod to Web Server socket */
    upgradeManagerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (upgradeManagerSocket < 0)
    {
        printf("Socket create failed\n");
        return -1;
    }

    memset(&AppToServerAddress, 0, sizeof(AppToServerAddress));
    AppToServerAddress.sin_family = AF_INET;
    AppToServerAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    AppToServerAddress.sin_port = htons(SOCKET_PORT_UPGRADE_MGR);

    if (connect(upgradeManagerSocket, (struct sockaddr *)&AppToServerAddress,
                sizeof(AppToServerAddress)) < 0)
    {
        perror("connect to Upgrade Manager Server failed: ");
        return -1;
    }

    return upgradeManagerSocket;
}


/*********************************************************
 *   NAME: svsSWUServerInit
 *   DESCRIPTION: Called when application starts.  Forks a
 *               process and exec's the Card Manager daemon
 *               Waits 5 seconds to allow Card Manager to
 *               start.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsSWUServerInit(void)
{
    int nRetVal = ERR_PASS;
    pid_t pid;
    swu_connection_type_t connectionType = swu_Command;
    int dataLength = sizeof(swu_Command);

//#ifndef DEBUG
    /* Fork our upgrademgr application */
    pid = fork();
    switch (pid)
    {
        case -1:
            printf("[%s] fork of upgrademanager failed\n", __func__);
        break;

        case 0:
        {
            char *prog1_argv[4];
            prog1_argv[0] = SWU_MANAGER_DAEMON_NAME;
            prog1_argv[1] = NULL;
            execvp(prog1_argv[0], prog1_argv);
        }
        break;

        default:
            sleep(5); // Allow time for the upgrade manager to startup
        break;
    }

//#endif /* DEBUG */

    /* Create client socket and connect to Upgrade Manager */
    upgradeManagerCmdSocket = openTCPConnectionToUpgradeManager();
    if (upgradeManagerCmdSocket < 0)
    {
        return -1;
    }

    // First send Connection Type to server
    nRetVal = sendUpgradeManagerCommand(upgradeManagerCmdSocket, UPGRADE_MANAGER_CONN_TYPE,
                                     dataLength, &connectionType);
    if (nRetVal < 0)
    {
        return -1;
    }

    return nRetVal;
}

/* Kill off the Upgrade Manager, we are finished with it. */
/*********************************************************
 *   NAME: svsSWUServerUninit
 *   DESCRIPTION: Called on shutdown.  Sends Upgrade Manager
 *               an exit command, waits for return status
 *               from Upgrade Manager.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsSWUServerUninit(void)
{
    swu_upgradeMgrResult_t UpgradeMgrResult;
    unsigned char commandData[32];
    int rc;
    int dataSize;

    rc = sendUpgradeManagerCommand(upgradeManagerCmdSocket, UPGRADE_MANAGER_EXIT, 0, commandData);
    if (rc < 0)
    {
        /* Some kind of error */
        return -1;
    }

    dataSize = sizeof(swu_upgradeMgrResult_t);
    /* recv() with timeout to get status */
    rc = recvUpgradeManagerResult(&upgradeManagerCmdSocket, &UpgradeMgrResult, &dataSize);

    close(upgradeManagerCmdSocket);
    upgradeManagerCmdSocket = -1;

    return ERR_PASS;
}

/*********************************************************
 *   NAME: svsUpgradeInit
 *   DESCRIPTION: Called by svsInit() which is called by
 *                the application at startup.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsUpgradeInit(void)
{
    int rc;

    swu_connection_type_t connectionType = swu_Command;
    int dataLength = sizeof(swu_Command);

    /* Create client socket and connect to Card Manager */
    upgradeManagerCmdSocket = openTCPConnectionToUpgradeManager();
    if (upgradeManagerCmdSocket < 0)
    {
        return ERR_FAIL;
    }

    /* First send Connection Type to server */
    rc = sendUpgradeManagerCommand(upgradeManagerCmdSocket, UPGRADE_MANAGER_CONN_TYPE, dataLength,
                                &connectionType);
    if (rc < 0)
    {
        return ERR_FAIL;
    }
    return ERR_PASS;
}


/*********************************************************
 *   NAME: svsUpgradeUninit
 *   DESCRIPTION: Called by svsUninit() which is called by
 *                the application at startup.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsUpgradeUninit(void)
{
    close(upgradeManagerCmdSocket);
    upgradeManagerCmdSocket = -1;
    return ERR_PASS;
}


char gOTAUpgradePackagePath[256] = {0};

/*********************************************************
 *   NAME: svsDoOTAUpgrade
 *   DESCRIPTION: Start an upgrade of Kiosk software using
 *                  and Over The Air upgrade package from
 *                  the back end server.
 *   IN: Pointer to buffer holding the pathname for an OTA
 *               upgrade file.
 *   OUT:  Success or error code status.
 *
 **********************************************************/
svs_err_t *svsDoOTAUpgrade(char *otaUpgradePackagePath)
{
    static svs_err_t theError;
    swu_upgradeMgrResult_t theCmdResult;
    int resultDataSize;
    unsigned char commandData[256];

    int retval = 0;

    if (otaUpgradePackagePath)
    {
        strcpy((char *)commandData, otaUpgradePackagePath);
        // Here we will want to notify the upgrade manager application
        // about our new upgrade package and kick off the upgrade?
        /* Send UPGRADE_MANAGER_OTA command */
        retval = sendUpgradeManagerCommand(upgradeManagerCmdSocket,
                                       UPGRADE_MANAGER_OTA_UPGRADE_CMD,
                                       0, commandData);
        if (retval < 0)
        {
            theError.code = ERR_SOCK_FAIL;
            theError.str = "SOCKET FAILURE";
            return (&theError);
        }

        resultDataSize = sizeof(swu_upgradeMgrResult_t);
        /* recv() with timeout to get status */
        retval = recvUpgradeManagerResult(&upgradeManagerCmdSocket, &theCmdResult, &resultDataSize);
        if (retval)
        {
            theError.code = ERR_SOCK_FAIL;
            theError.str = "SWU_ERR_INIT 1";
            return (&theError);
        }

        if (theCmdResult.error)
        {
            svs_err_t *pError = swu_errUpdate(theCmdResult.status);
            memcpy(&theError, pError, sizeof(svs_err_t));
            return &theError;
        }
    }

    if (retval)
    {
#define ERR_GETSWVERS 100
        theError.code = ERR_GETSWVERS;
        theError.str = "Get SWVersion Failed";
    }
    else
    {
        theError.code = ERR_PASS;
        theError.str = "Success";
    }
    return (&theError);
}


/*********************************************************
 *   NAME: svsGetSWVersion
 *   DESCRIPTION: Get the currently installed version number
 *                  for the Kiosk.
 *   IN:   Pointer to buffer to receive version number.
 *   OUT:  Returns 0 for success or 1 for failure 
 *
 **********************************************************/
int svsGetSWVersion(char *currentSWVersion)
{
    int status = 0;

    xmlNodePtr versionNode;

    xmlNodePtr root;

    xmlDocPtr versDoc;
    char currentVersionsFile[256];
    char *workStr;

    sprintf(currentVersionsFile, "%s", "/usr/scu/default/versions.xml");

    // Parse the current versions XML file, get the version number
    versDoc = xmlReadFile(currentVersionsFile, NULL, XML_PARSE_RECOVER);
    if (versDoc == NULL )
    {
        // If no versions.xml file in the current /usr/scu/default then we will do the upgrade
        logError("Unable to read %s. File does not exist.", currentVersionsFile); 
        return ERR_FAIL;
    }

    root = xmlDocGetRootElement(versDoc);
    if ( root == NULL )
    {
        logError("Unable to parse %s. Could not find root node.", currentVersionsFile);
        return ERR_FAIL; 
    }


    versionNode = getNodeByName(root, "version");
    if (versionNode == NULL)
    {
        /* There is a problem with the versions.xml file */
        logError("Unable to parse %s. Could not find versionNode.", currentVersionsFile);
        return ERR_FAIL; 
    }


    /* Reset the retval to be no valid version each
     * time we start the processing again.
     * retval will be set to success by each function that is
     * called if there was no error, so we want to accurately
     * report no valid version found when we drop out of the loop at
     * end of XML file.
     */

#define SWUPDATE_MIN_VER_LEN 5

    // Now get releaseVersion
    workStr = getNodeContentByName(versionNode, "releaseVersion");
    if (workStr)
    {
        int tmpLen = strlen(workStr);
        if (tmpLen >= SWUPDATE_MIN_VER_LEN)
        {
            strcpy(currentSWVersion, workStr);
            xmlFree(workStr);
        }
        else
        {
            logError("Bad version value read from versions.xml");
            status = 1;
            xmlFree(workStr);
        }
    }
    else
    {
        logError("Unable to get release version from versions.xml");
        status = 1;
    }

    if (versDoc)
    {
        xmlFreeDoc(versDoc); /* Effectively close the XML file */
    }

    return status;
}
