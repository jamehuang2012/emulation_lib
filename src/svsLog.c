
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#include <svsSocket.h>
#include <libSVS.h>

static socket_thread_info_t socket_thread_info;
static int log_server = 0;
static log_verbosity_t  log_verbosity = LOG_VERBOSITY_DEBUG;
static const char *log_file;
static FILE *log_fd = 0;
static uint32_t current_log_limit = (4 * 1024 * 1024);

static int svsSocketClientLogHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
static int log_svsSocketSendLog(int sockFd, log_verbosity_t verbosity, uint8_t *payload, uint16_t len);
static int log_svsSocketSend(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
static void logShow(char *payload);

int svsLogServerInit(const char *fileName)
{
    int rc = ERR_PASS;

    log_server = 1;

    memset(&socket_thread_info, 0, sizeof(socket_thread_info_t));

    // configure server info
    socket_thread_info.name        = "LOG";
    socket_thread_info.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info.port        = SOCKET_PORT_LOG;
    socket_thread_info.devFd1      = -1;
    socket_thread_info.devFd2      = -1;
    socket_thread_info.fnServer    = 0;
    socket_thread_info.fnClient    = svsSocketClientLogHandler;
    socket_thread_info.fnDev1      = 0;
    socket_thread_info.fnDev2      = 0;
    socket_thread_info.fnThread    = svsSocketServerThread;
    socket_thread_info.len_max     = LOG_MSG_PAYLOAD_MAX;

    // create the server
    rc = svsSocketServerCreate(&socket_thread_info);
    if(rc != ERR_PASS)
    {   // XXX
        return(rc);
    }

    if(fileName == 0)
    {
        fprintf(stderr, "\nWARNING: no log file specified, using default\n");
        fileName = LOG_DEFAULT_FILE_NAME;
    }
    log_fd = fopen(fileName, "a+");
    if(log_fd == 0)
    {
        fprintf(stderr, "ERR: failed to open file: %s\n", strerror(errno));
        return(ERR_FAIL);
    }
    log_file = fileName;
    return(rc);
}

int logVerbositySet(log_verbosity_t verbosity)
{
    if(verbosity >= LOG_VERBOSITY_MAX)
    {
        return(ERR_FAIL);
    }

    log_verbosity = verbosity;

    return(ERR_PASS);
}

static char *logverbositytostring(log_verbosity_t v)
{
    switch(v)
    {
        case LOG_VERBOSITY_NONE:
            return(char*)"NNE";
        case LOG_VERBOSITY_ERROR:
            return(char*)"ERR";
        case LOG_VERBOSITY_WARNING:
            return(char*)"WRN";
        case LOG_VERBOSITY_INFO:
            return(char*)"INF";
        case LOG_VERBOSITY_DEBUG:
            return(char*)"DBG";
        default:
            break;
    }
    return("VERBOSITY UNKNOWN");
}

//
// Called by client
//
void logOutput(log_verbosity_t verb, char *file, const char *fctn, int line, const char *format, ...)
{
    int rc, len;
    int size = LOG_BUF_SIZE;
    static char buf[LOG_BUF_SIZE];
    va_list ap;
    static int busy = 0;

    // prevent recursive calls
    if(log_server && busy)
    {   // disabled for now, just need to make sure the functions used in this context do not call the LOG routines
        //return;
    }
    busy = 1;

    //memset(buf, 0, LOG_BUF_SIZE);

    len = 0;

    switch(verb)
    {
        case LOG_VERBOSITY_ERROR:
        case LOG_VERBOSITY_WARNING:
        case LOG_VERBOSITY_INFO:
        case LOG_VERBOSITY_DEBUG:
            // Get UTC time
#if 1
            /*{
                time_t rawtime;
                struct tm *ptm;
                time(&rawtime);
                ptm = gmtime(&rawtime);

                len += snprintf(&buf[len], size, "[UTC: %04d-%02d-%02d %02d:%02d:%02d] ", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
            }*/
            {
                struct timeval tv;
                struct tm *tm;

                gettimeofday(&tv,0);
                tm   = localtime(&tv.tv_sec);
                len += snprintf(&buf[len], size, "[%02d-%02d-%02d %02d:%02d:%02d:%02ld] ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec/1000);
            }
#else
            {
                long ltime;
                time( &ltime );
                len += snprintf(&buf[len], size, "[UTC: %s]", asctime(gmtime( &ltime )));
            }
#endif
            buf[len-2] = ']';
            buf[len-1] = '\0';
            len--;
            break;

        default:
            break;
    }

    size = LOG_BUF_SIZE - len;

    switch(verb)
    {
        case LOG_VERBOSITY_ERROR:
        case LOG_VERBOSITY_WARNING:
        case LOG_VERBOSITY_INFO:
        case LOG_VERBOSITY_DEBUG:
            len += snprintf(&buf[len], size, "[%s: %s %s %d %s()]", logverbositytostring(verb), svsAppNameGet(), file, line, fctn);
            break;

        default:
            break;
    }

    size = LOG_BUF_SIZE - len;

    va_start(ap, format);
    len += vsnprintf(&buf[len], size, format, ap);
    va_end(ap);

    size = LOG_BUF_SIZE - len;

    switch(verb)
    {
        case LOG_VERBOSITY_ERROR:
        case LOG_VERBOSITY_WARNING:
        case LOG_VERBOSITY_INFO:
        case LOG_VERBOSITY_DEBUG:
            buf[len] = '\n';
            len++;
            buf[len] = '\0';
            len++;
            break;

        default:
            break;
    }

    int sockFd = svsLogSockFdGet();

    // Send buffer to LOG server if LOG server socket connection is available
    if((log_server == 0) && (sockFd > 0))
    {   // Send the output to the server
        rc = log_svsSocketSendLog(sockFd, verb, (uint8_t *)buf, len);
        if(rc != ERR_PASS)
        {
            if (rc == ERR_SOCK_DISC)
            {
                fprintf(stderr, "ERROR: svsLogSockFd far end disconnected...closing\n");
                close(sockFd);
                svsLogSockFdSet(0);
            }
            fprintf(stderr, "ERROR: log_svsSocketSendLog\n");
            goto _logOutput;
        }
        // Set timeout to wait on socket
        struct timeval timeout;
        int timeout_ms = 10;
        // Set timeout on the socket
        timeout.tv_sec  = timeout_ms / 1000 ;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        rc = setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        if(rc < 0)
        {
            fprintf(stderr, "setsockopt: %s",  strerror(errno));
            goto _logOutput;
        }
        // Wait for a message from the server
        svsSocketMsgHeader_t    hdr;
        rc = svsSocketRecvMsg(sockFd, &hdr, (uint8_t *)buf, LOG_BUF_SIZE);
        if (rc != ERR_PASS)
        {
            fprintf(stderr, "svsSocketRecvMsg: %s",  strerror(errno));
            if (rc == ERR_SOCK_DISC)
            {
                close(sockFd);
                svsLogSockFdSet(0);
                logWarning("%s: closing socket...far end disconnected", __func__);
            }
            goto _logOutput;
        }
        else
        {
            if(hdr.len != 0)
            {
                logShow(buf);
            }
        }
    }
    else
    {
        logShow(buf);
    }

    _logOutput:

    busy = 0;
}

//
// Description:
// This handler is called when the server receives request from client
//
static int svsSocketClientLogHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc;

    if(hdr == 0)
    {
        return(ERR_FAIL);
    }

    // The client is waiting for a response
    // this could be empty if the verbosity level is not high enough
    // otherwise the response would be the data to print on the client side

    if(hdr->u.loghdr.verbosity > log_verbosity)
    {   // Send the empty output to the client
        hdr->len = 0;
        rc = log_svsSocketSend(sockFd, hdr, 0);
        return rc;
    }

    if(payload == 0)
    {
        fprintf(stderr, "svsSocketClientLogHandler: payload null\n");
        return(ERR_FAIL);
    }
    // Send the output to the client
    rc = log_svsSocketSend(sockFd, hdr, payload);
    logShow((char *)payload);

    return rc;
}

int log_svsSocketSendLog(int sockFd, log_verbosity_t verbosity, uint8_t *payload, uint16_t len)
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

    rc = log_svsSocketSend(sockFd, &hdr, payload);

    return(rc);
}

int log_svsSocketSend(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc, flag;

    // *** DO NOT CALL LOG ROUTINES FROM THIS FUNCTION
    if(hdr == 0)
    {
        printf("%d %s: hdr null\n", __LINE__, __FUNCTION__);fflush(stdout);
        return ERR_FAIL;
    }

    if(hdr->len == 0)
    {
        flag = MSG_NOSIGNAL;
    }
    else
    {   // partial message, to hold OFF from actually sending the msg until the payload is sent
        flag = MSG_MORE | MSG_NOSIGNAL;
    }

    // First send the header, but hold OFF with MSG_MORE
    rc = send(sockFd, hdr, sizeof(svsSocketMsgHeader_t), flag);
    if(rc < 0)
    {
        fprintf(stderr, "send1: %s\n", strerror(errno));
        if (errno == EPIPE)
            return ERR_SOCK_DISC;
        return ERR_FAIL;
    }
    else
    {
        if(rc != sizeof(svsSocketMsgHeader_t))
        {
            fprintf(stderr, "header partially sent %d\n",  rc);
            return ERR_FAIL;
        }
    }
    // Next send the payload
    if(hdr->len != 0)
    {
        if(payload == 0)
        {
            fprintf(stderr, "payload null");
            return ERR_FAIL;
        }
        rc = send(sockFd, payload, hdr->len, MSG_NOSIGNAL);
        if(rc < 0)
        {
            fprintf(stderr, "send2: %s\n", strerror(errno));
            if (errno == EPIPE)
                return ERR_SOCK_DISC;
            return ERR_FAIL;
        }
        else
        {
            if(rc != hdr->len)
            {
                fprintf(stderr, "payload partially sent %d\n",  rc);
                return ERR_FAIL;
            }
        }
    }

    return ERR_PASS;
}

static void logManage(void)
{
    struct stat info;
    int status;
    char backup[100];

    if(log_fd <= 0)
        return;

    status = stat(log_file, &info);
    if(!status)
    {
        if(info.st_size < current_log_limit)
            return;
    }
    fclose(log_fd);
    log_fd = 0;
    sprintf(backup, "%s.0", log_file);
    status = rename(log_file, backup);
    log_fd = fopen(log_file, "a+");
    if(log_fd == 0)
    {
        fprintf(stderr, "ERR: failed to open file: %s\n", strerror(errno));
    }
}

void logShow(char *payload)
{
    if (payload == NULL)
    {
        fprintf(stderr, "payload null\n");
        return;
    }

    if (log_server)
    {
        logManage();
        if (log_fd != 0)
        {
            fputs(payload, log_fd);
            fflush(log_fd);
        }
    }
}


