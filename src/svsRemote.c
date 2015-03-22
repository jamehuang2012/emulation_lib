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
 *   svsRemote.c Remote Manager API Implementation for svs library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsSocket.h>

#include "tools/remote/include/common.h"
#include "xmlparser.h"

#define MAX_RECV_LEN 1024
#define MAX_VPN_TRIES   20

#define ADDRESS        "address"
#define INTERSECTION   "intersection"
#define LATITUDE       "lat"
#define LONGITUDE      "long"
#define STATIONNAME    "stxname"
#define STXID          "stationid"

#define STATION_INFO_FILE   "/usr/scu/downloads/station_info.xml"

static int station_fd;
static svs_err_t svs_err;

/*********************************************************
 * NAME: get_macaddr
 * DESCRIPTION: returns the MAC address of the specified
 * interface in string format
 * 
 * IN:  Pointer to an interface name
 * OUT: Pointer to a MAC address string
 **********************************************************/
static char *get_macaddr(char *ifname)
{
    struct ifreq ifr;
    char *macaddr;
    int sock;
    int status;
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return NULL;
    
    strcpy(ifr.ifr_name, ifname);

    status = ioctl(sock, SIOCGIFHWADDR, &ifr);
    close(sock);
    if (status < 0)
        return NULL;
    
    macaddr = malloc(MAX_STATIONID_LEN);
    if (macaddr == NULL)
        return NULL;

    sprintf(macaddr, "%02x:%02x:%02x:%02x:%02x:%02x",
            ifr.ifr_hwaddr.sa_data[0], ifr.ifr_hwaddr.sa_data[1],
            ifr.ifr_hwaddr.sa_data[2], ifr.ifr_hwaddr.sa_data[3],
            ifr.ifr_hwaddr.sa_data[4], ifr.ifr_hwaddr.sa_data[5]);
    return (macaddr);
}

/*********************************************************
 * NAME: get_ipaddr
 * DESCRIPTION: returns the IP address of the specified
 * interface in string format
 * 
 * IN:  Pointer to an interface name
 * OUT: Pointer to an IP address string
 **********************************************************/
static char *get_ipaddr(char *ifname)
{
    int sock;
    int status;
    struct ifreq ifr;
    char *ipaddr;
    struct sockaddr_in *addr;
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return NULL;
    
    memset(&ifr, 0, sizeof(struct ifreq));
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    status = ioctl(sock, SIOCGIFADDR, &ifr);
    close(sock);
    if (status < 0)
        return NULL;

    ipaddr = (char *)malloc(MAX_STATIONIPADDR_LEN);
    if (ipaddr == NULL)
        return NULL;
        
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    strcpy(ipaddr, inet_ntoa(addr->sin_addr));

    return ipaddr;
}

/*********************************************************
 * NAME: get_station_info
 * DESCRIPTION: gets the station information from the
 * downloaded file
 * 
 * IN:  Pointer to a station info data structure
 * OUT: None
 **********************************************************/
static void get_station_info(station_info_t *station_info)
{
    int fd;
    xmlDocPtr doc = NULL;
    xmlNodePtr rootElement;
    xmlNodePtr current;
    char *station_id;
    char *station_ipaddr;
    
    /* init XML library parser */
    xmlInitParser();

    /* weird but needed by client code for libxml */
    LIBXML_TEST_VERSION

    /* Allow formatting of the XML file */
    xmlKeepBlanksDefault(0);
    
    /* Open and lock the XML file */
    fd = open(STATION_INFO_FILE, O_RDWR);
    if (fd < 0)
    {
        printf("%s-%d: unable to open %s\n", __func__, __LINE__, "../station_info");
        return;
    }
    
    /* Load the specified XML file */
    doc = xmlReadFile(STATION_INFO_FILE, NULL, XML_PARSE_RECOVER); 
    if (!doc)
    {
        printf("%s-%d: xmlReadFile error\n", __func__, __LINE__);
        close(fd);
        return;
    }
    
    /* Get the root element */
    rootElement = xmlDocGetRootElement(doc);
    for (current = rootElement->children; current != NULL; current = current->next)
    {
        xmlChar *value;
        
        value = xmlNodeGetContent(current);
        if      (strcmp((char *)current->name, ADDRESS) == 0)
        {
            strcpy(station_info->station_address, (char *)value);
        }
        else if (strcmp((char *)current->name, INTERSECTION) == 0)
        {
            strcpy(station_info->station_intersection, (char *)value);
        }
        else if (strcmp((char *)current->name, LATITUDE) == 0)
        {
            strcpy(station_info->station_lat, (char *)value);
        }
        else if (strcmp((char *)current->name, LONGITUDE) == 0)
        {
            strcpy(station_info->station_lon, (char *)value);
        }
        else if (strcmp((char *)current->name, STATIONNAME) == 0)
        {
            strcpy(station_info->station_name, (char *)value);
        }
        else if (strcmp((char *)current->name, STXID) == 0)
        {
            strcpy(station_info->station_stxid, (char *)value);
        }
        xmlFree(value);
    }
    xmlFreeDoc(doc);
    close(fd);
    
    /* Now get the remaining info. */
    station_id = get_macaddr("eth1");
    if (station_id)
    {
        strcpy(station_info->station_id, station_id);
        free(station_id);
    }
    station_ipaddr = get_ipaddr("tun0");
    if (station_ipaddr)
    {
        strcpy(station_info->station_ipaddr, station_ipaddr);
        free(station_ipaddr);
    }
}

/*********************************************************
 * NAME: send_identity
 * DESCRIPTION: connects to the remote manager and sends
 * the station identity information
 * 
 * IN:  Pointer to the socket file descriptor
 * OUT: None
 **********************************************************/
static void send_identity(int fd)
{
    int status;
    msg_identity_t identity;
    
    memset(&identity, 0, sizeof(msg_identity_t));
    
    /* We're connected...send our identity to the network manager */
    identity.hdr.src = compStation;
    identity.hdr.dst = compServer;
    identity.hdr.type = msgTypeIdentity;
    
    get_station_info(&(identity.station_info));
    
    status = send(fd, &identity, sizeof(identity), 0);
    if (status < 0)
    {
        logError("failed to send identity");
        return;
    }
}

/*********************************************************
 * NAME: remote_manager_connect
 * DESCRIPTION: connects to the remote manager and sends
 * the station identity information
 * 
 * IN:  Pointer to the socket file descriptor
 * OUT: None
 **********************************************************/
static void remote_manager_connect(int *fd)
{
    int opt = 1;
    int count = 0;
    struct sockaddr_in server;
    
    if (*fd == 0)
    {
        /* Get a tcp file descriptor */
        *fd = socket(AF_INET, SOCK_STREAM, 0);
        if (*fd < 0)
        {
            logError("failed to open socket");
            *fd = 0;
            return;
        }
        
        /* Setup the tcp port information */
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        //server.sin_addr.s_addr = inet_addr("192.168.2.3");
        server.sin_addr.s_addr = inet_addr("10.0.3.1");
        //server.sin_addr.s_addr = inet_addr("10.0.1.10");
        server.sin_port = htons(REMOTE_MGR_PORT);
        
        setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        while (connect(*fd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            if (count > 5)
            {
                logError("failed to connect to the remote manager...aborting");
                close(*fd);
                *fd = 0;
                return;
            }
            ++count;
            logError("failed to connect to the remote manager...retrying");
            sleep(5);
        }
    }
    send_identity(*fd);
}

/*********************************************************
 * NAME: is_vpn_connected
 * DESCRIPTION: checks to see if the VPN client has
 * connected to the vpn server (a valid IP address is the
 * determining factor).
 * 
 * IN:  None
 * OUT: None
 **********************************************************/
static bool is_vpn_connected(void)
{
    int sock;
    int status;
    struct ifreq ifr;
    struct sockaddr_in *addr;
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return false;
    
    memset(&ifr, 0, sizeof(struct ifreq));
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "tun0", IFNAMSIZ-1);

    status = ioctl(sock, SIOCGIFADDR, &ifr);
    close(sock);
    if (status < 0)
        return false;
    
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    if (addr->sin_addr.s_addr != 0)
        return true;
    
    return false;
}

/*********************************************************
 * NAME: vpn_connect
 * DESCRIPTION: connects to the vpn server
 * 
 * IN:  None
 * OUT: bool
 **********************************************************/
static bool vpn_connect(void)
{
    int count = 0;
    
    system("/etc/init.d/openvpn start personica");
    while (1)
    {
        if (is_vpn_connected())
            return true;
        
        logDebug("no VPN server connection yet...");
        ++count;
        if (count > MAX_VPN_TRIES)
        {
            system("/etc/init.d/openvpn stop");
            return false;
        }
        sleep(1);
    }
}

/*********************************************************
 * NAME: vpn_disconnect
 * DESCRIPTION: disconnects from the vpn server
 * 
 * IN:  None
 * OUT: None
 **********************************************************/
static void vpn_disconnect(void)
{
    system("/etc/init.d/openvpn stop");
}

/*********************************************************
 * NAME: svsRemoteStatusGet
 * DESCRIPTION: this function will open a VPN connection to
 * the server and then establish a connection to the
 * remote manager, send it the identity information and wait
 * for status from the remote manager as to whether the
 * connection should remain open or not.
 * 
 * IN:  None
 * OUT: Success or error code status.
 **********************************************************/
svs_err_t *svsRemoteStatusGet(bool *stayConnected)
{
    int status;
    int retval = ERR_PASS;
    struct timeval tv;
    uint8_t buf[MAX_RECV_LEN];
    bool done = false;
    bool vpn_connected;
    
    *stayConnected = false;
    
    vpn_connected = vpn_connect();
    if (!vpn_connected)
    {
        logError("failed to connect to the VPN server");
        retval = ERR_FAIL;
        goto out;
    }
    
    remote_manager_connect(&station_fd);
    if (station_fd <= 0)
    {
        logError("failed to open server connection");
        retval = ERR_FAIL;
        goto out;
    }
    
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    setsockopt(station_fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    while (!done)
    {
        status = recv(station_fd, buf, MAX_RECV_LEN, 0);
        switch (status)
        {
            case -1:
                logError("recv error: %s", strerror(errno));
                /*
                 * If we timed out then we didn't get the response we were
                 * expecting so send our identity again.
                 */
                if (errno == EAGAIN)
                    send_identity(station_fd);
                else
                {
                    retval = ERR_FAIL;
                    done = true;
                }
            break;
        
            case 0:
                logError("remote server connection closed");
                retval = ERR_FAIL;
                done = true;
            break;
            
            default:
            {
                message_t *msg = (message_t *)buf;
                
                switch(msg->control.type)
                {
                    case msgTypeStationConfig:
                        *stayConnected = msg->config.stay_connected;
                        status -= sizeof(msg_stationconfig_t);
                        done = true;
                    break;
                    
                    default:
                        logError("unknown message from remote server");
                        retval = ERR_FAIL;
                        done = true;
                    break;
                }
            }
            break;
        }
    }
    if (!*stayConnected)
    {
        close(station_fd);
        station_fd = 0;
        vpn_disconnect();
    }

out:
    svs_err.code = retval;
    return &svs_err;
}

/*********************************************************
 * NAME: svsRemoteConnect
 * DESCRIPTION: this function will open a VPN connection
 * to the server and the establish a connection with the
 * remote manager.
 * 
 * IN:  None
 * OUT: Success or error code status.
 **********************************************************/
svs_err_t *svsRemoteConnect(void)
{
    int status;
    int retval = ERR_PASS;
    struct timeval tv;
    uint8_t buf[MAX_RECV_LEN];
    bool done = false;
    bool vpn_connected;
    
    vpn_connected = vpn_connect();
    if (!vpn_connected)
    {
        logError("failed to connect to the VPN server");
        retval = ERR_FAIL;
        goto out;
    }
    
    remote_manager_connect(&station_fd);
    if (station_fd <= 0)
    {
        logError("failed to open server connection");
        retval = ERR_FAIL;
        vpn_disconnect();
        goto out;
    }
    
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    setsockopt(station_fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    while (!done)
    {
        status = recv(station_fd, buf, MAX_RECV_LEN, 0);
        switch (status)
        {
            case -1:
                logError("recv error: %s", strerror(errno));
                
                /*
                 * If we timed out then we didn't get the response we were
                 * expecting so send our identity again.
                 */
                if (errno == EAGAIN)
                    send_identity(station_fd);
                else
                {
                    close(station_fd);
                    station_fd = 0;
                    retval = ERR_FAIL;
                    vpn_disconnect();
                    goto out;
                }
            break;
        
            case 0:
                logError("remote server connection closed");
                close(station_fd);
                station_fd = 0;
                retval = ERR_FAIL;
                vpn_disconnect();
                goto out;
            break;
            
            default:
            {
                message_t *msg = (message_t *)buf;
                
                switch(msg->control.type)
                {
                    case msgTypeStationConfig:
                        status -= sizeof(msg_stationconfig_t);
                        done = true;
                    break;
                    
                    default:
                        logError("unknown message from remote server");
                        retval = ERR_FAIL;
                        vpn_disconnect();
                        goto out;
                    break;
                }
            }
            break;
        }
    }

out:
    svs_err.code = retval;
    return &svs_err;
}

/*********************************************************
 * NAME: svsRemoteDisconnect
 * DESCRIPTION: this function closes the connection to the
 * remote manager and then close the VPN connection.
 * 
 * IN:  None
 * OUT: Success or error code status.
 **********************************************************/
svs_err_t *svsRemoteDisconnect(void)
{
    if (station_fd > 0)
    {
        close(station_fd);
        station_fd = 0;
    }
    vpn_disconnect();
    
    svs_err.code = ERR_PASS;
    return &svs_err;
}
