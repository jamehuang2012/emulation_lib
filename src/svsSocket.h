#ifndef SVS_SOCKET_H
#define SVS_SOCKET_H

#include <svsCommon.h>
#include <svsLog.h>
#include <svsCallback.h>
#include <svsApi.h>
#include <svsBdp.h>
#include <svsKr.h>
#include <libSVS.h>
#include <svsSocket.h>
#include "ports.h"

#define SVS_SOCKET_SERVER_LISTEN_IP    "0.0.0.0"    // INADDR_ANY
#define SVS_SOCKET_SERVER_IP           "127.0.0.1"  // LOCAL HOST

#define SVS_SOCKET_MSG_PAYLOAD_MAX     (1024)
#define SVS_SOCKET_MSG_APPNAME_MAX     (32)

typedef enum
{
    SOCKET_SERVER_LOG,
    SOCKET_SERVER_CALLBACK,
    SOCKET_SERVER_BDP,
    SOCKET_SERVER_KR,   // key reader
    SOCKET_SERVER_WIM,  //
    SOCKET_SERVER_CCR,  // credit card reader
    SOCKET_SERVER_PRT,  // printer
    SOCKET_SERVER_SCM,  //
    SOCKET_SERVER_SVS,  //

    SOCKET_SERVER_MAX   // keep last
} socket_server_t;

typedef struct
{
    char        appName[SVS_SOCKET_MSG_APPNAME_MAX];
    uint8_t     module_id;          // module ID needed to select appropriate header union below
    uint16_t    dev_num;            // device number
    uint16_t    len;                // payload length
    uint32_t    seq;                // sequence number (internally used within SVSD to track/debug socket frames)
    int64_t     tsent_ms;           // time message sent
    int64_t     timeout_ms;         // timeout value in ms associated with sending and receiving a message to a device
    union
    {
        svsMsgBdpHeader_t       bdphdr;
        svsMsgKrHeader_t        krhdr;
        svsMsgLogHeader_t       loghdr;
        svsMsgSvsHeader_t       svshdr;
        svsMsgCallbackHeader_t  cbhdr;
    } u;
} svsSocketMsgHeader_t;

typedef struct
{
    svsSocketMsgHeader_t    header;
    uint8_t                 payload[SVS_SOCKET_MSG_PAYLOAD_MAX];
} svsSocketMsg_t;

typedef int (* msg_hdlr_fn_t)(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
typedef int (* msg_dev_hdlr_fn_t)(int devFd);
typedef void *(* thread_fn_t)(void *arg);

typedef struct
{
    char                *name;      // thread name
    char                *ipAddr;    // IP address where packets are expected to come from (typically INADDR_ANY or 127.0.0.1)
    int                 port;       // port to listen on
    int                 devFd1;     // the device FD (serial, USB...), set to -1 if not used
    int                 devFd2;     // the device FD (serial, USB...), set to -1 if not used
    msg_hdlr_fn_t       fnServer;   // server function handler
    msg_hdlr_fn_t       fnClient;   // client function handler
    msg_dev_hdlr_fn_t   fnDev1;     // device function handler
    msg_dev_hdlr_fn_t   fnDev2;     // device function handler
    thread_fn_t         fnThread;   // thread function
    pthread_t           pthread;    // thread pointer
    uint8_t             *payload;   // payload buffer when receiving data from client
    uint16_t            len_max;    // maximum payload buffer length
    uint8_t             cbserver;   // when set to 1 identifies the callback server thread
} socket_thread_info_t;

extern void *svsSocketServerThread(void *arg);

int svsSocketServerCreateAll(void);

int svsSocketClientCreateSvs(int *socketFd);
int svsSocketClientDestroySvs(int socketFd);

int svsSocketClientCreateLog(int *socketFd);
int svsSocketClientDestroyLog(int socketFd);

int svsSocketClientCreateBdp(int *socketFd);
int svsSocketClientDestroyBdp(int socketFd);

int svsSocketClientCreateKr(int *socketFd);
int svsSocketClientDestroyKr(int socketFd);

int svsSocketClientCreateCallback(int *socketFd, uint8_t appflag);
int svsSocketClientDestroyCallback(int socketFd);

int svsSocketServerCreateBdp(void);
int svsSocketServerDestroyBdp(void);

int svsSocketClientCreate(char *ipAddr, int port, int *sockFd, uint8_t appflag);
int svsSocketServerCreate(socket_thread_info_t *thread_info);
int svsSocketServerDestroy(socket_thread_info_t *thread_info);
int svsSocketClientDestroy(int socketFd);

int svsSocketRecvMsg(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload, uint16_t len);
int svsSocketSend(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);

int svsSocketSendBdp(int sockFd, uint16_t msg_id, uint16_t dev_num, int timeout_client_ms, int timeout_ms, uint8_t flags, callback_fn_t callback, uint8_t *payload, uint16_t len);
int svsSocketSendKr(int sockFd, uint16_t msg_id, uint16_t dev_num, int timeout_ms, uint8_t flags, uint8_t *payload, uint16_t len);
int svsSocketSendLog(int sockFd, log_verbosity_t verbosity, uint8_t *payload, uint16_t len);
int svsSocketSendCallback(int sockFd, uint8_t module_id, uint16_t dev_num, uint16_t msg_id, callback_fn_t callback, uint8_t *payload, uint16_t len);
int svsSocketSendSvs(int sockFd, uint16_t msg_id, uint16_t dev_num, uint8_t *payload, uint16_t len);

int svsSocketServerSvsTransfer(int sockFd, module_id_t module_id, uint16_t dev_num, uint16_t msgID, int timeout_ms, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len);
int svsSocketServerSvsTransferSafe(uint16_t dev_num, svs_msg_id_t msg_id, int timeout_ms, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len);

int svsSocketServerBdpTransferSafe(uint16_t dev_num, bdp_msg_id_t msg_id, blocking_t blocking, int timeout_ms, uint8_t flags, callback_fn_t callback, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len);
int svsSocketServerKrTransferSafe(uint16_t dev_num, kr_msg_id_t msg_id, int timeout_ms, uint8_t flags, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len);

#endif // SVS_SOCKET_H
