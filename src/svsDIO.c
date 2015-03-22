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
 * svsDIO.c: FPGA/GPIO processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ports.h"
#include "svsCommon.h"
#include "svsSocket.h"
#include "svsErr.h"
#include "svsLog.h"
#include "svsDIO.h"

#define DIO_DAEMON_NAME "/usr/scu/bin/diomanager"
#define CONNECTION_DELAY    15
#define MAX_RECV_LEN        1024

static void *dio_monitor(void *arg);
static void dio_change_state(dio_power_state_t state);

static pid_t dio_pid;
static int dio_fd;  /* Connection to the server */
static dio_power_state_t power_state;
static dio_input_state_t input_state;
static bool state_initialized = false;
static int dio_cb_socket;
static uint8_t reported_state;

static bool monitor_active = false;

int svsDIOServerInit(void)
{
    int retVal = ERR_PASS;

    /* Fork our DIO manager application */
    dio_pid = fork();
    switch (dio_pid)
    {
        case -1:
            logError("[%s] fork of DIO manager failed", __func__);
            retVal = ERR_FAIL;
        break;

        case 0:
        {
            char *prog1_argv[4];
            prog1_argv[0] = DIO_DAEMON_NAME;
            prog1_argv[1] = NULL;
            execvp(prog1_argv[0], prog1_argv);
        }
        break;

        default:
            sleep(5); /* Allow time for the DIO manager to startup */
        break;
    }

    return retVal;
}

int svsDIOServerUninit(void)
{
    if (dio_pid > 0)
        kill(dio_pid, SIGQUIT);

    return ERR_PASS;
}

int svsDIOInit(void)
{
    pthread_t thread_id;
    pthread_attr_t thread_attr;
    int retVal;

    retVal = pthread_attr_init(&thread_attr);
    if (retVal)
    {
        return -1;
    }

    retVal = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    if (retVal)
    {
        return -1;
    }

    retVal = pthread_create(&thread_id, &thread_attr, dio_monitor, NULL);
    if (retVal)
    {
        return -1;
    }

    return ERR_PASS;
}

void svsDIOUninit(void)
{
    monitor_active = 0;
}

int svsDIOGreenLEDSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.green_led != enable)
    {
        power_state.green_led = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOGreenLEDGet(void)
{
    return power_state.green_led;
}

int svsDIORedLEDSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.red_led != enable)
    {
        power_state.red_led = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIORedLEDGet(void)
{
    return power_state.red_led;
}

int svsDIOBDPBusASet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.bdp_bus_a != enable)
    {
        power_state.bdp_bus_a = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOBDPBusAGet(void)
{
    return power_state.bdp_bus_a;
}

int svsDIOBDPBusBSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.bdp_bus_b != enable)
    {
        power_state.bdp_bus_b = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOBDPBusBGet(void)
{
    return power_state.bdp_bus_b;
}

int svsDIO5VSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.supply != enable)
    {
        power_state.supply = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIO5VGet(void)
{
    return power_state.supply;
}

int svsDIOModemPowerSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.modem != enable)
    {
        power_state.modem = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOModemPowerGet(void)
{
    return power_state.modem;
}

int svsDIOWIMPowerSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.wim != enable)
    {
        power_state.wim = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOWIMPowerGet(void)
{
    return power_state.wim;
}

int svsDIOSparePowerSet(uint8_t enable)
{
    int rVal = ERR_PASS;

    if (power_state.spare != enable)
    {
        power_state.spare = enable;
        dio_change_state(power_state);
    }
    return rVal;
}

uint8_t svsDIOSparePowerGet(void)
{
    return power_state.spare;
}

static void dio_change_state(dio_power_state_t state)
{
    int status;
    dio_change_t msg;

    if (dio_fd > 0)
    {
        msg.hdr.type = dioTypeChange;
        msg.power = state;

        status = send(dio_fd, &msg, sizeof(msg), 0);
        if (status < 0)
        {
            logError("[%s] send() returned %d", __func__, status);
        }
    }
    else
        logError("[%s] dio_fd invalid: %d", __func__, dio_fd);
}

static int dio_connect(void)
{
    struct sockaddr_in addr;
    int status;
    int fd;
    dio_header_t msg;

    msg.type = dioTypeState;

    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {
        logError("[%s] socket create failed\n", __func__);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(SOCKET_PORT_DIO_MGR);

    while(1)
    {
        status = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (status < 0)
        {
            logError("[%s] connection to DIO manager failed...waiting %d seconds before next attempt", __func__, CONNECTION_DELAY);
            sleep(CONNECTION_DELAY);
            continue;
        }

        logInfo("[%s] DIO manager socket opened (%d).", __func__, fd);

        /* Send query to get the current state */
        status = send(fd, &msg, sizeof(msg), 0);
        if (status < 0)
        {
            logError("[%s] send() returned %d", __func__, status);
        } 
        break;
    }

    return fd;
}

#ifdef DEBUG
static void dump_state(void)
{
    printf("power_state.green_led: %s\n", power_state.green_led ? "On":"Off");
    printf("power_state.red_led:   %s\n", power_state.red_led ? "On":"Off");
    printf("power_state.bdp_bus_a: %s\n", power_state.bdp_bus_a ? "On":"Off");
    printf("power_state.bdp_bus_b: %s\n", power_state.bdp_bus_b ? "On":"Off");
    printf("power_state.supply:    %s\n", power_state.supply ? "On":"Off");
    printf("power_state.modem:     %s\n", power_state.modem ? "On":"Off");
    printf("power_state.wim:       %s\n", power_state.wim ? "On":"Off");
    printf("power_state.spare:     %s\n", power_state.spare ? "On":"Off");
    printf("\n");
    printf("input_state.door:      %s\n", input_state.door ? "Closed":"Open");
    printf("input_state.spare:     %s\n", input_state.spare ? "Closed":"Open");
    printf("input_state.pb:        %s\n", input_state.pb ? "Closed":"Open");
}
#endif

static int get_msg_len(dio_message_t *msg)
{
    switch (msg->control.type)
    {
        case dioTypeIdentity:
            return sizeof(dio_header_t);
        break;
        case dioTypeState:
            return sizeof(dio_state_t);
        break;
        default:
            logError("[%s] invalid message type", __func__);
        break;
    }
    return 0;
}

static uint8_t gpio_changed(dio_input_state_t current_state, dio_input_state_t new_state, uint8_t *state)
{
    uint8_t changeDetected = 0;

    if (current_state.door != new_state.door)
    {
        if (new_state.door)
            *state &= ~DOOR_OPEN;
        else
            *state |= DOOR_OPEN;
        changeDetected = 1;
        logInfo("[%s] door state changed: %02x", __func__, state);
    }

    if (current_state.spare != new_state.spare)
    {
        if (new_state.door)
            *state &= ~SPARE_ACTIVE;
        else
            *state |= SPARE_ACTIVE;
        changeDetected = 1;
        logInfo("[%s] spare state changed: %02x", __func__, state);
    }

    if (current_state.pb != new_state.pb)
    {
        if (new_state.door)
            *state &= ~TPB_ACTIVE;
        else
            *state |= TPB_ACTIVE;
        changeDetected = 1;
        logInfo("[%s] button state changed: %02x", __func__, state);
    }

    return changeDetected;
}

static void process_dio_message(int *fd)
{
    int size;
    int pos;
    char buffer[MAX_RECV_LEN];

    memset(buffer, 0, MAX_RECV_LEN);
    size = recv(*fd, buffer, MAX_RECV_LEN, 0);
    pos = 0;
    switch (size)
    {
        case -1:
        {
            logError("[%s] select error: %s", __func__, strerror(errno));
        }
        break;

        case 0:
        {
            logError("[%s] Server connection closed... trying to reconnect", __func__);
            /* Server connection closed...try to reconnect */
            close(*fd);
            *fd = 0;
            *fd = dio_connect();
        }
        break;

        default:
        {
            while (size > 0)
            {
                dio_message_t *msg = (dio_message_t *)&buffer[pos];

                switch (msg->control.type)
                {
                    case dioTypeIdentity:
                    case dioTypeChange:
                    break;
                    case dioTypeState:
                        if (!state_initialized)
                        {
                            /*
                             * This is the first message we've gotten from the
                             * DIO manager so we'll use this to capture the
                             * current GPIO state.
                             */
                            power_state = msg->state.power;
                            input_state = msg->state.input;
                            state_initialized = true;
                            logInfo("[%s] DIO state initialized - power: %02x, input: %02x", __func__, power_state, input_state);
                        }
                        else
                        {
                            /*
                             * We don't care about power state since the API
                             * supports both get's and set's. However, changes
                             * in the button or door state are propagated up
                             * asynchronously so we need to detect for changes
                             * here and report any changes to the application.
                             */
                            if (gpio_changed(input_state, msg->state.input, &reported_state))
                            {
                                int rc;

                                rc = svsSocketSendCallback(dio_cb_socket, MODULE_ID_DIO, 0, 0, 0, &reported_state, sizeof(reported_state));
                                if (rc != ERR_PASS)
                                {
                                    logError("[%s] svsSocketSendCallback() failed - rc: %d", __func__, rc);
                                }
                            }
                            power_state = msg->state.power;
                            input_state = msg->state.input;
                        }
#ifdef DEBUG
                        logInfo("[%s] dioTypeState message received: %d", __func__, msg->control.type);
                        dump_state();
#endif
                    break;
                    default:
                        logError("[%s] invalid message type: %d", __func__, (int)msg->control.type);
                    break;
                }
                size -= get_msg_len(msg);
                pos += get_msg_len(msg);
            }
        }
    } /* End switch (size) */
}

static void *dio_monitor(void *arg)
{
    int rc;

    dio_fd = dio_connect();
    if (dio_fd < 0)
    {
        logError("[%s] failed to connect to DIO manager", __func__);
        return arg;
    }

    rc = svsSocketClientCreateCallback(&dio_cb_socket, 0);
    if (rc)
    {
        logError("[%s] failed to connect to callback manager", __func__);
        return arg;
    }

    logInfo("[%s] DIO client initializated", __func__);

    monitor_active = true;

    while (monitor_active)
    {
        fd_set read_fds;
        int status;

        FD_ZERO(&read_fds);

        /* Add the server sockets to the list */
        FD_SET(dio_fd, &read_fds);

        /* Wait for activity on the file descriptors */
        status = select(dio_fd + 1, &read_fds, (fd_set *)0, (fd_set *)0, NULL);
        if (status < 0)
        {
            /* select() returned an error... continuing processing */
            continue;
        }

        if (status == 0)
        {
            /* Timeout... */
            continue;
        }

        /* Check to see if a message is coming from the server */
        if (FD_ISSET(dio_fd, &read_fds))
        {
            logInfo("[%s] Message received from %d. Calling process_dio_message()", __func__, dio_fd);
            process_dio_message(&dio_fd);
        }
    }

    return arg;
}
