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

 /**************************************************************************

	upgrademanager.c

	Software Update Manager Application main processing

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>

#include <common.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <termios.h>

#include "ports.h"
#include "xmlConfig.h"
#include "upgrademanager.h"
#include "netlink.h"
#include "socketlist.h"
#include "upgradedatadefs.h"
#include "swupdate.h"

#ifndef SUCCESS
    #define SUCCESS 0
#endif

#define ERR_NO_DIRECTORY -100
#define ERR_FINDING_DISK_DEVICENAME -101

/* The timeout interval to use for select() */
#define MAIN_SELECT_TIMEOUT_SECS 10

/* Allow a maximum of 2 connection at a time to the TCP/IP socket. */
#define MAXPENDING_CONNECTIONS 2

/* Socket handles */
int uevent_fd = -1;

/* TCP/IP sockets fd for Web Server interaction */
static int ServerSocket = -1;

// int theTCPServerPort = UPGRADEMANAGER_PORT;

unsigned int ClientCmdLen;
struct sockaddr_in ClientCmdAddress;
struct socketlist socketConnectionList;

char szDiskName[256] = {0};
bool bDiskDeviceAttached = false;
bool bDiskDeviceMounted = false;

unsigned long		Port;
unsigned long		Baudrate;

bool bShutdown = false;

static bool createPIDFile = false;
static char *pidFile;

/********************************************************
 * NAME: closeSocketListHandle()
 * DESCRIPTION: Closes an open socket handle from the socket
 *              list.
 *
 * IN: socketHandle the handle of the socket to close.
 * OUT: SUCCESS (0) or error.
 *********************************************************/
int closeSocketListHandle(int socketHandle)
{
    struct socketEntry *pSocketEntry;

    pSocketEntry = find_socketlist_entry_by_handle( &socketConnectionList, socketHandle );
    if(pSocketEntry)
    {
        if(pSocketEntry->socketHandle >= 0)
        {
            close(pSocketEntry->socketHandle);
            pSocketEntry->socketHandle = -1;
            // Remove the entry from the list
            // del_socketlist_entry( &socketConnectionList, pSocketEntry );
        }
    }
    return SUCCESS;
}

/********************************************************
 * NAME: CleanupOpenHandles()
 * DESCRIPTION: Closes all open handles.
 *
 * IN: None.
 * OUT: None.
 *********************************************************/
void CleanupOpenHandles(void)
{
    struct socketEntry *pSocketEntry;

    /* Close TCP/IP sockets if open. */
    if (ServerSocket > 0)
    {
        close(ServerSocket);
        ServerSocket = -1;
    }

    TAILQ_FOREACH(pSocketEntry, &socketConnectionList.head, next_entry)
    {
        closeSocketListHandle(pSocketEntry->socketHandle);
    }

    TAILQ_FOREACH(pSocketEntry, &socketConnectionList.head, next_entry)
    {
        del_socketlist_entry( &socketConnectionList, pSocketEntry );
    }
}

/*********************************************************
 * NAME: processSignals()
 * DESCRIPTION: Processes the HUP signal.  Reads
 * configuration file and applies any changes.
 *
 * IN: sig - The signal value.
 * OUT: Nothing.
 *********************************************************/
static void processSignals(int sig)
{
    switch (sig)
    {
        case SIGHUP:
        break;

        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
        case SIGUSR1:
#ifdef DEBUG
            printf("[%s] shutting down upgrade manager\n", __func__);
#endif
            CleanupOpenHandles();
            exit(0);
        break;
    }
}

/*********************************************************
 * NAME: InitTCPIPServer()
 * DESCRIPTION: Sets up the TCP/IP server to use to
 *              connect with the Web Server
 *
 * IN: None.
 * OUT: SUCCESS (0) or error code.
 *********************************************************/
int initTCPIPServer(void)
{
    struct sockaddr_in ServerAddress;

    ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerSocket < 0)
    {
        CleanupOpenHandles();
        return -1;
    }

    memset(&ServerAddress, 0, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ServerAddress.sin_port = htons(SOCKET_PORT_UPGRADE_MGR);

    if (ServerSocket > 0)
    {
        int yes = 1;
        setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(ServerSocket, (struct sockaddr *)&ServerAddress, sizeof(ServerAddress)) < 0)
        {
            CleanupOpenHandles();
            return -2;
        }

        /* Need to listen on the server socket */
        if (listen(ServerSocket, MAXPENDING_CONNECTIONS) < 0)
        {
            CleanupOpenHandles();
            return -3;
        }

    }

    return SUCCESS;
}

/*********************************************************
 * NAME: usage
 * DESCRIPTION: display command-line options
 * IN: name: program name
 * OUT: None
 *********************************************************/
static void usage(char *name)
{
    printf("Usage: %s [-d]\n", name);
    printf("\t-p <pid file>: create a PID file\n");
    exit(1);
}

/*********************************************************
 * NAME:  processArgs
 * DESCRIPTION: process command-line arguments
 * IN: argc: argument count
 *     argv: argument list
 * OUT: None
 *********************************************************/
static void processArgs(int argc, char *argv[])
{
    int c = 0;

    while ((c = getopt(argc, argv, "p:d")) != -1)
    {
        switch (c)
        {
            case 'p':
                createPIDFile = true;
                pidFile = optarg;
            break;

            case 'd':
            break;

            default:
                usage(argv[0]);
            break;
        }
    }
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
int sendTCPBuffer( int socketHandle, void *buffer, int bufferSize )
{
    if (socketHandle < 0)
    {
#ifdef DEBUG
        printf( "[%s] Handle invalid\n", __func__);
#endif
        return -2;
    }

    // Now send the actual data buffer, which should include the total size as first number of bytes
    if (send(socketHandle, buffer, bufferSize, MSG_NOSIGNAL) != bufferSize )
    {
#ifdef DEBUG
        printf( "[%s] Sent different number of bytes than expected\n", __func__);
#endif
        close(socketHandle);
        socketHandle = -1;
        return -1;
    }

    return SUCCESS;
}
/*********************************************************
*   NAME: recvTCPBuffer
*   DESCRIPTION: Receives the command request from the Kiosk
*                application.
*
*   IN: socketHandle - TCP socket handle.
*       buffer - pointer to buffer to hold command and any
*               data.
*       byteCount - pointer to int to receive size of
*               the received data.
*   OUT:    SUCCESS (0) or error code.
*
**********************************************************/
int recvTCPBuffer(
    int *socketHandle,
    void *buffer,
    int *byteCount)
{
    int bytesRcvd;

    if (*socketHandle < 0)
    {
#ifdef DEBUG
        printf("[%s] Invalid handle\n", __func__);
#endif
        *byteCount = 0;
        return -2;
    }

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

    return SUCCESS;
}

/*********************************************************
*   NAME: recvUpgradeManagerCommand
*   DESCRIPTION: Receives and processes a TCP/IP command.
*   IN: Handle to TCP/IP socket.
*   OUT:  SUCCESS (0) or error code.
*
**********************************************************/
int recvUpgradeManagerCommand(int cmdSocketHandle)
{
    int nRetVal = SUCCESS;
    int bytesReceived = 4096; // Set max bytes to receive

    unsigned char recvBuffer[4096];
    memset(recvBuffer, 0, sizeof(recvBuffer));

    nRetVal = recvTCPBuffer(&cmdSocketHandle, recvBuffer, &bytesReceived);
    if (nRetVal)
    {
        printf("[%s] Failed to receive response\n", __func__);
        return nRetVal;
    }

    // Here we need to parse the command, and perform the requested operation on the card reader
    // if appropriate.
    swu_upgradeOTACmd_t *pUpgradeOTACmd = (swu_upgradeOTACmd_t *)recvBuffer;

    switch(pUpgradeOTACmd->swu_command.command)
    {
        case UPGRADE_MANAGER_OTA_UPGRADE_CMD:
            nRetVal = performUpdate(pUpgradeOTACmd->upgradePkgPathname);
            if(nRetVal)
            {
                swu_upgradeMgrResult_t swuResult;
                // Return success status to Kiosk application
                swuResult.protocolVersion = UPGRADE_PROTOCOL_VERSION;

                swuResult.command = pUpgradeOTACmd->swu_command.command;
                swuResult.error = true;
                swuResult.status = nRetVal;

                nRetVal = sendTCPBuffer( cmdSocketHandle, &swuResult, sizeof(swu_upgradeMgrResult_t) );
                if (nRetVal)
                {
#ifdef DEBUG
                    printf("[%s] Problem sending result\n", __func__);
#endif
                }
            }
            else
            {
                swu_upgradeMgrResult_t swuResult;
                // Return success status to Kiosk application
                swuResult.protocolVersion = UPGRADE_PROTOCOL_VERSION;

                swuResult.command = pUpgradeOTACmd->swu_command.command;
                swuResult.error = false;
                swuResult.status = SUCCESS;

                nRetVal = sendTCPBuffer( cmdSocketHandle, &swuResult, sizeof(swu_upgradeMgrResult_t) );
                if (nRetVal)
                {
#ifdef DEBUG
                    printf("[%s] Problem sending result\n", __func__);
#endif
                }

            }
            break;

        case UPGRADE_MANAGER_EXIT:
            // Clean up if needed, sets shutdown flag.
            CleanupOpenHandles();
            break;

        default:
            break;
    }

//printf("PM: Received %d bytes Cmd: %d Vers: %d\n\n", bytesReceived, pUpgradeMgrCmd->command, pUpgradeMgrCmd->protocolVersion);
    return nRetVal;
}

/*********************************************************
 * NAME:   main
 * DESCRIPTION: Main processing function, including main
 *              loop.
 * IN: Nothing.
 * OUT: 0 or error code.
 *********************************************************/
int main( int argc, char** argv )
{
    int	nRetVal;
    int theTCPServerPort = SOCKET_PORT_UPGRADE_MGR;

    processArgs(argc, argv);

    signal(SIGINT, processSignals);
    signal(SIGQUIT, processSignals);
    signal(SIGTERM, processSignals);
    signal(SIGHUP, processSignals);

    prctl(PR_SET_PDEATHSIG, SIGUSR1);

    atexit(CleanupOpenHandles);

    // Initialize the XML configuration code. not really a server per se.
    nRetVal = xmlConfigInit("/usr/scu/etc/svsConfig.xml");

    if(nRetVal != SUCCESS)
    {
        printf("Failed to initialize svsConfigServer\n");
        exit(1);
    }

    nRetVal = xmlConfigParamIntGet("upgrademanager", "tcpserverport", SOCKET_PORT_UPGRADE_MGR, &theTCPServerPort);

    if(nRetVal != SUCCESS)
    {
        return nRetVal;
    }


    uevent_fd = init_netlink_uevent_socket();
    if(uevent_fd < 0)
    {
        // Cannot do any processing of USB software updates
        printf("Error: Netlink socket failed to open\n");
    }

    /*
     * This will be used to set up socket for the Kiosk Application.
     */
    nRetVal = initTCPIPServer();
    if (nRetVal)
    {
        printf("Error starting server...exiting\n");
        // Need to log this and remove printf()
        exit(1);
    }

    /* Init the Socket Connection list */
    init_socketlist(&socketConnectionList);
    while (1)
    {
        fd_set read_fds;
        int status;
        int track_fd = 0;
        struct socketEntry *pSocketEntry;
        struct socketlist *pSocketList = &socketConnectionList;

        if(bShutdown)
        {
            break;
        }

        FD_ZERO(&read_fds);

        FD_SET(uevent_fd, &read_fds);
        if(uevent_fd > track_fd)
        {
            track_fd = uevent_fd;
        }

        FD_SET(ServerSocket, &read_fds);
        track_fd = ServerSocket;

        // Track the socket list and add any non-async socket handles to the read_fds set
        TAILQ_FOREACH(pSocketEntry, &pSocketList->head, next_entry)
        {
            if(pSocketEntry->connType == swu_Command)
            {
                if(pSocketEntry->socketHandle > track_fd)
                {
                    FD_SET(pSocketEntry->socketHandle, &read_fds);
                    track_fd = pSocketEntry->socketHandle;
                }
            }
        }

        /* wait for activity on the file descriptors */
        status = select(track_fd + 1, &read_fds, (fd_set *)0, (fd_set *)0, NULL);

        if (status < 0)
        {
            nRetVal = errno;
            if (errno == EBADF)
            {
                /* We got a bad file descriptor error
                 * see which one it is.
                 */
                nRetVal = fcntl(uevent_fd, F_GETFL );
                if (nRetVal < 0)
                {
                    /*
                     * File descriptor is bad
                     */
                    close(uevent_fd);
                    uevent_fd = -1;
                }

                TAILQ_FOREACH(pSocketEntry, &pSocketList->head, next_entry)
                {
                    if(pSocketEntry->connType == swu_Command)
                    {
                        if(pSocketEntry->socketHandle >= 0)
                        {
                            nRetVal = fcntl(pSocketEntry->socketHandle, F_GETFL );
                            if (nRetVal < 0)
                            {
                                /*
                                 * File descriptor is bad
                                 * Probably client disconnected.
                                 */
                                close(pSocketEntry->socketHandle);
                                pSocketEntry->socketHandle = -1;
                            }
                        }
                    }
                }

            } /* End error EBADF */

            // Continuing for processing
            continue;
        } /* End of error case from select() */

        if (status == 0)
        {
            continue;
        }

        if(FD_ISSET(uevent_fd, &read_fds))
        {
            // Process the notification to see if we have a USB drive attach
            // Mounting, and processing will be completed when we return
            process_netlink_uevent_message(uevent_fd);
        }

        if (FD_ISSET(ServerSocket, &read_fds))
        {
            int lclSocketHandle = -1;

            /* Now accept the connection */
            if ((lclSocketHandle = accept(ServerSocket, (struct sockaddr *)&ClientCmdAddress, &ClientCmdLen)) < 0)
            {
                lclSocketHandle = -1;
            }
            else
            {
                // Get a new socketentry for the list.
                struct socketEntry *pSocketEntry;
                swu_upgradeMgrCmd_t swuCommand;
                int byteCount;

                pSocketEntry = add_socketlist_entry(&socketConnectionList, lclSocketHandle);
                if(pSocketEntry)
                {
                    // Add the socket handle to socketlist entry.
                    pSocketEntry->socketHandle = lclSocketHandle;

                    // Read the Type structure from the socket.  Just a straight read with timeout.
                    byteCount = sizeof(swu_upgradeMgrCmd_t);
                    nRetVal = recvTCPBuffer(&(pSocketEntry->socketHandle), &swuCommand, &byteCount);
                    if(nRetVal == SUCCESS)
                    {
                        if(swuCommand.command != UPGRADE_MANAGER_CONN_TYPE)
                        {
                            printf("UpgradeMgr: Expected Connection Type command but received %d\n",
                                   swuCommand.command);
                        }
                        else
                        {
                            pSocketEntry->connType = swuCommand.connType;
                            /*
                            * Added the ID value to the socketlist entry.
                            * Now we are set to process, we are done with the socket
                            * handle var we used here, it can become local.  For select
                            * need to walk the list and get the handle(s) If it is not a
                            * command socket or a monitor socket then we will add to the select set.
                            */
                        }
                    } // End of success reading ID command from the client
                 } // End of error getting socket list entry
            } // End of accept() was successful
        } // End of Server Socket is set

        TAILQ_FOREACH(pSocketEntry, &pSocketList->head, next_entry)
        {
            if(pSocketEntry->connType == swu_Command)
            {
                if(FD_ISSET(pSocketEntry->socketHandle, &read_fds))
                {
                    if (recvUpgradeManagerCommand(pSocketEntry->socketHandle) < 0)
                    {
                        if (pSocketEntry->socketHandle > 0)
                        {
                            close(pSocketEntry->socketHandle);
                            pSocketEntry->socketHandle = -1;
                        }
                        continue;
                    }
                }
            }
        }


    } /* End of while(1) */

    CleanupOpenHandles();
    return 0;
}

