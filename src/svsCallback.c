
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsApi.h>
#include <svsCallback.h>
#include <svsBdp.h>
#include <svsSocket.h>

//
// This is the table holding the callback functions for each of the modules and all the message IDs per module
//
static callback_fn_t        cb_bdp[MSG_ID_BDP_MAX];
static callback_fn_t        cb_kr[MSG_ID_KR_MAX];
static callback_fn_t        cb_ccr = 0;
static callback_fn_t        cb_dio = 0;
static callback_fn_t        cb_upgrade = 0;
static callback_fn_t        cb_pm = 0;
static callback_fn_t        cb_wim = 0;

static int app_fds[REGISTERED_APPS_MAX];
static int app_cnt = 0;

static socket_thread_info_t socket_thread_info;

static void *svsClientCallbackHandler(void *arg);
static int svsSocketServerCallbackHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);

int svsCallbackServerInit(void)
{
    int rc = 0;

    memset(cb_bdp, 0, sizeof(cb_bdp));
    memset(cb_kr, 0, sizeof(cb_kr));

    memset(&socket_thread_info, 0, sizeof(socket_thread_info_t));

    // configure server info
    socket_thread_info.name        = "CALLBACK";
    socket_thread_info.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info.port        = SOCKET_PORT_CALLBACK;
    socket_thread_info.devFd1      = -1;
    socket_thread_info.devFd2      = -1;
    socket_thread_info.fnServer    = 0;
    socket_thread_info.fnClient    = svsSocketServerCallbackHandler;
    socket_thread_info.fnDev1      = 0;
    socket_thread_info.fnDev2      = 0;
    socket_thread_info.fnThread    = svsSocketServerThread;
    socket_thread_info.len_max     = CALLBACK_MSG_PAYLOAD_MAX;
    socket_thread_info.cbserver    = 1;

    // create the server
    rc = svsSocketServerCreate(&socket_thread_info);
    if(rc != ERR_PASS)
    {   // XXX
        return(rc);
    }

    return(rc);
}

int svsCallbackServerUninit(void)
{
    int rc = ERR_PASS;

    return(rc);
}

pthread_t   pthread_client;    // thread pointer

//
// Called by application
// - creates client thread
// - establishes connection with callback server
//
int svsCallbackInit(void)
{
    int status;
    int rc = 0;

    memset(cb_bdp, 0, sizeof(cb_bdp));
    memset(cb_kr, 0, sizeof(cb_kr));

    // Create the thread so the client can receive the message from the callback server and calls the callback function
    status = pthread_create(&pthread_client, 0, svsClientCallbackHandler, 0);
    if(-1 == status)
    {
        logError("pthread_create: %s", strerror(errno));
        rc = ERR_FAIL;
    }

    return(rc);
}

int svsCallbackRegister(module_id_t module_id, uint16_t msg_id, callback_fn_t callback)
{
    int rc = ERR_PASS;

    switch(module_id)
    {
        case MODULE_ID_BDP:
            if(msg_id >= MSG_ID_BDP_MAX)
            {
                logError("invalid msg_id: %d", msg_id);
                rc = ERR_FAIL;
                break;
            }
            logDebug("registering fctn %p with msg ID %d module %d", callback, msg_id, module_id);
            cb_bdp[msg_id] = callback;
            break;

        case MODULE_ID_KR:
            if(msg_id >= MSG_ID_KR_MAX)
            {
                logError("invalid msg_id: %d", msg_id);
                rc = ERR_FAIL;
                break;
            }
            //logDebug("registering fctn %p with msg ID %d", callback, msg_id);
            cb_kr[msg_id] = callback;
            break;

        case MODULE_ID_CCR:
            cb_ccr = callback;
            break;

        case MODULE_ID_DIO:
            logDebug("registering fctn %p for DIO", callback);
           // cb_dio = callback;
            break;

        case MODULE_ID_UPGRADE:
            logDebug("registering fctn %p for Upgrade Manager", callback);
            cb_upgrade = callback;
            break;

        case MODULE_ID_PM:
            logDebug("registering fctn %p for Power Manager", callback);
            cb_pm = callback;
            break;

        case MODULE_ID_WIM:
            logDebug("registering fctn %p for WIM Manager", callback);
            cb_wim = callback;
            break;

        default:
            rc = ERR_FAIL;
            break;
    }

    return(rc);
}

//
// Description:
// This handler is called when the server receives request from client
//
static void *svsClientCallbackHandler(void *arg)
{
    int rc = ERR_PASS;
    int svsCallbackSockFd;
    svsSocketMsg_t msg;

    //logDebug("");

    /*
     * Setup connection with callback server. Note that this function is
     * called by applications. In order for applications to receive the
     * callback messages from the server we pass in a 1 as the second
     * parameter to indicate that we are an application and so should register
     * with the CB server after connecting. We need to do this because the
     * same function is used by server threads to connect with the CB server
     * and this flag allows us to distinguish between applications that need
     * to receive the callback messages and servers that don't.
     */
    rc = svsSocketClientCreateCallback(&svsCallbackSockFd, 1);
    if(rc != ERR_PASS)
    {
        logError("");
        exit(1);
    }

    for(;;)
    {
        // Wait for message from callback server
        rc = svsSocketRecvMsg(svsCallbackSockFd, &msg.header, msg.payload, SVS_SOCKET_MSG_PAYLOAD_MAX);
        if(rc != ERR_PASS)
        {
            if (rc == ERR_SOCK_DISC)
            {
                /*
                 * Note that this is bogus error handling but it's the best we
                 * can do with the exiting code. If a server exits the client
                 * should indeed close as we do here but then should be trying
                 * to reconnect to the server (expecting it to come back).
                 * But with the current codebase we have no way to practically
                 * do this.
                 */
                logError("callback server connection closed...exiting");
                close(svsCallbackSockFd);
                svsCallbackSockFd = 0;
                return NULL;
            }
            logError("svsSocketRecvMsg");
            continue;
        }

        //logDebug("Received cb from module %d seq %d msgid %d", msg.header.module_id, msg.header.seq, msg.header.u.cbhdr.msg_id);

        if(msg.header.len != 0)
        {
            if(msg.payload == 0)
            {
                logError("");
                continue;
            }
        }

        uint8_t msg_id  = msg.header.u.cbhdr.msg_id;
        uint8_t _msg_id = 255; // some invalid value

        // Call the corresponding callback
        switch(msg.header.module_id)
        {
            case MODULE_ID_BDP:
                if(msg_id >= MSG_ID_BDP_MAX)
                {
                    logError("invalid msg_id: %d", msg_id);
                    rc = ERR_FAIL;
                    break;
                }
                // check to see if the message had a callback from the client
                if(msg.header.u.cbhdr.callback != 0)
                {   // client had requested to override a registered callback or to use the incoming callback
                    // call the callback
                    logDebug("Calling BDP %d callback %s", msg.header.dev_num, msgIDToString(msg_id));
                    (msg.header.u.cbhdr.callback)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                    break;
                }

                //logDebug("cb_bdp fctn %p with msg ID %d module %d", cb_bdp[msg_id], msg_id, msg.header.module_id);

                // check to see if registered for all msg IDs
                if(cb_bdp[MSG_ID_BDP_ALL] != 0)
                {
                    _msg_id = MSG_ID_BDP_ALL;
                }
                else
                {
                    if(cb_bdp[msg_id] != 0)
                    {
                        _msg_id = msg_id;
                    }
                    else
                    {
                        logWarning("callback function null on module ID %d (not registered for msg ID: %d)", msg.header.module_id, msg_id);
                        break;
                    }
                }
                // call the callback
                (cb_bdp[_msg_id])(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                break;

            case MODULE_ID_KR:
                if(msg_id >= MSG_ID_KR_MAX)
                {
                    logError("invalid msg_id: %d", msg_id);
                    rc = ERR_FAIL;
                    break;
                }
                // check to see if registered for all msg IDs
                if(cb_kr[MSG_ID_KR_ALL] != 0)
                {
                    _msg_id = MSG_ID_KR_ALL;
                }
                else
                {
                    if(cb_kr[msg_id] != 0)
                    {
                        _msg_id = msg_id;
                    }
                    else
                    {
                        logWarning("callback function null on module ID %d (not registered for msg ID: %d)", msg.header.module_id, msg_id);
                        break;
                    }
                }
                // call the callback
                (cb_kr[_msg_id])(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                break;

            case MODULE_ID_CCR:
                if(cb_ccr)
                {
                    (cb_ccr)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                }
                break;

            case MODULE_ID_DIO:
                if(cb_dio)
                {
                    (cb_dio)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                }
                break;

            case MODULE_ID_UPGRADE:
                if(cb_upgrade)
                {
                    (cb_upgrade)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                }
                break;
            case MODULE_ID_PM:
                if(cb_pm)
                {
                    (cb_pm)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                }
                break;
            case MODULE_ID_WIM:
                if(cb_wim)
                {
                    (cb_wim)(msg_id, msg.header.dev_num, msg.payload, msg.header.len);
                }
                break;

            default:
                logError("invalid module ID %d with MSG ID %d", msg.header.module_id, msg_id);
                rc = ERR_FAIL;
                break;
        }
    }
}

/*
 * This function is called only from the CB server. It's primary function is
 * to receive a message from one of the other servers in svsd that needs to
 * be passed up to application space. However, it is also invoked when a
 * registration is received from an application that wants to register for
 * callback messages. Up to REGISTERED_APPS_MAX applications can register
 * for callback messages.
 */
static int svsSocketServerCallbackHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;
    int i;

    /*
     * If both hdr and payload are NULL this is an indication that the
     * socket has disconnected. If it is one of the application sockets
     * then remove it from the list.
     */
    if ((hdr == NULL) && (payload == NULL))
    {
        int i;

        logInfo("fd = %d went away...trying to remove from our list", sockFd);
        for (i = 0; i < app_cnt; ++i)
        {
            if (app_fds[i] == sockFd)
            {
                int j;

                /* This socket closed...remove and adjust */
                app_fds[i] = 0; /* In case there are no other entries */
                for (j = i + 1; j < app_cnt; ++j)
                {
                    app_fds[j - 1] = app_fds[j];
                    app_fds[j] = 0;
                }
                --app_cnt;
                logInfo("removed registered app - fd: %d, app_cnt: %d", sockFd, app_cnt);
                break;
            }
        }

        return(rc);
    }

    if (hdr == NULL)
    {
        logError("hdr is NULL");
        return(ERR_FAIL);
    }

    /*
     * If this is an application registration then save the file descriptor
     * so that we can begin forwarding callback messages.
     */
    if (hdr && (hdr->u.bdphdr.msg_id == MSG_ID_CB_APP))
    {
        if (app_cnt < REGISTERED_APPS_MAX)
        {
            app_fds[app_cnt++] = sockFd;
            logInfo("registered app with CB server - fd: %d, app_cnt: %d", sockFd, app_cnt);
        }
        else
        {
            logWarning("Maximum registered callback application exceeded...failed to register %d", sockFd);
        }
        return(rc);
    }

    /*
     * Not an identity message...that means it's a message from one of the
     * servers...forward to all registered applications.
     */
    if (app_cnt > 0)
    {
        for (i = 0; i < app_cnt; ++i)
        {
            if (app_fds[i] > 0)
            {
                rc = svsSocketSend(app_fds[i], hdr, payload);
                if (rc != ERR_PASS)
                {
                    logError("error sending to callback application");
                }
            }
            else
                logError("app_fds[%d] is not a valid file descriptor - app_cnt: %d", i, app_cnt);
        }
    }
    else
        logInfo("no registered callbacks - app_cnt: %d", app_cnt);

    return(rc);
}


