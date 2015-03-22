#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <xmlrpc.h>
#include <xmlrpc_client.h>

#include "svsLog.h"
#include "svsErr.h"
#include "svsCommon.h"
#include "svsApi.h"
#include "svsCallback.h"
#include "svsWIM.h"
#include "svsSocket.h"

#define MAX_RECV_LEN    1024

#define SYNAPSE_OUI         "00:1C:2C:1B:26:"

struct client_entry
{
   int fd;

   TAILQ_ENTRY(client_entry) next_entry;
};

struct client_list
{
    int size;

    TAILQ_HEAD(, client_entry) head;
};

struct request_entry
{
   get_log_request_t *request;

   TAILQ_ENTRY(request_entry) next_entry;
};

struct request_list
{
    int size;

    TAILQ_HEAD(, request_entry) head;
};

static xmlrpc_env env;
static char *wimiface = "192.168.20.1";
static char *wimurl = "http://192.168.20.10:8081";
static bool monitorActive;
static struct client_list client_list;
static int svsWIMMonitorCBSocket;
static struct request_list request_list;

static void *monitorWIM(void *arg);

/***********************************************************
 * NAME: add_request_entry
 * DESCRIPTION: add a request entry to the request list.
 *
 * IN:  request list
 * OUT: none
 ***********************************************************/
static void add_request_entry(
    struct request_list *req_list,
    char *addr)
{
    struct request_entry *entry;

    entry = (struct request_entry *)calloc(1, sizeof(struct request_entry));
    if (!entry)
    {
        return;
    }
    entry->request = (get_log_request_t *)calloc(1, sizeof(get_log_request_t));
    if (!entry->request)
    {
        free(entry);
        return;
    }
    strcpy(entry->request->gpswAddr, addr);

    TAILQ_INSERT_TAIL(&req_list->head, entry, next_entry);
    req_list->size++;
}

/***********************************************************
 * NAME: del_request_entry
 * DESCRIPTION: delete a request entry from the request list
 *
 * IN:  request list, request entry to delete
 * OUT: none
 ***********************************************************/
static void del_request_entry(
    struct request_list *req_list,
    struct request_entry *entry)
{
    TAILQ_REMOVE(&req_list->head, entry, next_entry);
    req_list->size--;
    free(entry);
    /* Note: the request element will be freed later */
}

/***********************************************************
 * NAME: find_request_entry
 * DESCRIPTION: delete a request entry from the request list
 *
 * IN:  request list, GPSW address
 * OUT: none
 ***********************************************************/
static struct request_entry *find_request_entry(
    struct request_list *req_list,
    char *addr)
{
    struct request_entry *entry;

    TAILQ_FOREACH(entry, &req_list->head, next_entry)
    {
        if (strcmp(addr, entry->request->gpswAddr) == 0)
            return entry;
    }

    return NULL;
}

/***********************************************************
 * NAME: init_request_list
 * DESCRIPTION: initialize the request list
 *
 * IN:  request list
 * OUT: none
 ***********************************************************/
static void init_request_list(struct request_list *req_list)
{
    memset(req_list, 0, sizeof(struct request_list));
    TAILQ_INIT(&req_list->head);
}

/***********************************************************
 * NAME: add_client_connection
 * DESCRIPTION: add a client entry to the client list.
 *
 * IN:  client list and file descriptor
 * OUT: none
 ***********************************************************/
static void add_client_connection(
    struct client_list *client_list,
    int fd)
{
    struct client_entry *entry;

    entry = (struct client_entry *)calloc(1, sizeof(struct client_entry));
    if (!entry)
    {
        close(fd);
        return;
    }
    entry->fd = fd;
    TAILQ_INSERT_TAIL(&client_list->head, entry, next_entry);
    client_list->size++;
}

/***********************************************************
 * NAME: del_client_connection
 * DESCRIPTION: delete a client entry from the client list
 *
 * IN:  client list, client entry to delete
 * OUT: none
 ***********************************************************/
static void del_client_connection(
    struct client_list *client_list,
    struct client_entry *entry)
{
    TAILQ_REMOVE(&client_list->head, entry, next_entry);
    client_list->size--;
    close(entry->fd);
    free(entry);
}

/***********************************************************
 * NAME: init_client_list
 * DESCRIPTION: initialize the client list
 *
 * IN:  client list
 * OUT: none
 ***********************************************************/
static void init_client_list(struct client_list *client_list)
{
    memset(client_list, 0, sizeof(struct client_list));
    TAILQ_INIT(&client_list->head);
}

/***********************************************************
 * NAME: process_client_connection
 * DESCRIPTION: accept a client connection and adds an
 * entry for this client in the list.
 *
 * IN:  client file descriptor, client list
 * OUT: none
 ***********************************************************/
static void process_client_connection(
    int fd,
    struct client_list *client_list)
{
    int client;
    struct sockaddr_in client_socket;
    int len = sizeof(client_socket);

    client = accept(fd, (struct sockaddr*)&client_socket, (socklen_t*)&len);
    add_client_connection(client_list, client);
}

/***********************************************************
 * NAME: process_client_message
 * DESCRIPTION: process a message from a client
 *
 * IN:  client list, client entry
 * OUT: none
 ***********************************************************/
static void process_client_message(
    struct client_list *client_list,
    struct client_entry *client)
{
    int size = 0;
    char buffer[MAX_RECV_LEN];

    memset(buffer, 0, MAX_RECV_LEN);
    size = recv(client->fd, buffer, MAX_RECV_LEN, 0);
    switch (size)
    {
        case -1:
        {
        }
        break;

        case 0:
        {
            del_client_connection(client_list, client);
        }
        break;

        default:
        {
            int rc;

            /* Forward the data onto the client side */
            rc = svsSocketSendCallback(svsWIMMonitorCBSocket, MODULE_ID_WIM, 0, 0, 0, (uint8_t *)buffer, size);
            if (rc != ERR_PASS)
            {
                logError("svsSocketSendCallback");
            }
        }
    } /* End switch (size) */
}

static uint32_t GPSWMac2Short(char *gpsw_addr)
{
    char shortGPSWAddr[13];
    uint32_t thisAddr;

    /*
     * SCU app will send the full, colon-separated, MAC address of the GPSW
     * but the current WIM code is expecting only the last 3 octets with
     * a \x prefix before each octet so we produce the shortened address
     * here and then send to the WIM.
     */
    sprintf(shortGPSWAddr, "%c%c%c%c%c%c",
            gpsw_addr[21], gpsw_addr[22],
            gpsw_addr[18], gpsw_addr[19],
            gpsw_addr[15], gpsw_addr[16]);
    thisAddr = strtoul(shortGPSWAddr, NULL, 16);

    return thisAddr;
}

static char *Short2GPSWMac(char *short_addr)
{
    char *result = (char *)malloc(GPSW_ADDR_LEN);

    if (!result)
        return NULL;

    sprintf(result, "%s%c%c:%c%c:%c%c",
            SYNAPSE_OUI,
            toupper(short_addr[0]), toupper(short_addr[1]),
            toupper(short_addr[2]), toupper(short_addr[3]),
            toupper(short_addr[4]), toupper(short_addr[5]));

    return result;
}

int svsWIMServerInit(void)
{
    int nRetVal;
    pthread_t thread_id;
    pthread_attr_t thread_attr;

    nRetVal = pthread_attr_init(&thread_attr);
    if (nRetVal)
        goto err_fail;

    nRetVal = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    if (nRetVal)
        goto err_fail;

    nRetVal = pthread_create(&thread_id, &thread_attr, monitorWIM, NULL);
    if (nRetVal)
        goto err_fail;

    return ERR_PASS;

err_fail:
    return ERR_FAIL;
}

int svsWIMServerUninit(void)
{
    int rc = ERR_PASS;

    return rc;
}

int svsWIMInit(void)
{
    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS,
                        "WIM", "1.0", NULL, 0);

    init_request_list(&request_list);

    return 0;
}

int svsWIMUninit(void)
{
    /* Clean up our error-handling environment. */
    xmlrpc_env_clean(&env);

    /* Shutdown our XML-RPC client library. */
    xmlrpc_client_cleanup();

    return 0;
}

int svsWIMGetGPSLog(
    char *addr)
{
    xmlrpc_value *resultP;
    uint32_t gpswAddr;

    add_request_entry(&request_list, addr);

    gpswAddr = GPSWMac2Short(addr);
    resultP = xmlrpc_client_call(
                &env, wimurl, "get_gps_log", "(6)", &gpswAddr, (size_t)3);
    xmlrpc_DECREF(resultP);
    if (env.fault_occurred)
        return ERR_FAIL;

    return ERR_PASS;
}

get_log_request_t *svsProcessGPSLogRequest(
    char *gpswShortAddr,
    char *gpswGPSLog)
{
    struct request_entry *entry;
    get_log_request_t *request;
    char *gpswFullAddr;

    gpswFullAddr = Short2GPSWMac(gpswShortAddr);
    if (gpswFullAddr != NULL)
    {
        entry = find_request_entry(&request_list, gpswFullAddr);
        if (entry == NULL)
            return NULL;

        request = entry->request;
        del_request_entry(&request_list, entry);

        /* Fill in the log file information */
        strcpy(request->gpsLogPath, gpswGPSLog);

        /* Return the request info */
        return request;
    }
    return NULL;
}

static int server_init(void)
{
    int opt = 1;
    int fd;
    struct sockaddr_in server_socket;

    /* Get a tcp file descriptor */
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    /* set this so that we can rebind to it after a crash or a manual kill */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    /* Setup the tcp port information */
    memset(&server_socket, 0, sizeof(server_socket));
    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = inet_addr(wimiface);
    server_socket.sin_port = htons(SOCKET_PORT_WIM);

    /* Bind the socket */
    if (bind(fd, (struct sockaddr *)&server_socket, sizeof(server_socket)) < 0)
    {
        perror("WIM bind");
        return -1;
    }

    /* listen for up to 5 connections */
    listen (fd, 5);
    return fd;
}

static void *monitorWIM(void *arg)
{
    int rc;
    int server_fd;
    struct client_entry *client;

    /*
     * Create the server socket to listen for notifications
     * from the WIM.
     */
    server_fd = server_init();
    if (server_fd < 0)
        return arg;

    /*
     * Create the callback client, this will send the messages
     * to the callback server.
     */
    rc = svsSocketClientCreateCallback(&svsWIMMonitorCBSocket, 0);
    if (rc)
    {
        return arg;
    }

    init_client_list(&client_list);

    monitorActive = 1;

    while (monitorActive)
    {
        fd_set socket_set;
        int status;
        int track_fd;

        FD_ZERO(&socket_set);
        FD_SET(server_fd, &socket_set);
        track_fd = server_fd;

        TAILQ_FOREACH(client, &client_list.head, next_entry)
        {
            FD_SET(client->fd, &socket_set);
            if (client->fd > track_fd)
            {
                track_fd = client->fd;
            }
        }

        /* wait for activity on the file descriptors */
        status = select(track_fd + 1, &socket_set, (fd_set*)0, (fd_set*)0, NULL);
        if (status < 0)
        {
            continue;
        }

        if (FD_ISSET(server_fd, &socket_set))
        {
            process_client_connection(server_fd, &client_list);
        }

        TAILQ_FOREACH(client, &client_list.head, next_entry)
        {
            if (FD_ISSET(client->fd, &socket_set))
            {
                process_client_message(&client_list, client);
            }
        }
    }

    svsSocketClientDestroyCallback(svsWIMMonitorCBSocket);
    TAILQ_FOREACH(client, &client_list.head, next_entry)
    {
        del_client_connection(&client_list, client);
    }
    close(server_fd);

    return arg;
}
