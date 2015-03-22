
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsCallback.h>
#include <svsApi.h>
#include <svsBdp.h>
#include <libSVS.h>
#include <svsSocket.h>

static int svsSocketRecvFlush(int sockFd);

extern pthread_mutex_t mutexSocketRecv;
extern pthread_mutex_t mutexSocketSend;

int svsSocketClientCreateSvs(int *sockFd)
{
    return(svsSocketClientCreate(SVS_SOCKET_SERVER_IP, SOCKET_PORT_SVS, sockFd, 0));
}

int svsSocketClientCreateLog(int *sockFd)
{
    return(svsSocketClientCreate(SVS_SOCKET_SERVER_IP, SOCKET_PORT_LOG, sockFd, 0));
}

int svsSocketClientDestroyLog(int socketFd)
{
    svsSocketClientDestroy(socketFd);

    return(ERR_PASS);
}

int svsSocketClientDestroyCallback(int socketFd)
{
    svsSocketClientDestroy(socketFd);

    return(ERR_PASS);
}

int svsSocketClientDestroySvs(int socketFd)
{
    svsSocketClientDestroy(socketFd);

    return(ERR_PASS);
}

int svsSocketClientCreateCallback(int *sockFd, uint8_t appflag)
{
    return(svsSocketClientCreate(SVS_SOCKET_SERVER_IP, SOCKET_PORT_CALLBACK, sockFd, appflag));
}

int svsSocketClientCreateBdp(int *sockFd)
{
    return(svsSocketClientCreate(SVS_SOCKET_SERVER_IP, SOCKET_PORT_BDP_TX, sockFd, 0));
}

int svsSocketClientCreateKr(int *sockFd)
{
    return(svsSocketClientCreate(SVS_SOCKET_SERVER_IP, SOCKET_PORT_KR, sockFd, 0));
}

int svsSocketClientDestroyKr(int socketFd)
{
    svsSocketClientDestroy(socketFd);

    return(ERR_PASS);
}

int svsSocketClientDestroyBdp(int socketFd)
{
    svsSocketClientDestroy(socketFd);

    return(ERR_PASS);
}

int svsSocketClientCreate(char *ipAddr, int port, int *sockFd, uint8_t appflag)
{
    int rc;
    struct sockaddr_in servaddr;

    //logDebug("");

    *sockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*sockFd == -1)
    {
        logError("socket: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    //logDebug("svsSocketClientCreate port %d socket %d\n", port, *sockFd);

    bzero((void *) &servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_port           = htons(port);
    servaddr.sin_addr.s_addr    = inet_addr(ipAddr);

    int retry = 0;
    do
    {
        rc = connect(*sockFd, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (rc == -1)
        {   // allow some time for server to startup
            usleep(1000);
            retry++;
            if (retry >= 100)
            {
                break;
            }
        }
    } while(rc < 0);
    if (rc < 0)
    {
        close(*sockFd);
        logError("failed to contact server after %d retries, socket %d", retry, *sockFd);
        return(ERR_FAIL);
    }

    if (appflag)
    {
        svsSocketMsgHeader_t hdr;

        /*
         * Register with the callback server as an application. This allows
         * us to become a destination for messages that the callback
         * server receives. Note that appflag is only valid for calls from
         * svsSocketClientCreateCallback(). All other calls to this function
         * will set appflag to 0.
         * Note that this is a hoaky way of doing this because this is
         * supposed to be a generic routine. But it's the only thing we can
         * do right now.
         */
        memset(&hdr, 0, sizeof(hdr));
        strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
        hdr.module_id = MODULE_ID_CALLBACK;

        hdr.u.cbhdr.msg_id = (uint8_t)MSG_ID_CB_APP;

        rc = svsSocketSend(*sockFd, &hdr, NULL);
        if (rc < 0)
        {
            logError("Failed to register application with callback server");
            return(ERR_FAIL);
        }
    }
    //logInfo("success in contacting server after %d retries, socket %d", retry, *sockFd);

    return(ERR_PASS);
}

int svsSocketClientDestroy(int socketFd)
{
    close(socketFd);

    return(ERR_PASS);
}

int svsSocketServerCreate(socket_thread_info_t *thread_info)
{
    int rc = ERR_PASS;
    int status;

    logDebug("%s", thread_info->name);

    if (thread_info->len_max != 0)
    {
        uint8_t *p = 0;
        p = (uint8_t *)malloc(sizeof(uint8_t) * thread_info->len_max);
        if (p)
        {
            thread_info->payload = p;
        }
        else
        {
            logError("malloc: %s", strerror(errno));
            return(ERR_FAIL);
        }
    }

    // Create the threads
    status = pthread_create(&thread_info->pthread, NULL, thread_info->fnThread, (void *)thread_info);
    if (-1 == status)
    {
        logError("pthread_create: %s", strerror(errno));
        rc = ERR_FAIL;
    }

    return(rc);
}

int svsSocketServerDestroy(socket_thread_info_t *thread_info)
{
    return(ERR_PASS);
}

//
// Description:
// The client application calls this function to send the request to the server and waits for a response
//
int svsSocketServerBdpTransfer(int sockFdRx, int sockFdTx, module_id_t module_id, uint16_t dev_num, uint16_t msgID, blocking_t blocking, int timeout_ms, uint8_t flags, callback_fn_t callback, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int                     rc;
    struct timeval          timeout;
    svsSocketMsgHeader_t    hdr;
    int                     timeout_client_ms;

    if (sockFdRx <= 0)
        return ERR_FAIL;

    // in non blocking calls, the response is not set, so we clear all its data as this will set the status to ERR_PASS
    // just a way to avoid setting this in every API function
    memset(rsp_payload, 0, rsp_len);

    // flush the socket, in case previous message were not handled
    rc = svsSocketRecvFlush(sockFdRx);
    if ((rc != ERR_PASS) && (rc != ERR_COMMS_TIMEOUT))
    {
        logError("svsSocketRecvFlush failed");
        return(rc);
    }

    if (dev_num == BDP_NUM_ALL)
    {
        // We ask the BDP to respond when in broadcast for MSG_ID_BDP_ADDR_GET, otherwise it depends on the timeout and callback value
        if (msgID != MSG_ID_BDP_ADDR_GET)
        {
            if ((blocking == BLOCKING_OFF) && (timeout_ms == 0) && (callback == 0))
            {   // non blocking call and timeout 0 and callback 0, tell the BDP not to send a response
                flags |= BDP_MSG_FRAME_FLAG_NO_ACK;
            }
            else
            {
                if ((blocking == BLOCKING_ON) && (timeout_ms == 0))
                {   // blocking call and timeout 0, tell the BDP not to send a response
                    flags |= BDP_MSG_FRAME_FLAG_NO_ACK;
                }
            }
        }
    }
    else
    {
        if ((blocking == BLOCKING_OFF) && (timeout_ms == 0) && (callback == 0))
        {   // non blocking call and timeout 0 and callback 0, tell the BDP not to send a response
            flags |= BDP_MSG_FRAME_FLAG_NO_ACK;
        }
        else
        {
            if ((blocking == BLOCKING_ON) && (timeout_ms == 0))
            {   // blocking call and timeout 0, tell the BDP not to send a response
                flags |= BDP_MSG_FRAME_FLAG_NO_ACK;
            }
        }
        if ((blocking == BLOCKING_OFF) && (callback == 0))
        {   // non blocking call and callback 0, tell the BDP not to send a response
            flags |= BDP_MSG_FRAME_FLAG_NO_ACK;
        }
    }

    // compute the timeout for the client based on the device timeout and the retries if any
    // if throtteling or backoff are engaged than the time to send a message will increase and
    // might need to change the timeout
    timeout_client_ms = timeout_ms * (BDP_RETRY_MAX + 1);

    int64_t tstart = svsTimeGet_ms();
    logDebug("Client sending BDP %d %s %lld ms", dev_num, msgIDToString(msgID), tstart);
    // Send request to server
    rc = svsSocketSendBdp(sockFdRx, msgID, dev_num, timeout_client_ms, timeout_ms, flags, callback, req_payload, req_len);
    if (rc != ERR_PASS)
    {
        logError("svsSocketSendBdp");
        return(rc);
    }

    if (blocking == BLOCKING_OFF)
    {   // non blocking call, we are done, signal success
        return(ERR_PASS);
    }

    // For a blocking call we need some timeout value so as not to block the client in case the server fails to respond.

    // Set timeout on the socket (time to wait for the server to respond)
    if (timeout_client_ms == 0)
    {   // we need to wait for the server to let us know that it is done sending
        // in this case, the server does not wait for a response from the BDP
        // but there is still some minimum time to process
        timeout_client_ms = BDP_TIMEOUT_MIN_MS;
    }

    timeout.tv_sec  = timeout_client_ms / 1000 ;
    timeout.tv_usec = (timeout_client_ms % 1000) * 1000;
    rc = setsockopt(sockFdRx, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if (rc != 0)
    {
        logError("setsockopt: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    // Wait for response from server
    rc = svsSocketRecvMsg(sockFdRx, &hdr, rsp_payload, rsp_len);
    if (rc == ERR_PASS)
    {
        logDebug("Received BDP server response, roundtrip %lld ms", svsTimeGet_ms() - tstart);

        // validate the packet
        if (hdr.u.bdphdr.msg_id != msgID)
        {   // we should have received a message with the same msgID sent
            // check to see if that was MSG_ID_BDP_ACK
            if (hdr.u.bdphdr.msg_id == MSG_ID_BDP_ACK)
            {   // get ack status and use it as an error code
                bdp_ack_msg_rsp_t *bdp_ack_msg_rsp;
                bdp_ack_msg_rsp = (bdp_ack_msg_rsp_t *)rsp_payload;
                rc = bdp_ack_msg_rsp->status;
                logError("ACK message ID received with status %d", rc);
                return(rc);
            }
            else
            {
                logError("message ID mismatch expected %d, received %d", msgID, hdr.u.bdphdr.msg_id);
            }
        }
        // Validate the header
        if (dev_num != BDP_NUM_ALL)
        {   // when it is not a broadcast
            if (hdr.dev_num != dev_num)
            {
                logError("dev num mismatch %d %d", hdr.dev_num, dev_num);
                return(ERR_FAIL);
            }
        }
    }
    else
    {
        if (rc == ERR_COMMS_TIMEOUT)
        {
            if (timeout_client_ms == 0)
            {
                rc = ERR_PASS;
            }
            else
            {
                logWarning("Timeout waiting for BDP server");
                logDebug("No BDP server response after %lld ms", svsTimeGet_ms() - tstart);
            }
        }
        else
        {
            logError("Failed to communicate with BDP server");
        }
    }

    return(rc);
}

//
// Description:
// The client application calls this function to send the request to the server and waits for a response
//
int svsSocketServerKrTransfer(int sockFd, module_id_t module_id, uint16_t dev_num, uint16_t msgID, int timeout_ms, uint8_t flags, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int                     rc;
    struct timeval          timeout;
    svsSocketMsgHeader_t    hdr;

    if (sockFd <= 0)
        return ERR_FAIL;

    rc = svsSocketRecvFlush(sockFd);
    if ((rc != ERR_PASS) && (rc != ERR_COMMS_TIMEOUT))
    {
        logError("svsSocketRecvFlush failed");
        return(rc);
    }

    int retry = 1;
    do
    {
        // Send request to server
        rc = svsSocketSendKr(sockFd, msgID, dev_num, timeout_ms, flags, req_payload, req_len);
        if (rc != ERR_PASS)
        {
            logError("svsSocketSendKr");
            return(rc);
        }

        // Set timeout on the socket
        timeout.tv_sec  = timeout_ms / 1000 ;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        rc = setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        if (rc != 0)
        {
            logError("setsockopt: %s",  strerror(errno));
            return(ERR_FAIL);
        }

        // Wait for response from server
        rc = svsSocketRecvMsg(sockFd, &hdr, rsp_payload, rsp_len);
        if (rc == ERR_PASS)
        {
            break;
        }
        if (rc == ERR_COMMS_TIMEOUT)
        {
            logError("No response from KR, retrying %d...", retry);
            retry++;
        }
        else
        {
            logError("Failed to communicate with KR");
            return(rc);
        }
    } while(retry <= 10);

    if (rc == ERR_COMMS_TIMEOUT)
    {
        logError("Failed to communicate with KR after %d retries", retry-1);
        return(rc);
    }
    //
    // Validate the header
    //
    if (hdr.u.krhdr.msg_id != msgID)
    {
        logError("message ID mismatch expected %d, received %d", msgID, hdr.u.krhdr.msg_id);
        return(ERR_FAIL);
    }

    return(ERR_PASS);
}

int svsSocketServerBdpTransferSafe(uint16_t dev_num, bdp_msg_id_t msg_id, blocking_t blocking, int timeout_ms, uint8_t flags, callback_fn_t callback, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int rc1, rc2;
    extern pthread_mutex_t mutexSocketClientBdp;

    // Perform atomic operation
    rc1 = pthread_mutex_lock(&mutexSocketClientBdp);
    if (rc1 != 0)
    {
        logError("pthread_mutex_lock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    // Send message to the server
    rc1 = svsSocketServerBdpTransfer(svsBdpSockFdGet(), svsBdpSockFdGet(), MODULE_ID_BDP, dev_num, msg_id, blocking, timeout_ms, flags, callback, req_payload, req_len, rsp_payload, rsp_len);
    if (rc1 != ERR_PASS)
    {
        if (rc1 == ERR_SOCK_DISC)
        {
            close(svsBdpSockFdGet());
            svsBdpSockFdSet(0);
        }
        logError("");
    }

    rc2 = pthread_mutex_unlock(&mutexSocketClientBdp);
    if (rc2 != 0)
    {
        logError("pthread_mutex_unlock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    return(rc1);
}

int svsSocketServerKrTransferSafe(uint16_t dev_num, kr_msg_id_t msg_id, int timeout_ms, uint8_t flags, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int rc1, rc2;
    extern pthread_mutex_t mutexSocketClientKr;

    // Perform atomic operation
    rc1 = pthread_mutex_lock(&mutexSocketClientKr);
    if (rc1 != 0)
    {
        logError("pthread_mutex_lock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    // Send message to the server
    rc1 = svsSocketServerKrTransfer(svsKrSockFdGet(), MODULE_ID_KR, dev_num, msg_id, timeout_ms, flags, req_payload, req_len, rsp_payload, rsp_len);
    if (rc1 != ERR_PASS)
    {
        if (rc1 == ERR_SOCK_DISC)
        {
            close(svsKrSockFdGet());
            svsKrSockFdSet(0);
        }
        logError("");
    }

    rc2 = pthread_mutex_unlock(&mutexSocketClientKr);
    if (rc2 != 0)
    {
        logError("pthread_mutex_unlock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    return(rc1);
}

int svsSocketServerSvsTransferSafe(uint16_t dev_num, svs_msg_id_t msg_id, int timeout_ms, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int rc1, rc2;
    extern pthread_mutex_t mutexSocketClientSvs;

    // Perform atomic operation
    rc1 = pthread_mutex_lock(&mutexSocketClientSvs);
    if (rc1 != 0)
    {
        logError("pthread_mutex_lock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    // Send message to the server
    rc1 = svsSocketServerSvsTransfer(svsSvsSockFdGet(), MODULE_ID_SVS, dev_num, msg_id, timeout_ms, req_payload, req_len, rsp_payload, rsp_len);
    if (rc1 != 0)
    {
        if (rc1 == ERR_SOCK_DISC)
        {
            close(svsKrSockFdGet());
            svsKrSockFdSet(0);
        }
        logError("");
    }

    rc2 = pthread_mutex_unlock(&mutexSocketClientSvs);
    if (rc2 != 0)
    {
        logError("pthread_mutex_unlock: %s",  strerror(errno));
        return(ERR_FAIL);
    }

    return(rc1);
}

//
// Description:
// The client application calls this function to send the request to the server and waits for a response
//
int svsSocketServerSvsTransfer(int sockFd, module_id_t module_id, uint16_t dev_num, uint16_t msgID, int timeout_ms, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len)
{
    int                     rc;
    struct timeval          timeout;
    svsSocketMsgHeader_t    hdr;

    rc = svsSocketRecvFlush(sockFd);
    if ((rc != ERR_PASS) && (rc != ERR_COMMS_TIMEOUT))
    {
        logError("svsSocketRecvFlush failed");
        return(rc);
    }

    // Send request to server
    rc = svsSocketSendSvs(sockFd, msgID, dev_num, req_payload, req_len);
    if (rc != ERR_PASS)
    {
        logError("svsSocketSendSvs");
        return(rc);
    }

    // Set timeout on the socket
    timeout.tv_sec  = timeout_ms / 1000 ;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    rc = setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if (rc != 0)
    {
        logError("setsockopt: %s",  strerror(errno));
        return(ERR_FAIL);
    }
    // Wait for response from server
    rc = svsSocketRecvMsg(sockFd, &hdr, rsp_payload, rsp_len);
    if (rc != ERR_PASS)
    {
        logError("svsSocketRecvMsg");
        return(rc);
    }
    //logInfo("Response received: %s %d %s", hdr.appName, hdr.len, rsp_payload);

    //
    // Validate the header
    //
    if (hdr.dev_num != dev_num)
    {
        logError("dev num mismatch: expected %d received %d", dev_num, hdr.dev_num);
        return(ERR_FAIL);
    }

    if (hdr.module_id != module_id)
    {
        logError("module ID mismatch %d %d %d", sockFd, hdr.module_id, module_id);
        return(ERR_FAIL);
    }

    if (hdr.u.svshdr.id != msgID)
    {
        logError("message ID mismatch %d %d %d", sockFd, hdr.u.svshdr.id, msgID);
        return(ERR_FAIL);
    }

    rc = hdr.u.svshdr.status;

    return(rc);
}

void *svsSocketServerThread(void *arg)
{
    int rc;
    fd_set master;      // master file descriptor list
    fd_set read_fds;    // temp file descriptor list for select()
    struct sockaddr_in serveraddr;  // server address
    struct sockaddr_in clientaddr;  // client address
    int fdmax;      // maximum file descriptor number
    int listener;   // listening socket descriptor
    int newfd;      // newly accept()ed socket descriptor
    int yes = 1;    // for setsockopt() SO_REUSEADDR, below
    int addrlen;
    int i;
    socket_thread_info_t *thread_info = (socket_thread_info_t *)arg;

    //logDebug("Starting server thread: %s", thread_info->name);

#if 0    // Set max priority level
    struct sched_param sp;
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    rc = sched_setscheduler(0, SCHED_FIFO, &sp);
    if (rc != 0)
    {
        fprintf(stderr, "ERR: failed to change priority %s\n",  strerror(errno));
        exit(-1);
    }
    // Lock the process memory to avoid paging
    rc = mlockall(MCL_CURRENT|MCL_FUTURE);
    if (rc != 0)
    {
        fprintf(stderr, "ERR: mlockall() %d %s\n", rc, strerror(errno));
        exit(-1);
    }
#endif
    // clear the master and temp sets
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    // clear the sockets
    bzero(&serveraddr, sizeof(struct sockaddr_in));
    bzero(&clientaddr, sizeof(struct sockaddr_in));
    // get the listener
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Server-socket() error");
        exit(1);
    }
    //fprintf(stderr, "Server-socket() is OK...");
    // "address already in use" error message
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("Server-setsockopt() error");
        exit(1);
    }
    //fprintf(stderr, "Server-setsockopt() is OK...");
    // bind
    serveraddr.sin_family       = AF_INET;
    serveraddr.sin_port         = htons(thread_info->port);
#if 0
    serveraddr.sin_addr.s_addr  = htonl(thread_info->ipAddr);
#else

    rc = inet_pton(AF_INET, thread_info->ipAddr, &serveraddr.sin_addr);
    if (rc != 1)
    {
        if (rc < 0)
        {
            perror("error: not a valid address family");
        }
        else
        {
            perror("error: invalid network address");
        }
        close(listener);
        exit(1);
    }

#endif
    if (bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        perror("Server-bind() error");
        exit(1);
    }
    //fprintf(stderr, "Server-bind() is OK...");
    // listen
    if (listen(listener, SOMAXCONN) == -1)
    {
        perror("Server-listen() error");
        exit(1);
    }
    //fprintf(stderr, "Server-listen() is OK...");
    // add the listener to the master set
    FD_SET(listener, &master);
    if (thread_info->devFd1 > 0)
    {   // add device FD to the master
        FD_SET(thread_info->devFd1, &master);
    }
    if (thread_info->devFd2 > 0)
    {   // add device FD to the master
        FD_SET(thread_info->devFd2, &master);
    }
    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    if (thread_info->devFd1 > fdmax)
    {
        fdmax = thread_info->devFd1;
    }
    if (thread_info->devFd2 > fdmax)
    {
        fdmax = thread_info->devFd2;
    }

    // loop
    for(;;)
    {
        // copy it
        read_fds = master;

        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            logWarning("%s: select error: %s", thread_info->name, strerror(errno));
            continue;
        }

        //fprintf(stderr, "%s: Server-select() is OK...", thread_info->name);

        // run through the existing connections looking for data to be read
        for(i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {   // we got one...
                if (i == listener) // server
                {
                    // handle new connections
                    addrlen = sizeof(clientaddr);
                    if ((newfd = accept(listener, (struct sockaddr *)&clientaddr, (socklen_t *)&addrlen)) == -1)
                    {
                        perror("Server-accept() error");
                    }
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax)
                        {
                            fdmax = newfd;
                        }
                        if (thread_info->fnServer)
                        {
                            thread_info->fnServer(i, 0, 0);
                        }
                    }
                }
                else
                {
                    if (i == thread_info->devFd1)
                    {
                        if (thread_info->fnDev1)
                        {
                            rc = thread_info->fnDev1(i);
                            if (rc == ERR_FILE_DESC)
                            {
                                FD_CLR(i, &master);
                                close(i);
                                thread_info->devFd1 = 0;
                                logWarning("%s: closing file descriptor %d...no longer valid", thread_info->name, i);
                            }
                        }
                    }
                    else
                    {
                        if (i == thread_info->devFd2)
                        {
                            if (thread_info->fnDev2)
                            {
                                rc = thread_info->fnDev2(i);
                                if (rc == ERR_FILE_DESC)
                                {
                                    FD_CLR(i, &master);
                                    close(i);
                                    thread_info->devFd2 = 0;
                                    logWarning("%s: closing file descriptor %d...no longer valid", thread_info->name, i);
                                }
                            }
                        }
                        else
                        {
                            svsSocketMsgHeader_t hdr;
#if 0
                            memset(thread_info->payload, 0, thread_info->len_max);
#endif
                            // get the data from a client, but limit length to max allowed
                            rc = svsSocketRecvMsg(i, &hdr, thread_info->payload, thread_info->len_max);
                            if (rc != ERR_PASS)
                            {
                                if (rc == ERR_SOCK_DISC)
                                {
                                    /*
                                     * If we are the callback server then
                                     * make a special call to the client
                                     * handler to remove the socket from the
                                     * app registration list since the far end
                                     * connection closed. Note that the use of
                                     * NULL for parameters 2 and 3 are a special
                                     * indication to the function that this is a
                                     * deregistration operation.
                                     */
                                    if (thread_info->cbserver && thread_info->fnClient)
                                    {
                                        thread_info->fnClient(i, NULL, NULL);
                                    }
                                    
                                    FD_CLR(i, &master);
                                    close(i);
                                    logWarning("%s: closing socket %d...client disconnected", thread_info->name, i);
                                }
                            }
                            else
                            {
                                if (thread_info->fnClient)
                                {
                                    rc = thread_info->fnClient(i, &hdr, thread_info->payload);
                                    if (rc != ERR_PASS)
                                    {
                                        if (rc == ERR_SOCK_DISC)
                                        {
                                            /*
                                             * If we are the callback server then
                                             * make a special call to the client
                                             * handler to remove the socket from the
                                             * app registration list since the far end
                                             * connection closed. Note that the use of
                                             * NULL for parameters 2 and 3 are a special
                                             * indication to the function that this is a
                                             * deregistration operation.
                                             */
                                            if (thread_info->cbserver && thread_info->fnClient)
                                            {
                                                thread_info->fnClient(i, NULL, NULL);
                                            }
                                            
                                            FD_CLR(i, &master);
                                            close(i);
                                            logWarning("%s: closing socket %d...client disconnected", thread_info->name, i);
                                        }
                                    }
                                }
                            } // nbytes
                        } // client
                    }
                }
            }
        } // for
    } // for
}

int svsSocketRecvFlush(int sockFd)
{
    int rc = ERR_PASS;
    int len;
    static uint8_t buf[sizeof(svsSocketMsg_t)];

    do
    {
        len = recv(sockFd, buf, sizeof(svsSocketMsg_t), MSG_DONTWAIT);
        if (len <= 0)
        {
            if (len == 0)
            {
                logWarning("client disconnected, on socket %d: %s", sockFd, strerror(errno));
                rc = ERR_SOCK_DISC;
            }
            else
            {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                {
                    //logWarning("svsSocketRecvMsg1 timedout: %s", strerror(errno));
                    rc = ERR_COMMS_TIMEOUT;
                }
                else
                {
                    logError("recv: %s",  strerror(errno));
                    rc = ERR_SOCK_FAIL;
                    close(sockFd);
                }
            }
        }
    } while(len > 0);

    return(rc);
}

int svsSocketRecvMsg(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload, uint16_t len_max)
{
    int rc = ERR_PASS;
    int len;

    if (hdr == 0)
    {
        logError("hdr null");
        rc = ERR_FAIL;
        goto _svsSocketRecvMsg;
    }

    // First get the header
    len = recv(sockFd, hdr, sizeof(svsSocketMsgHeader_t), MSG_WAITALL);
    if (len <= 0)
    {
        if (len == 0)
        {
            logWarning("client disconnected, on socket %d", sockFd);
            rc = ERR_SOCK_DISC;
        }
        else
        {
            if (errno == EAGAIN)
            {
                rc = ERR_COMMS_TIMEOUT;
            }
            else
            {
                logError("recv1: %s",  strerror(errno));
                rc = ERR_SOCK_FAIL;
                close(sockFd);
            }
        }
        goto _svsSocketRecvMsg;
    }
    if (len != sizeof(svsSocketMsgHeader_t))
    {
        logError("length: expected %d received %d", sizeof(svsSocketMsgHeader_t), len);
        rc = ERR_FAIL;
        goto _svsSocketRecvMsg;
    }

    // Validate payload length
    if (hdr->len > len_max)
    {
        logError("length: payload length out of range %d %d, truncating message", hdr->len, len_max);
        hdr->len = len_max;
    }

    if (hdr->len > 0)
    {
        if (payload == 0)
        {
            logError("payload null");
            rc = ERR_FAIL;
            goto _svsSocketRecvMsg;
        }
        // Next get the payload
        len = recv(sockFd, payload, hdr->len, MSG_WAITALL);
        if (len <= 0)
        {
            if (len == 0)
            {
                logWarning("client disconnected, on socket %d: %s ", sockFd, strerror(errno));
                rc = ERR_SOCK_DISC;
            }
            else
            {
                if (errno == EAGAIN)
                {
                    logWarning("svsSocketRecvMsg2 timedout: %s", strerror(errno));
                    rc = ERR_COMMS_TIMEOUT;
                }
                else
                {
                    logError("recv2: %s",  strerror(errno));
                    rc = ERR_SOCK_FAIL;
                    close(sockFd);
                }
            }
            goto _svsSocketRecvMsg;
        }
        if (len != hdr->len)
        {
            logError("length: expected %d received %d", hdr->len, len);
            rc = ERR_FAIL;
            goto _svsSocketRecvMsg;
        }
    }

    _svsSocketRecvMsg:

    return(rc);
}

int svsSocketSendBdp(int sockFd, uint16_t msg_id, uint16_t dev_num, int timeout_client_ms, int timeout_ms, uint8_t flags, callback_fn_t callback, uint8_t *payload, uint16_t len)
{
    int rc;
    svsSocketMsgHeader_t hdr;

    //logDebug("");

    // Fill the header
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
    hdr.module_id   = MODULE_ID_BDP;
    hdr.dev_num     = dev_num;
    hdr.len         = len;
    hdr.timeout_ms  = timeout_client_ms;
    hdr.tsent_ms    = svsTimeGet_ms();

    //logDebug("%s moduleID %d msdID %d", hdr.appName, hdr.module_id, msg_id);

    hdr.u.bdphdr.msg_id     = msg_id;
    hdr.u.bdphdr.flags      = flags;
    hdr.u.bdphdr.callback   = callback;
    hdr.u.bdphdr.timeout_ms = timeout_ms;

    rc = svsSocketSend(sockFd, &hdr, payload);

    return(rc);
}

int svsSocketSendKr(int sockFd, uint16_t msg_id, uint16_t dev_num, int timeout_ms, uint8_t flags, uint8_t *payload, uint16_t len)
{
    int rc;
    svsSocketMsgHeader_t hdr;

    // Fill the header
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
    hdr.module_id  = MODULE_ID_KR;
    hdr.dev_num    = dev_num;
    hdr.len        = len;
    hdr.timeout_ms = timeout_ms;
    hdr.tsent_ms   = svsTimeGet_ms();

    hdr.u.krhdr.msg_id = msg_id;
    hdr.u.krhdr.flags  = flags;

    rc = svsSocketSend(sockFd, &hdr, payload);

    return(rc);
}

int svsSocketSendLog(int sockFd, log_verbosity_t verbosity, uint8_t *payload, uint16_t len)
{
    int rc;
    svsSocketMsgHeader_t hdr;

    // Fill the header
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
    hdr.module_id = MODULE_ID_LOG;
    hdr.dev_num   = 0;
    hdr.len       = len;

    hdr.u.loghdr.verbosity = verbosity;

    rc = svsSocketSend(sockFd, &hdr, payload);

    return(rc);
}

int svsSocketSendCallback(int sockFd, uint8_t module_id, uint16_t dev_num, uint16_t msg_id, callback_fn_t callback, uint8_t *payload, uint16_t len)
{
    int rc;
    svsSocketMsgHeader_t hdr;

    //logDebug("");

    // Fill the header
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
    hdr.module_id       = module_id;
    hdr.dev_num         = dev_num;
    hdr.len             = len;
    hdr.seq             = 0;

    hdr.u.cbhdr.msg_id   = msg_id;
    hdr.u.cbhdr.callback = callback;

    rc = svsSocketSend(sockFd, &hdr, payload);

    //logDebug("sent module %d dev %d seq %d", hdr.module_id, hdr.dev_num, hdr.seq);

    return(rc);
}

int svsSocketSendSvs(int sockFd, uint16_t msg_id, uint16_t dev_num, uint8_t *payload, uint16_t len)
{
    int rc;
    svsSocketMsgHeader_t hdr;

    // Fill the header
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.appName, svsAppNameGet(), SVS_SOCKET_MSG_APPNAME_MAX);
    hdr.module_id = MODULE_ID_SVS;
    hdr.len       = len;
    hdr.dev_num   = dev_num;

    //logDebug("svsSocketSendSvs %s moduleID %d msdID %d", hdr.appName, hdr.module_id, msg_id);

    hdr.u.svshdr.id = msg_id;

    rc = svsSocketSend(sockFd, &hdr, payload);

    return(rc);
}

int svsSocketSend(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;
    int flag, len;
    uint32_t static seq = 1;

    if (hdr == 0)
    {
        logError("hdr null");
        rc = ERR_FAIL;
        goto _svsSocketSend;
    }

    if (hdr->len > SVS_SOCKET_MSG_PAYLOAD_MAX)
    {
        logError("payload length out of range %d", hdr->len);
        rc = ERR_FAIL;
        goto _svsSocketSend;
    }

    if (hdr->len == 0)
    {
        flag = MSG_NOSIGNAL;
    }
    else
    {   // partial message, to hold OFF from actually sending the msg until the payload is sent
        flag = MSG_MORE | MSG_NOSIGNAL;
    }

    if ((hdr->seq == 0) && (hdr->module_id == MODULE_ID_BDP))
    {
        hdr->seq = seq;
        seq++;
    }
    //logDebug("sending module %d seq %d", hdr->module_id, hdr->seq);

    // First send the header, but hold OFF with MSG_MORE only if payload length is not 0
    len = send(sockFd, hdr, sizeof(svsSocketMsgHeader_t), flag);
    if (len < 0)
    {
        logError("send1: sockFd %d %s", sockFd, strerror(errno));
        if (errno == EPIPE)
            rc = ERR_SOCK_DISC;
        else
            rc = ERR_FAIL;
        goto _svsSocketSend;
    }
    else
    {
        if (len != sizeof(svsSocketMsgHeader_t))
        {
            logError("header partially sent %d",  len);
            rc = ERR_FAIL;
            goto _svsSocketSend;
        }
    }
    // Next send the payload
    if (hdr->len != 0)
    {
        if (payload == 0)
        {
            logError("payload null but length %d not 0", len);
            rc = ERR_FAIL;
        }
        else
        {
            len = send(sockFd, payload, hdr->len, MSG_NOSIGNAL);
            if (len < 0)
            {
                logError("send2: %s", strerror(errno));
                if (errno == EPIPE)
                    rc = ERR_SOCK_DISC;
                else
                    rc = ERR_FAIL;
            }
            else
            {
                if (len != hdr->len)
                {
                    logError("payload partially sent %d",  len);
                    rc = ERR_FAIL;
                }
            }
        }
    }

    _svsSocketSend:

    return(rc);
}


