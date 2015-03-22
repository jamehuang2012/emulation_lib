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
 *   svsPM.c Power Manager API Implementation for svs library
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

#include <svsPM.h>

#define CONNECTION_DELAY    15

static int powerManagerSocket = -1;
static int applicationSocket = -1;
static int pmServerActive = 0;

pthread_t thread_id;

/*
 * Global storage for static Power Manager board info
 * This stuff never changes, so we'll get it once and save it
 */
int pdmFound = 0;
int scmFound = 0;
static char *pdmFWVersion = NULL;
static char *pdmSerialNum = NULL;
static char *scmFWVersion = NULL;
static char *scmSerialNum = NULL;

#define THREAD_SELECT_TIMEOUT_SECS  1

/*********************************************************
 *   NAME: openTCPConnectionToManager
 *   DESCRIPTION: Opens the socket connection to the
 *               Power Manager.
 *   IN:    Nothing.
 *   OUT:   Socket handle or negative error code.
 *
 **********************************************************/
static int openTCPConnectionToManager(void)
{
    struct sockaddr_in AppToServerAddress;
    int managerSocket = -1;

    managerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (managerSocket < 0)
    {
        logError("Socket create failed\n");
        return -1;
    }

    memset(&AppToServerAddress, 0, sizeof(AppToServerAddress));
    AppToServerAddress.sin_family = AF_INET;
    AppToServerAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    AppToServerAddress.sin_port = htons(SOCKET_PORT_POWER_MGR);

    if (connect(managerSocket, (struct sockaddr *)&AppToServerAddress,
                sizeof(AppToServerAddress)) < 0)
    {
        logError("Connection to Power Manager Server failed: ");
        return -1;
    }

    return managerSocket;
}

/*********************************************************
 * NAME:   svsPMThread()
 * DESCRIPTION: Thread function that processes track data
 *              notifications received from the Power
 *              Manager.
 *
 * IN:   arg - Pointer to data structure containing the
 *              socket handle and callback function
 *              pointer.
 * OUT: void pointer to the passed in arg pointer.
 *********************************************************/
static void *svsPMThread(void *arg)
{
    int callbackSocket = -1;
    int rc = ERR_PASS;

    /*
     * Create the callback client socket,
     * this will be used to send the messages to the callback server
     */
    rc = svsSocketClientCreateCallback(&callbackSocket, 0);
    if (rc)
    {
        return arg;
    }

    pmServerActive = 1;

    /* While thread is alive process notifications from the Power Manager */
    while (pmServerActive)
    {
        fd_set read_fds;
        int status;
        int track_fd;

        if (powerManagerSocket <= 0)
        { 
            pm_message_type_t register_cb = PM_REGISTER_CB_SERVER;

            logInfo("Attempting to establish connection with Power Manager");

            /* Open the socket to the Power Manager application */
            while (powerManagerSocket <= 0)
            {
                powerManagerSocket = openTCPConnectionToManager(); 
                
                /* If the connection wasn't opened, sleep for 15 sec before retry */
                if (powerManagerSocket <= 0)
                {
                    sleep(CONNECTION_DELAY);
                }
            }

            /* Register our socket with the power manager */
            status = send(powerManagerSocket, &register_cb, sizeof(register_cb), 0);
            if (status <= 0)
            {
                logError("send() returned %d", status);
            }
            
            logInfo("Connection with the Power Manager is establish and callback is registered");
        }

        FD_ZERO(&read_fds);

        track_fd = powerManagerSocket;
        FD_SET(powerManagerSocket, &read_fds);

        /* wait for activity on the file descriptors */
        status = select(track_fd + 1, &read_fds, (fd_set *)0, (fd_set *)0, NULL);
        if (status < 0)
        {
            /* select() error */
            logError("select() returned %d in PM thread", status);
            continue;
        }
        else if (status == 0)
        {
            /* select() timeout */
            continue;
        }
        else if (FD_ISSET(powerManagerSocket, &read_fds))
        {
            uint8_t buffer[1024];
            int nbytes;

            nbytes = recv(powerManagerSocket, buffer, 1024, 0);
            switch (nbytes)
            {
                case -1:
                break;

                case 0:
                    /*
                     * This means connection to the PM has died,
                     * probably because the power manager itself
                     * died. So let's close it out and attempt to
                     * reconnect hoping it comes back
                     */
                    logError("Received 0 bytes from PM. Attempting to reconnect");

                    /* Close out the old socket */
                    close(powerManagerSocket);
                    powerManagerSocket = -1;
                break;

                default:
                    /* THIS SHOULD ONLY BE CALLED AT LOW VOLTAGE WARNING */
                    logInfo("Received low voltage warning");
                    rc = svsSocketSendCallback(callbackSocket, MODULE_ID_PM, 0, 0, NULL, (uint8_t *)buffer, nbytes);
                    if (rc != ERR_PASS)
                    {
                        logError("Unable to send callback. Returned %d", rc);
                    }
                break;
            }
        }
    }

    logInfo("Cleaning up and exiting SVS PM thread");

    close(powerManagerSocket);
    svsSocketClientDestroyCallback(callbackSocket);
    powerManagerSocket = -1;

    /* Clean up thread and exit thread function, kills thread */
    return arg;
}

/*********************************************************
 * NAME: svsPMServerInit
 * DESCRIPTION: Called when SVS application starts. Forks
 *              a process and exec's the Power Manager
 *              daemon. Will wait 5 seconds to allow the
 *              Manager to start then attempts communication
 *
 * IN:  Nothing.
 * OUT: Success (0) or negative error code.
 *
 **********************************************************/
int svsPMServerInit(void)
{
    pthread_attr_t thread_attr;
    pid_t pid;

    int nRetVal = ERR_PASS;

    /* Fork our powermanager application */
    pid = fork();
    switch (pid)
    {
        case -1:
            logError("fork of powermanager failed");
        break;

        case 0:
        {
            char *prog1_argv[4];
            prog1_argv[0] = PM_DAEMON_NAME;
            prog1_argv[1] = NULL;
            execvp(prog1_argv[0], prog1_argv);
        }
        break;

        default:
            sleep(5); /* Allow time for the Power Manager to startup */
        break;
    }

    /* Create the Power Manager SVS Thread */
    nRetVal = pthread_attr_init(&thread_attr);
    if (nRetVal)
    {
        return ERR_FAIL;
    }

    nRetVal = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    if (nRetVal)
    {
        return ERR_FAIL;
    }

    nRetVal = pthread_create(&thread_id, &thread_attr, svsPMThread, NULL);
    if (nRetVal)
    {
        return ERR_FAIL;
    }

    return nRetVal;
}

/* Kill off the Power Manager, we are finished with it. */
/*********************************************************
 *   NAME: svsPMServerUninit
 *   DESCRIPTION: Called on shutdown.  Sends Power Manager
 *               an exit command, waits for return status
 *               from the Power Manager.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsPMServerUninit(void)
{
    pm_message_type_t message = PM_EXIT;
    int status;

    status = send(powerManagerSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
    }

    /* Kill the PM thread */
    pmServerActive = 0;
    sleep(2);   /* wait for the PM thread to clean up */

    return ERR_PASS;
}

/*********************************************************
 *   NAME: svsPMInit
 *   DESCRIPTION: Called by svsInit() which is called by
 *                the application at startup.
 *   IN: Nothing.
 *   OUT:  Success (0) or negative error code.
 *
 **********************************************************/
int svsPMInit(void)
{
    struct timeval tv;
    int rc;

    /* Create socket from the Kiosk application to the Power Manager */
    applicationSocket = openTCPConnectionToManager();
    if (applicationSocket < 0)
        return ERR_FAIL;

    tv.tv_sec = 1;  /* 1 second timeout */

    setsockopt(applicationSocket, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    /* Get the PDM info and store it locally */
    rc = svsGetPDMInfo();
    if (rc)
    {
        logError("Unable to retrieve PDM info");
        pdmFound = 0;
    }
    else
    {
        pdmFound = 1;
    }

    rc = svsGetSCMInfo();
    if (rc)
    {
        logError("Unable to retrieve SCM info");
        scmFound = 0;
    }
    else
    {
        scmFound = 1;
    }

#if 0
    /* 
     * Temporary call to shut down PAYPASS to save power until
     * processing has been implemented
     */
    svsSetPDMPowerDevicePortState(PDM_PAYPASS_DEV_POWER_PORT, PDM_POWER_DEV_OFF);
#endif

    return ERR_PASS;
}

/*********************************************************
 * NAME: svsPMUninit
 * DESCRIPTION: Called by svsUninit() which is called by
 * the application at startup.
 *
 * IN:  Nothing.
 * OUT: Success (0) or negative error code.
 *
 **********************************************************/
int svsPMUninit(void)
{
    close(applicationSocket);
    applicationSocket = -1;

    return ERR_PASS;
}

/*********************************************************
 * NAME: svsGetPDMInfo
 * DESCRIPTION: Sends a message to the PM to retrieve the
 *              firmware version and serial number of the
 *              PDM. This will update the global variables
 *              that will be used later by applications
 *
 * IN:  None
 * OUT: Returns 0 for success or 1 for failure
 *
 **********************************************************/
int svsGetPDMInfo(void)
{
    pm_info_t message;
    int status;
    int nbytes;

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    message.type = PDM_GET_INFO;

    /* Send the command */
    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return ERR_FAIL;  
    }

    /* Receive the response */
    nbytes = recv(applicationSocket, &message, sizeof(message), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else
    {
        if (message.type == PM_DEVICE_READ_ERROR)
        {
            logError("Device read error");
            return ERR_FAIL;
        }
        else
        {
            logDebug("Received PDM Info");

            /* Store the data locally */
            pdmFWVersion = (char *)malloc(strlen(message.fw_version) + 1);
            pdmSerialNum = (char *)malloc(strlen(message.serial_number) + 1);

            strcpy(pdmFWVersion, message.fw_version);
            strcpy(pdmSerialNum, message.serial_number);
        }
    }

    return ERR_PASS;
}

/*********************************************************
 * NAME: svsGetPDMFWVersion
 * DESCRIPTION: Returns the fw version stored in the
 *              global data. This was filled by
 *              svsGetPDMInfo() when svsPMInit() was called
 *
 * IN:  None
 * OUT: Pointer to the PDM firmware version
 *
 **********************************************************/
char *svsGetPDMFWVersion(void)
{
    return pdmFWVersion;
}

/*********************************************************
 * NAME: svsGetPDMSerial
 * DESCRIPTION: Returns the serial number stored in the
 *              global data. This was filled by
 *              svsGetPDMInfo() when svsPMInit() was called
 *
 * IN:  None
 * OUT: Pointer to the PDM serial number
 *
 **********************************************************/
char *svsGetPDMSerialNumber(void)
{
    return pdmSerialNum;
}

/*********************************************************
 * NAME: svsGetPDMPowerDevicePortState
 * DESCRIPTION: Sends a message to the PM to retrieve the
 *              state of the input port. Returns the state
 *
 *
 * INPUT:  int device - port to retrieve state of
 * OUTPUT: state of the port specified
 *
 **********************************************************/
int svsGetPDMPowerDevicePortState(pdm_power_device_t device)
{
    pm_pdm_port_state_t message;
    pm_pdm_port_state_t reply;
    int status;
    int nbytes;

    if (!pdmFound)
    {
        logError("PDM not found");
        return ERR_FAIL;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    message.type = PDM_GET_PORT_STATE;
    message.port = device;

    /* Send the command */
    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return ERR_FAIL;
    }

    /* Receive the response */
    nbytes = recv(applicationSocket, &reply, sizeof(reply), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else if (reply.type == PM_DEVICE_READ_ERROR)
    {
        logError("Device read error. Unable to retrieve port state");
        return ERR_FAIL;
    }

    return reply.state;
}

/*********************************************************
 * NAME: svsSetPDMPowerDevicePortState
 * DESCRIPTION:
 *
 *
 * INPUT:   int device - device list in pmdatadefs.h
 *          int state - PDM_POWER_DEV_ON or PDM_POWER_DEV_OFF
 * OUTPUT:  Success or error code status.
 *
 **********************************************************/
void svsSetPDMPowerDevicePortState(pdm_power_device_t device, int state)
{
    pm_pdm_port_state_t message;
    int status;
    int nbytes;

    if (!pdmFound)
    {
        logError("PDM not found");
        return;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return;
        }
    }

    message.type = PDM_SET_PORT_STATE;
    message.port = device;
    message.state = state;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return;
    }

    nbytes = recv(applicationSocket, &message, sizeof(message), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d. %s (errno=%d) ", nbytes, strerror(errno), errno);
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
    }
    else if (message.type == PM_DEVICE_READ_ERROR)
    {
        logError("Device read error. Unable to set port state");
    }
}

/*********************************************************
 * NAME: svsSetPDMPowerThreshold
 * DESCRIPTION:
 *
 *
 * INPUT:   float threshold - new threshold voltage
 *
 * OUTPUT:  None
 *
 **********************************************************/
void svsSetPDMPowerThreshold(float threshold)
{
    pm_pdm_power_threshold_t message;
    int status;
    int nbytes;

    if (!pdmFound)
    {
        logError("PDM not found");
        return;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return;
        }
    }

    message.type = PDM_SET_POWER_THRESHOLD;
    message.threshold = threshold;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return;
    }

    nbytes = recv(applicationSocket, &message, sizeof(message), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
    }
    else if (message.type == PM_DEVICE_READ_ERROR)
    {
        logError("Device read error. Unable to set power threshold");
    }
}

/*********************************************************
 * NAME: svsGetPowerThreshold
 * DESCRIPTION:
 *
 *
 * INPUT:   None
 *
 * OUTPUT:  float - the PDM power threshold
 *
 **********************************************************/
float svsGetPDMPowerThreshold(void)
{
    pm_pdm_power_threshold_t message;
    int status;
    int nbytes;

    if (!pdmFound)
    {
        logError("PDM not found");
        return ERR_FAIL;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    message.type = PDM_GET_POWER_THRESHOLD;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return ERR_FAIL;
    }

    nbytes = recv(applicationSocket, &message, sizeof(message), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else if (message.type == PM_DEVICE_READ_ERROR)
    {
        logError("Device read error. Unable to get power threshold");
        return ERR_FAIL;
    }

    return message.threshold;
}

/*********************************************************
 * NAME: svsGetPDMLoad
 * DESCRIPTION:
 *
 *
 * INPUT:
 *
 * OUTPUT:  Success or error code status.
 *
 **********************************************************/
int svsGetPDMLoad(float *load_v, int *load_i)
{
    pm_pdm_power_status_t message;
    pm_pdm_power_status_t *response;
    char buffer[1024];
    int status;
    int nbytes;
    int expectedLength = sizeof(pm_pdm_power_status_t);

    *load_v = 0;
    *load_i = 0;

    if (!pdmFound)
    {
        logError("PDM not found");
        return ERR_FAIL;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    /* Get the Voltage */
    message.type = PDM_GET_POWER_STATUS;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return ERR_FAIL;
    }

    nbytes = recv(applicationSocket, buffer, sizeof(buffer), 0);
    response = (pm_pdm_power_status_t *)buffer;
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else if (nbytes > expectedLength)
    {
        logError("Received %d bytes, expected %d bytes\n", nbytes, expectedLength);
        return ERR_FAIL;
    }
    else if (response->type == PM_DEVICE_READ_ERROR)
    {
        logError("Device read error. Unable to get PDM load");
        return ERR_FAIL;
    }

    logInfo("Successfully read PDM Load");
    *load_v = response->load_v;
    *load_i = response->load_i;

    return ERR_PASS;
}

/*********************************************************
 * NAME: svsGetSCMInfo
 * DESCRIPTION: Sends a message to the PM to retrieve the
 *              firmware version and serial number of the
 *              SCM. This will update the global variables
 *              that will be used later by applications
 *
 * IN:  None
 * OUT: Returns 0 for success or 1 for failure
 *
 **********************************************************/
int svsGetSCMInfo(void)
{
    pm_info_t message;
    int status;
    int nbytes;

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    message.type = SCM_GET_INFO;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
    }

    nbytes = recv(applicationSocket, &message, sizeof(message), 0);
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else
    {
        if (message.type == PM_DEVICE_READ_ERROR)
        {
            logError("Power Manager: Device read error. Unable to get SCM info");
            return ERR_FAIL;
        }
        else
        {
            logDebug("Received SCM Info");

            /* Store the data locally */
            scmFWVersion = (char *)malloc(strlen(message.fw_version) + 1);
            scmSerialNum = (char *)malloc(strlen(message.serial_number) + 1);

            strcpy(scmFWVersion, message.fw_version);
            strcpy(scmSerialNum, message.serial_number);
        }
    }

    return ERR_PASS;
}

/*********************************************************
 * NAME: svsGetSCMFWVersion
 * DESCRIPTION: Returns the fw version stored in the
 *              global data. This was filled by
 *              svsGetSCMInfo() when svsPMInit() was called
 *
 * IN:  None
 * OUT: Pointer to the PDM firmware version
 *
 **********************************************************/
char *svsGetSCMFWVersion(void)
{
    return scmFWVersion;
}

/*********************************************************
 * NAME: svsGetSCMSerialNumber
 * DESCRIPTION: Returns the serial number stored in the
 *              global data. This was filled by
 *              svsGetSCMInfo() when svsPMInit() was called
 *
 * IN:  None
 * OUT: Pointer to the PDM serial number
 *
 **********************************************************/
char *svsGetSCMSerialNumber(void)
{
    return scmSerialNum;
}

/*********************************************************
 * NAME: svsGetSCMPowerStatus
 * DESCRIPTION:
 *
 *
 * IN:  float *load_v - pointer to the load voltage to return
 *      int *load_i - pointer to the load current to return
 *
 * OUT: None
 *
 **********************************************************/
int svsGetSCMPowerStatus(float *load_v, int *load_i, float *battery_v, int *battery_i)
{
    pm_scm_power_status_t message;
    pm_scm_power_status_t *response;
    char buffer[1024];
    int status;
    int nbytes;
    int expectedLength = sizeof(pm_scm_power_status_t);

    *load_v = 0;
    *load_i = 0;
    *battery_v = 0;
    *battery_i = 0;

    if (!scmFound)
    {
        logError("SCM not found");
        return ERR_FAIL;
    }

    if (applicationSocket <= 0)
    {
        /* application socket is closed... try to reopen once */
        logError("Application socket was closed unexpectedly");
        logInfo("Attempting to open socket to Power Manager");

        applicationSocket = openTCPConnectionToManager();
        if (applicationSocket <= 0)
        {
            logError("Unable to open socket with Power Manager. Returning failure.");
            return ERR_FAIL;
        }
    }

    /* Get the Voltage */
    message.type = SCM_GET_POWER_STATUS;

    status = send(applicationSocket, &message, sizeof(message), 0);
    if (status < 0)
    {
        logError("send() returned %d", status);
        return ERR_FAIL;
    }

    nbytes = recv(applicationSocket, buffer, sizeof(buffer), 0);
    response = (pm_scm_power_status_t *)buffer;
    if (nbytes < 0)
    {
        logError("recv() error. returned %d", nbytes);
        return ERR_FAIL;        
    }
    else if (nbytes == 0)
    {
        logError("recv() error. remote socket closed. cleaning up", nbytes);
        close(applicationSocket);
        applicationSocket = -1;
        return ERR_FAIL;
    }
    else if (nbytes > expectedLength)
    {
        logError("Received %d bytes, expected %d bytes\n", nbytes, expectedLength);
        return ERR_FAIL;
    }
    else if (response->type == PM_DEVICE_READ_ERROR)
    {
        logError("Power Manager: Device read error. Unable to get SCM power status");
        return ERR_FAIL;
    }

    *load_v = response->load_v;
    *load_i = response->load_i;
    *battery_v = response->battery_v;
    *battery_i = response->battery_i;
    return ERR_PASS;
}
