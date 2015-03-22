
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsApi.h>
#include <svsCallback.h>
#include <svsBdp.h>
#include <svsKr.h>
#include <svsSocket.h>
#include <svsConfig.h>
#include <crc.h>

#define KR_SIMULATOR            0

static kr_dev_info_t            kr_dev_info;
static kr_state_t               kr_state[KR_MAX];
static socket_thread_info_t     socket_thread_info;
static int svsCallbackSockFd    = -1;

static int svsSocketClientKrHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
#if KR_SIMULATOR
static int svsSocketClientKrSimHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
#endif
static int svsSocketClientKrDev1Handler(int devFd);
static int svsKrTx(int sockFd, uint8_t bus, svsSocketMsgHeader_t *hdr, uint8_t *payload);
static int svsKrRx(int devFd, uint8_t bus);

int svsKrServerInit(void)
{
    int rc;
    int i, devFd;

    memset(kr_state, 0, sizeof(kr_state_t) * KR_MAX);
    memset(&socket_thread_info, 0, sizeof(socket_thread_info_t));
    memset(&kr_dev_info, 0, sizeof(kr_dev_info_t));

    kr_dev_info.seq_num = 1;

    svsConfigModuleParamStrGet("kr", "devname", KR_DEV_DEFAULT_NAME1, kr_dev_info.kr_bus_dev_info[0].devName, KR_BUS_DEV_NAME_MAX);
    svsConfigParamIntGet("kr", "baudrate", KR_DEV_DEFAULT_BAUDRATE, &kr_dev_info.kr_bus_dev_info[0].baudRate);

    kr_dev_info.kr_bus_dev_info[0].devFd    = -1;

    svsConfigParamIntGet("kr", "loopback", 0, &kr_dev_info.loopback_enable);
    if(kr_dev_info.loopback_enable)
    {
        logInfo("loopback enabled");
    }

    crc16_init();

    //
    // Open and configure the serial ports
    //
    devFd = svsPortOpen(kr_dev_info.kr_bus_dev_info[0].devName, kr_dev_info.kr_bus_dev_info[0].baudRate);
    if(devFd < 0)
    {
        logError("");
    }
    else
    {
        kr_dev_info.kr_bus_dev_info[0].devFd = devFd;
    }

    // configure KR server info
    socket_thread_info.name        = "KR";
    socket_thread_info.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info.port        = SOCKET_PORT_KR;
    socket_thread_info.devFd1      = kr_dev_info.kr_bus_dev_info[0].devFd;
    socket_thread_info.devFd2      = kr_dev_info.kr_bus_dev_info[1].devFd;
    socket_thread_info.fnServer    = 0;
#if KR_SIMULATOR
    socket_thread_info.fnClient    = svsSocketClientKrSimHandler;
#else
    socket_thread_info.fnClient    = svsSocketClientKrHandler;
#endif
    socket_thread_info.fnDev1      = svsSocketClientKrDev1Handler;
    socket_thread_info.fnDev2      = 0;
    socket_thread_info.fnThread    = svsSocketServerThread;
    socket_thread_info.len_max     = KR_MSG_PAYLOAD_MAX;

    // create the KR server
    rc = svsSocketServerCreate(&socket_thread_info);
    if(rc != ERR_PASS)
    {
        logError("failed to start KR server");
        return(rc);
    }
    // create the Callback client, this will send the messages to the callback server
    rc = svsSocketClientCreateCallback(&svsCallbackSockFd, 0);
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    //
    // Populate the req/rsp structure addresses for all message IDs
    //
    for(i=0;i<KR_MAX;i++)
    {
        kr_state[i].id_data_addr[MSG_ID_KR_SN_SET].req = &kr_state[i].kr_sn_set_req;
        kr_state[i].id_data_addr[MSG_ID_KR_SN_SET].rsp = &kr_state[i].kr_sn_set_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_SN_SET].req_len = sizeof(kr_state[i].kr_sn_set_req);
        kr_state[i].id_data_addr[MSG_ID_KR_SN_SET].rsp_len = sizeof(kr_state[i].kr_sn_set_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_SN_GET].req = &kr_state[i].kr_sn_get_req;
        kr_state[i].id_data_addr[MSG_ID_KR_SN_GET].rsp = &kr_state[i].kr_sn_get_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_SN_GET].req_len = sizeof(kr_state[i].kr_sn_get_req);
        kr_state[i].id_data_addr[MSG_ID_KR_SN_GET].rsp_len = sizeof(kr_state[i].kr_sn_get_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_POWER_SET].req = &kr_state[i].kr_power_set_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_SET].rsp = &kr_state[i].kr_power_set_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_SET].req_len = sizeof(kr_state[i].kr_power_set_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_SET].rsp_len = sizeof(kr_state[i].kr_power_set_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_POWER_GET].req = &kr_state[i].kr_power_get_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_GET].rsp = &kr_state[i].kr_power_get_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_GET].req_len = sizeof(kr_state[i].kr_power_get_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_POWER_GET].rsp_len = sizeof(kr_state[i].kr_power_get_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_ECHO].req = &kr_state[i].kr_echo_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_ECHO].rsp = &kr_state[i].kr_echo_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_ECHO].req_len = sizeof(kr_state[i].kr_echo_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_ECHO].rsp_len = sizeof(kr_state[i].kr_echo_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_RFID_GET].req = &kr_state[i].kr_rfid_get_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_RFID_GET].rsp = &kr_state[i].kr_rfid_get_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_RFID_GET].req_len = sizeof(kr_state[i].kr_rfid_get_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_RFID_GET].rsp_len = sizeof(kr_state[i].kr_rfid_get_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_CHANGE_MODE].req = &kr_state[i].kr_change_mode_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_CHANGE_MODE].rsp = &kr_state[i].kr_change_mode_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_CHANGE_MODE].req_len = sizeof(kr_state[i].kr_change_mode_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_CHANGE_MODE].rsp_len = sizeof(kr_state[i].kr_change_mode_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_GET_MODE].req = &kr_state[i].kr_get_mode_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_MODE].rsp = &kr_state[i].kr_get_mode_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_MODE].req_len = sizeof(kr_state[i].kr_get_mode_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_GET_MODE].rsp_len = sizeof(kr_state[i].kr_get_mode_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND].req = &kr_state[i].kr_firmware_upgrade_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND].rsp = &kr_state[i].kr_firmware_upgrade_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND].req_len = sizeof(kr_state[i].kr_firmware_upgrade_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND].rsp_len = sizeof(kr_state[i].kr_firmware_upgrade_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_ACK].rsp = &kr_state[i].kr_ack_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_ACK].rsp_len = sizeof(kr_state[i].kr_ack_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_ADDR_GET].req = &kr_state[i].kr_address_get_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_ADDR_GET].rsp = &kr_state[i].kr_address_get_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_ADDR_GET].req_len = sizeof(kr_state[i].kr_address_get_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_ADDR_GET].rsp_len = sizeof(kr_state[i].kr_address_get_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_GET_BOOTBLOCK_REV].req = &kr_state[i].kr_get_bb_rev_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_BOOTBLOCK_REV].rsp = &kr_state[i].kr_get_bb_rev_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_BOOTBLOCK_REV].req_len = sizeof(kr_state[i].kr_get_bb_rev_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_GET_BOOTBLOCK_REV].rsp_len = sizeof(kr_state[i].kr_get_bb_rev_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_GET_APPLICATION_REV].req = &kr_state[i].kr_get_app_rev_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_APPLICATION_REV].rsp = &kr_state[i].kr_get_app_rev_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_GET_APPLICATION_REV].req_len = sizeof(kr_state[i].kr_get_app_rev_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_GET_APPLICATION_REV].rsp_len = sizeof(kr_state[i].kr_get_app_rev_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD].req = &kr_state[i].kr_bootblock_upload_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD].rsp = &kr_state[i].kr_bootblock_upload_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD].req_len = sizeof(kr_state[i].kr_bootblock_upload_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD].rsp_len = sizeof(kr_state[i].kr_bootblock_upload_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL].req = &kr_state[i].kr_bootblock_install_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL].rsp = &kr_state[i].kr_bootblock_install_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL].req_len = sizeof(kr_state[i].kr_bootblock_install_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL].rsp_len = sizeof(kr_state[i].kr_bootblock_install_msg_rsp);

        kr_state[i].id_data_addr[MSG_ID_KR_MEMORY_CRC].req = &kr_state[i].kr_memory_crc_msg_req;
        kr_state[i].id_data_addr[MSG_ID_KR_MEMORY_CRC].rsp = &kr_state[i].kr_memory_crc_msg_rsp;
        kr_state[i].id_data_addr[MSG_ID_KR_MEMORY_CRC].req_len = sizeof(kr_state[i].kr_memory_crc_msg_req);
        kr_state[i].id_data_addr[MSG_ID_KR_MEMORY_CRC].rsp_len = sizeof(kr_state[i].kr_memory_crc_msg_rsp);
    }

    // Verify the length of the structures, it should not exceed the maximum payload size of the KR
    int j;
    for(j=0;j<MSG_ID_KR_MAX;j++)
    {
        if(kr_state[0].id_data_addr[j].req_len > KR_MSG_PAYLOAD_MAX)
        {
            logError("req structure too large for msg ID: %d", j);
            exit(1);
        }
        if(kr_state[0].id_data_addr[j].rsp_len > KR_MSG_PAYLOAD_MAX)
        {
            logError("rsp structure too large for msg ID: %d", j);
            exit(1);
        }
    }

    // set the preamble
    memset(&kr_dev_info.preamble.preamble[0], 0xAA, KR_FRAME_PREAMBLE_SIZE);
    // set the start of frame byte
    kr_dev_info.preamble.sof = KR_FRAME_SOF;

    // set the broadcast address
    memset(&kr_dev_info.addr_broadcast, 0xFF, KR_MSG_ADDR_LENGTH);

    return(rc);
}

int svsKrServerUninit(void)
{
    int rc = ERR_PASS;

    close(kr_dev_info.kr_bus_dev_info[0].devFd);
    close(kr_dev_info.kr_bus_dev_info[1].devFd);

    return(rc);
}

//
// Description:
// This handler is called when KR server receives request from client
//
static int svsSocketClientKrHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;

    //logDebug("length %d", hdr->len);

    // Validate msg ID
    if(hdr->u.krhdr.msg_id >= MSG_ID_KR_MAX)
    {
        logError("invalid msg ID");
        return(ERR_FAIL);
    }
    // Validate device number
    if(hdr->dev_num != KR_NUM_ALL)
    {   // not a broadcast
        if(hdr->dev_num >= svsKrMaxGet())
        {
            logError("invalid device number %d", hdr->dev_num);
            return(ERR_FAIL);
        }
    }

    int i, start, end;
    // Update request field
    if(hdr->dev_num == KR_NUM_ALL)
    {
        start = 0;
        end   = svsKrMaxGet();
    }
    else
    {
        start = hdr->dev_num;
        end   = hdr->dev_num + 1;
    }
    for(i=start; i<end; i++)
    {
        if(kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req != 0)
        {   // Validate length, in case application has a mismatch with server
            if(hdr->len == kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req_len)
            {
                memcpy(kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req, payload, hdr->len);
            }
            else
            {
                logError("req length mismatch");
            }
        }
        else
        {
            logError("req not initialized");
        }
    }

    logInfo("Sending message to KR protocol");

    // send message to KR protocol
    rc = svsKrTx(sockFd, 0, hdr, payload);
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    return(rc);
}

//
// Description:
// Called when data becomes available to be read from the device KR protocol
//
int svsSocketClientKrDev1Handler(int devFd)
{
    return(svsKrRx(devFd, 0));
}

//
// Convert the virtual address to physical
//
int svsKrVirtualToPhysical(uint8_t index, kr_addr_t *addr)
{
    if(addr == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }
    if((index >= svsKrMaxGet()) && (index != KR_NUM_ALL))
    {
        logError("index out of range");
        return(ERR_FAIL);
    }

    if(index == KR_NUM_ALL)
    {   // broadcast address
        memcpy(addr, &kr_dev_info.addr_broadcast, sizeof(kr_addr_t));
    }
    else
    {
        memcpy(addr, &kr_state[index].kr_addr, sizeof(kr_addr_t));
    }

    return(ERR_PASS);
}

//
// Convert the physical address to virtual
//
int svsKrPhysicalToVirtual(kr_addr_t *addr, uint8_t *index)
{
    int rc = ERR_FAIL;    // assume not found
    uint8_t i;

    if(addr == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }
    if(index == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }

    /* NSJ - Added for debugging KR read failure */
    for(i=0; i<KR_MSG_ADDR_LENGTH; i++)
    {
        logInfo("Input: %02X | Table: %02X", addr->octet[i], kr_state[0].kr_addr.octet[i]);
    }
    /* END debug */

    // check to see for invalid zero address
    kr_addr_t addr_tmp;
    memset(&addr_tmp, 0, sizeof(kr_addr_t));
    if(svsKrAddrMatch(addr, &addr_tmp) == ERR_PASS)
    {
        logError("physical address {0,0,0,0,0,0} not allowed");
        return(ERR_FAIL);
    }
    // check to see for invalid broadcast address
    if(svsKrAddrMatch(addr, &kr_dev_info.addr_broadcast) == ERR_PASS)
    {
        logError("physical address BROADCAST not allowed");
        return(ERR_FAIL);
    }

    /* njozwiak@mlsw.biz -- 12/11/13
     * The following section compares the read address from the key
     * to the data stored in the ARP table. This was implemented to
     * mimic the BDPs functionality. As there is only 1 key reader
     * on a given Kiosk the ARP table is a length of 1 and does not
     * need to be a table. A bug was discovered where a key reader
     * would not respond to initialization properly and so the table
     * would contain 0 values. To resolve this, since the table is
     * not necessary to functionality. The following comparison has
     * been disabled. The code is being left until the key reader
     * processing has been thoroughly reviewed and fixed to remove
     * any unnecessary functionality and resolve initialization
     * issues.
     */
#if 0
    // lookup index
    for(i=0; i<KR_MAX; i++)
    {
        if(svsKrAddrMatch(&kr_state[i].kr_addr, addr) == ERR_PASS)
        {
            *index = i;
            rc = ERR_PASS;
            break;
        }
    }

    if(rc != ERR_PASS)
    {
        logWarning("physical address not found: ");
        for(i=0; i<KR_MSG_ADDR_LENGTH; i++)
        {
            logDebug("%02X ", addr->octet[i]);
        }
    }
#else
    rc = ERR_PASS;
#endif

    return(rc);
}

int svsKrAddrMatch(kr_addr_t *addr1, kr_addr_t *addr2)
{
    // returns 0 if identical
    if(memcmp(addr1->octet, addr2->octet, KR_MSG_ADDR_LENGTH) == 0)
    {
        return(ERR_PASS);
    }
    return(ERR_FAIL);
}

int svsKrArpTableUpdate(uint8_t bus, kr_addr_t *addr)
{
    int rc = ERR_PASS;
    int i;
    int found = 0;

    if(addr == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }

    // check to see for invalid zero address
    kr_addr_t addr_tmp;
    memset(&addr_tmp, 0, sizeof(kr_addr_t));
    if(svsKrAddrMatch(addr, &addr_tmp) == ERR_PASS)
    {
        logError("physical address {0,0,0,0,0,0} not allowed");
        return(ERR_FAIL);
    }
    // check to see for invalid broadcast address
    // occurs when SCU sends a BROADCAST
    if(svsKrAddrMatch(addr, &kr_dev_info.addr_broadcast) == ERR_PASS)
    {
        logError("physical address BROADCAST not allowed");
        return(ERR_FAIL);
    }

    // Make sure the new address does not exist
    for(i=0; i<kr_dev_info.kr_max; i++)
    {
        if(memcmp(addr->octet, &kr_state[i].kr_addr.octet, sizeof(KR_MSG_ADDR_LENGTH)) == 0)
        {   // address already in the table
            found = 1;
            logInfo("KR %d address already in the ARP table", i);
            break;
        }
    }

    if(found == 0)
    {
        // Check to see if the maximum allowed has been reached
        if(kr_dev_info.kr_max >= KR_MAX)
        {
            logError("KR number exceeded limit");
            return(ERR_FAIL);
        }

        // Validate address, should not be all 0s nor FFs XXX

        // Address not found in the ARP table, update the ARP table with the new address
        memcpy(&kr_state[i].kr_addr, addr, sizeof(kr_addr_t));
        kr_state[i].kr_bus = bus;
        // Update KR index
        kr_dev_info.kr_max++;
        kr_dev_info.kr_bus_dev_info[bus].kr_max++;
        logInfo("KR %d address updated into ARP table", i);
        logInfo("Total KR detected: %d", kr_dev_info.kr_max);
    }

    return(rc);
}

//
// Return the total number of physical KRs detected
//
uint8_t svsKrMaxGet(void)
{
    return(kr_dev_info.kr_max);
}

//
// Description:
// Read bytes from device (RS485)
// Once the full packet is received, send it to the client socket and to the callback handler
//
int svsKrRx(int devFd, uint8_t bus)
{
    int rc = ERR_PASS;
    int cnt;
    uint8_t kr_num;
    uint8_t data;
    uint8_t msgID;
    kr_bus_dev_info_t *kr_bus = &kr_dev_info.kr_bus_dev_info[bus];

    // wait for byte (blocking read)
    cnt = read(devFd, &data, 1);
    if (cnt <=0)
    {
        if (cnt == 0)
        {   // no data to read
            return(ERR_PASS);
        }
        else
        {   // some error occured
            logError("read: %d %s", errno, strerror(errno));
            if (errno == EAGAIN)
                rc = ERR_COMMS_TIMEOUT;
            else if (errno == EBADF)
                rc = ERR_FILE_DESC;
            else
                rc = ERR_FAIL;
            return(rc);
        }
    }

    // Get incoming bytes from the device, one byte at a time
    //do
    {
        //
        // We have one byte
        //

        //logDebug("byte %X", data);

        if(svsTimeGet_us() - kr_bus->timestamp_rx_activity_us > KR_MAX_INTER_BYTE_DURATION_US)
        {   // if the time in between bytes is too long, we reset the state machine
            if(kr_bus->frame_rx_state != KR_FRAME_STATE_IDLE)
            {
                logWarning("resetting frame_rx_state");
                kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
            }
        }
        kr_bus->timestamp_rx_activity_us = svsTimeGet_us(); // update RX timestamp

        //logDebug("state %d", kr_bus->frame_rx_state);

        // Process frame
        switch(kr_bus->frame_rx_state)
        {
            case KR_FRAME_STATE_IDLE:
                kr_bus->frame_rx_crc       = 0;
                kr_bus->frame_rx_index     = 0;
                kr_bus->preamble_rx_index  = 0;
                kr_bus->frame_done         = 0;
                kr_bus->pframe_rx          = (uint8_t *)&kr_bus->frame_rx;
                kr_bus->frame_rx_state     = KR_FRAME_STATE_PREAMBLE;
                // fall through to next state

            case KR_FRAME_STATE_PREAMBLE:
                if(data == kr_dev_info.preamble.preamble[kr_bus->preamble_rx_index])
                {
                    kr_bus->preamble_rx_index++;
                    if(kr_bus->preamble_rx_index >= KR_FRAME_PREAMBLE_SIZE)
                    {
                        kr_bus->frame_rx_state = KR_FRAME_STATE_SOF;
                    }
                }
                else
                {   // no preamble detected
                    kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                }
                break;

            case KR_FRAME_STATE_SOF:
                if(data == kr_dev_info.preamble.sof)
                {
                    kr_bus->frame_rx_state = KR_FRAME_STATE_FRAME;
                }
                else
                {   // no SOF detected
                    kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                }
                break;

            case KR_FRAME_STATE_FRAME:
                kr_bus->pframe_rx[kr_bus->frame_rx_index++] = data;
                // check to see when CRC field becomes available
                if(kr_bus->frame_rx_index > sizeof(kr_bus->frame_rx.hdr.crc))
                {   // CRC field received, start computing CRC on following data
                    kr_bus->frame_rx_crc = crc16_resume_compute(kr_bus->frame_rx_crc, &data, 1);
                }
                // check to see when len and len_inv fields become available
                if(kr_bus->frame_rx_index == (sizeof(kr_bus->frame_rx.hdr.crc) +
                                              sizeof(kr_bus->frame_rx.hdr.len) +
                                              sizeof(kr_bus->frame_rx.hdr.len_inv) + 1))
                {   // length and ~length fields received,
                    // validate the length field since we don't have the full frame to use the CRC for validation
                    if(kr_bus->frame_rx.hdr.len != (~kr_bus->frame_rx.hdr.len_inv & 0xFFFF))
                    {   // corrupt length
                        logError("corrupt frame length %d\n", kr_bus->frame_rx.hdr.len);
                        kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                        rc = ERR_LEN_FAIL;
                    }
                    else
                    {   // length field ok, validate it
                        if(kr_bus->frame_rx.hdr.len > KR_MSG_PAYLOAD_MAX)
                        {   // invalid length
                            STATS_INC(kr_dev_info.kr_bus_dev_info[bus].bus_stats.err_length);
                            logError("frame length out of range %d\n", kr_bus->frame_rx.hdr.len);
                            kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                            rc = ERR_LEN_TOO_LONG;
                        }
                    }
                }
                // check to see if whole frame has been received (header+payload but not the preamble)
                if(kr_bus->frame_rx_index == (kr_bus->frame_rx.hdr.len+sizeof(svsMsgKrFrameHeader_t)))
                {   // we have the whole frame, check the CRC
                    if(kr_bus->frame_rx_crc != kr_bus->frame_rx.hdr.crc)
                    {   // CRC invalid
                        STATS_INC(kr_dev_info.kr_bus_dev_info[bus].bus_stats.err_crc);
                        logError("invalid frame CRC computed %X received\n", kr_bus->frame_rx_crc, kr_bus->frame_rx.hdr.crc);
                        rc = ERR_CRC_FAIL;
                    }
                    else
                    {   // frame is valid, , we are done
                        STATS_INC(kr_dev_info.kr_bus_dev_info[bus].bus_stats.frame_rx_cnt);
                        kr_bus->frame_done = 1;
                        rc = ERR_PASS;
                    }
                    kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                }
                break;

            default:
                logError("unexpected state %d\n", kr_bus->frame_rx_state);
                kr_bus->frame_rx_state = KR_FRAME_STATE_IDLE;
                rc = ERR_UNEXP;
                break;
        }
    } // while((kr_bus->frame_done == 0) && (rc == ERR_PASS));

    //
    // Full frame received
    //
    if(kr_bus->frame_done != 1)
    {
        if(rc != ERR_PASS)
        {   // use call back to signal error while receiving frame
        }
    }
    else
    {
        kr_bus->frame_done = 0;
        msgID = kr_bus->frame_rx.hdr.msg_id;

        logDebug("KR frame received with msg ID %d", msgID);

        if(msgID >= MSG_ID_KR_MAX)
        {
            logError("invalid msg ID: %d", kr_bus->frame_rx.hdr.msg_id);
            rc = ERR_FAIL;
        }
        else
        {
            if(kr_dev_info.loopback_enable == 1)
            {   // when in loopback, replace the BROADCAST address by a valid address
                // so as to emulate a real device
                if(svsKrAddrMatch(&kr_bus->frame_rx.hdr.addr, &kr_dev_info.addr_broadcast) == ERR_PASS)
                {
                    memset(kr_bus->frame_rx.hdr.addr.octet, 0xEE, KR_MSG_ADDR_LENGTH);
                }
            }
            // Check to see if that was an ARP request
            if(kr_bus->frame_rx.hdr.msg_id == MSG_ID_KR_ADDR_GET)
            {
                rc = svsKrArpTableUpdate(bus, &kr_bus->frame_rx.hdr.addr);
                if(rc != ERR_PASS)
                {
                    logError("ARP table not updated");
                }
            }

            // Get the KR number from the KR address
            kr_num = 0;
            rc = svsKrPhysicalToVirtual(&kr_bus->frame_rx.hdr.addr, &kr_num);
            if(rc != ERR_PASS)
            {
                logError("KR number not found in ARP table");
            }
            else
            {   // we have the KR number, update the response field
                if(kr_state[kr_num].id_data_addr[msgID].rsp != 0)
                {   // validate length
                    if(kr_bus->frame_rx.hdr.len == kr_state[kr_num].id_data_addr[msgID].rsp_len)
                    {
                        memcpy(kr_state[kr_num].id_data_addr[msgID].rsp, kr_bus->frame_rx.payload, kr_bus->frame_rx.hdr.len);
                    }
                    else
                    {
                        logError("rsp length mismatch: received %d expected %d", kr_bus->frame_rx.hdr.len, kr_state[kr_num].id_data_addr[msgID].rsp_len);
                        rc = ERR_FAIL;
                        // when in loopback the response will have the same length as the request, so we ignore this error
                        if(kr_dev_info.loopback_enable == 1)
                        {
                            rc = ERR_PASS;
                        }
                    }
#if 0
                    logDebug("rsp length %d %d", kr_bus->frame_rx.hdr.len, kr_state[kr_num].id_data_addr[msgID].rsp_len);
                    {
                        int i;
                        for(i=0;i<kr_bus->frame_rx.hdr.len;i++)
                        {
                            logDebug("%02X", kr_bus->frame_rx.payload[i]);
                        }
                    }
#endif
                }
                else
                {
                    logError("rsp not initialized");
                    rc = ERR_FAIL;
                }
                kr_state[kr_num].msg_stats.rx_cnt++;
                kr_state[kr_num].msg_stats.msgid_stats[msgID].time_received = svsTimeGet_us();

                // Compute time it took to receive the message witht the same msg ID
                kr_state[kr_num].msg_stats.msgid_stats[msgID].time_response =
                kr_state[kr_num].msg_stats.msgid_stats[msgID].time_received -
                kr_state[kr_num].msg_stats.msgid_stats[msgID].time_sent;
            }
        }

        // Check for any errors, but not when in loopback
        if((rc != ERR_PASS) && (kr_dev_info.loopback_enable == 0))
        {   // In case of error, send status to callback with a SVS status message
            logError("");
        }
        else
        {   // All is good, send message to the waiting client if any
            if(kr_bus->frame_rx.hdr.sockFd > 0)
            {   // sockFd retrieved from incoming message
                rc = svsSocketSendKr(kr_bus->frame_rx.hdr.sockFd, msgID, kr_num, 0, 0, kr_bus->frame_rx.payload, kr_bus->frame_rx.hdr.len);
                if(rc != ERR_PASS)
                {
                    logError("svsSocketSendKr");
                }
            }
            else
            {
                if(kr_bus->frame_rx.hdr.sockFd == 0)
                {   // async message received, send message to callback server
                    rc = svsSocketSendCallback(svsCallbackSockFd, MODULE_ID_KR, kr_num, msgID, 0, kr_bus->frame_rx.payload, kr_bus->frame_rx.hdr.len);
                    if(rc != ERR_PASS)
                    {
                        logError("svsSocketSendCallback");
                    }
                }
                else // sockFd < 0, this response is not expected
                {
                    logError("unexpected response with sockFd < 0, with msgID %d", msgID);
                    rc = ERR_FAIL;
                }
            }
        }
    }

    return(rc);
}

//
// Send payload to physical KR
//
int svsKrTx(int sockFd, uint8_t bus, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc;
    int len;
    uint16_t crc;
    svsMsgKrFrameHeader_t frame_hdr;

    //logDebug("");

    int devFd   = kr_dev_info.kr_bus_dev_info[bus].devFd;

    if(devFd < 0)
    {   // this case is not a failure, devFd is not in use
        return(ERR_PASS);
    }
    //
    // fill the frame header
    //

    // Get the physical address from KR number
    rc = svsKrVirtualToPhysical(hdr->dev_num, &frame_hdr.addr);
    if(rc != ERR_PASS)
    {
        logError("destination address not found");
        return(rc);
    }

    frame_hdr.len       = hdr->len;
    frame_hdr.len_inv   = ~frame_hdr.len;
    frame_hdr.msg_id    = hdr->u.krhdr.msg_id;
    frame_hdr.sockFd    = sockFd;
    frame_hdr.flags     = hdr->u.krhdr.flags;
    frame_hdr.seq       = kr_dev_info.seq_num++;
    frame_hdr.timestamp = svsTimeGet_ms();
    // compute the CRC which includes the header and the payload but not the preamble
    crc = crc16_compute((uint8_t *)&(frame_hdr.len), sizeof(frame_hdr)-sizeof(frame_hdr.crc));
    crc = crc16_resume_compute(crc, payload, frame_hdr.len);
    frame_hdr.crc = crc;

    //
    // Perform backoff
    //
#if 0
    int32_t delay = 0;
    uint16_t j = 0;

    // check for any line activity, if any perform backoff
    while(svsTimeGet_us() - kr_dev_info.kr_bus_dev_info[bus].timestamp_rx_activity_us < (int64_t)KR_MIN_LISTEN_US)
    {
        // generate random delay
        // - add minimum delay (1 byte duration)
        // - add random number that is a fraction of the frame size, this is chosen based on 100 byte frame size (about 10000us)
        // - add an exponential factor that keep on increasing with every iteration
        delay = (rand() * ((1<<j) - 1) * KR_FRAME_AVG_TIME_US / RAND_MAX); // get random value (0..2^j-1) * 1000
        delay = KR_MIN_BACKOFF_US + delay;
        j++;
        if(j>16)
        {   // too many retries, cannot send packet
            STATS_INC(kr_dev_info.kr_bus_dev_info[bus].bus_stats.err_backoff);
            return(ERR_FAIL);
        }
        usleep(delay);
        //logDebug("backoff delay %d", delay);
    }

    if(j>0)
    {   // backoff has occured
        STATS_INC(kr_dev_info.kr_bus_dev_info[bus].bus_stats.backoff_cnt);
    }
    if(kr_dev_info.kr_bus_dev_info[bus].bus_stats.backoff_delay_max < delay)
    {
        kr_dev_info.kr_bus_dev_info[bus].bus_stats.backoff_delay_max = delay;
    }

    // perform inter-frame gap (delay between frames) to avoid bus over utilization
    if(svsTimeGet_us() - kr_dev_info.kr_bus_dev_info[bus].timestamp_rx_activity_us < (int64_t)KR_INTER_FRAME_GAP_US)
    {
        usleep(KR_INTER_FRAME_GAP_US);
        //logDebug("inter frame delay %d", KR_INTER_FRAME_GAP_US);
    }

    kr_dev_info.kr_bus_dev_info[bus].timestamp_rx_activity_us = svsTimeGet_us();
#endif

    //
    // TRANSMIT the frame
    //

    // send the preamble
    len = svsDevWrite(devFd, (uint8_t *)&kr_dev_info.preamble, sizeof(kr_dev_info.preamble));
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",kr_dev_info.kr_bus_dev_info[bus].devName, kr_dev_info.kr_bus_dev_info[bus].devFd);
        return(ERR_FAIL);
    }
    else
    {
        if(len != sizeof(kr_dev_info.preamble))
        {
            logError("write incomplete: wrote %d instead of %d", len, sizeof(kr_dev_info.preamble));
            return(ERR_FAIL);
        }
    }
    // send the header
    len = svsDevWrite(devFd, (uint8_t *)&frame_hdr, sizeof(frame_hdr));
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",kr_dev_info.kr_bus_dev_info[bus].devName, kr_dev_info.kr_bus_dev_info[bus].devFd);
        return(ERR_FAIL);
    }
    else
    {
        if(len != sizeof(frame_hdr))
        {
            logError("write incomplete: wrote %d instead of %d", len, sizeof(frame_hdr));
            return(ERR_FAIL);
        }
    }

    logDebug("wrote header %d\n", len);

    //logDebug("writing payload with length %d\n", hdr->len);

    // send the payload
    len = svsDevWrite(devFd, payload, hdr->len);
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",kr_dev_info.kr_bus_dev_info[bus].devName, kr_dev_info.kr_bus_dev_info[bus].devFd);
        return(ERR_FAIL);
    }
    else
    {
        if(len != hdr->len)
        {
            logError("write incomplete: wrote %d instead of %d", len, hdr->len);
            return(ERR_FAIL);
        }
    }

#if 0 // need to update all devices for KR_NUM_ALL
    if(hdr->dev_num != KR_NUM_ALL)
    {
        kr_state[hdr->dev_num].msg_stats.tx_cnt++;
        kr_state[hdr->dev_num].msg_stats.msgid_stats[hdr->u.krhdr.msg_id].time_sent = svsTimeGet_us();
    }
#endif

    logDebug("wrote payload %d\n", len);

    return(rc);
}

int svsDevWrite(int devFd, uint8_t *data, int len)
{
    int rc = 0;

    if(data == 0)
    {
        logError("data is null");
        return -1;
    }
    if(len <= 0)
    {   // don't allow writes with 0 or negative length
        return len;
    }

#if 1
    //
    // To solve the problem when using USB dongles or when using serial devices with a small hardware/software
    // buffer, we monitor the write() and retry for as long as there are characters in the buffer to send
    // and for as long as the EAGAIN (device busy) error is occuring.
    // In between calls we add a delay (could use a select call on write)
    //

    int      len_written    = 0;
    int      retry = 0;
    do
    {
        rc = write(devFd, &data[len_written], len - len_written);
        //logDebug("write %d", rc);
        if(rc < 0)
        {   // ignore the resource busy error
            if(errno != EAGAIN)
            {   // some other error
                logError("write: %d %s", errno, strerror(errno));
                return(rc);
            }
        }
        else
        {
            len_written = len_written + rc;
            if(len_written >= len)
            {   // we are done
                break;
            }
        }

        logDebug("wrote %d/%d", len_written, len);
        retry++;
        usleep(10000); // delay in between calls when EAGAIN is set
    } while(retry < 100);

    //logDebug("len_written %d, retry %d", len_written, retry);

    if(len_written != len)
    {
        logError("failed to write after %d retries, written bytes %d", retry, len_written);
    }
    rc = len_written;
#else
    int i;
    for(i=0; i<len; i++)
    {
        //logDebug("writing... len %d %d", len, i);
        rc = write(devFd, &data[i], 1);
        if(rc < 1)
        {
            logError("write: %s", strerror(errno));
            return(rc);
        }

        fsync(devFd);
        usleep(500);
    }
    rc = i;
#endif

    return(rc);
}

#if 0
int svsKrPortOpen(char *devName, int baudrate)
{
    int rc = ERR_FAIL;
    int devFd;

    if(devName == 0)
    {
        logError("device name is 0");
        return(-1);
    }

    if(devName[0] == '\0')
    {
        logWarning("device name not defined, port not open");
        return(-1);
    }

    logInfo("Opening port [%s]", devName);

    // Open port for reading and writing,
    // not as controlling TTY as we don't want to kill the thread if CTRL-C is detected (noise)
    // non-blocking as we are blocking in the select call
    // O_SYNC: the write will block until the data has been completely sent
    devFd = open(devName, O_RDWR | /*O_NONBLOCK |*/ O_NOCTTY | O_NDELAY | O_SYNC);
    if(-1 == devFd)
    {
        logError("open: %s %s", devName, strerror(errno));
        return(-1);
    }

    struct termios portConfig;
    /* Get the current port attributes and modify them */
    rc = tcgetattr(devFd, &portConfig);
    if(rc < 0)
    {
        logError("tcgetattr: %s", strerror(errno));
        close(devFd);
        return(-1);
    }

    bzero(&portConfig, sizeof(portConfig)); /* clear struct for new port settings */

    /*
    BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
    CRTSCTS : output hardware flow control
    CS8     : 8n1 (8bit,no parity,1 stopbit)
    CLOCAL  : local connection, no modem contol
    CREAD   : enable receiving characters
    */
    portConfig.c_cflag = /*BAUDRATE |*/ CS8 | CLOCAL | CREAD;
    // IGNPAR  : ignore bytes with parity errors
    portConfig.c_iflag = IGNPAR;
    // Raw output.
    portConfig.c_oflag = 0;
    // Raw input
    portConfig.c_lflag = 0;
    // initialize all control characters
    // default values can be found in /usr/include/termios.h, and are given
    // in the comments, but we don't need them here
    portConfig.c_cc[VINTR]    = 0;     /* Ctrl-c */
    portConfig.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    portConfig.c_cc[VERASE]   = 0;     /* del */
    portConfig.c_cc[VKILL]    = 0;     /* @ */
    portConfig.c_cc[VEOF]     = 0;     /* Ctrl-d */
    portConfig.c_cc[VTIME]    = 0;     /* inter-character timer */
    portConfig.c_cc[VMIN]     = 0;     /* non-blocking read */
    portConfig.c_cc[VSWTC]    = 0;     /* '\0' */
    portConfig.c_cc[VSTART]   = 0;     /* Ctrl-q */
    portConfig.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    portConfig.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    portConfig.c_cc[VEOL]     = 0;     /* '\0' */
    portConfig.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    portConfig.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    portConfig.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    portConfig.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    portConfig.c_cc[VEOL2]    = 0;     /* '\0' */

    speed_t speed;
    rc = svsBaudRateToSpeed(baudrate, &speed);
    if(rc == 0)
    {
        rc = cfsetspeed(&portConfig, speed);
        if(-1 == rc)
        {
            logError("cfsetspeed: %s", strerror(errno));
        }
        else
        {
            tcflush(devFd, TCIFLUSH);
            rc = tcsetattr(devFd, TCSANOW, &portConfig);
            if(-1 == rc)
            {
                logError("tcsetattr: %s", strerror(errno));
            }
        }
    }

    if(rc != 0)
    {
        logDebug("closing: %s", devName);
        close(devFd);
        devFd = -1;
    }
    return(devFd);
}

int svsBaudRateToSpeed(int baudrate, speed_t *speed)
{
    int rc = ERR_PASS;

    switch(baudrate)
    {
        case 9600:
            *speed = B9600;
            break;
        case 19200:
            *speed = B19200;
            break;
        case 38400:
            *speed = B38400;
            break;
        case 57600:
            *speed = B57600;
            break;
        case 115200:
            *speed = B115200;
            break;
        case 230400:
            *speed = B230400;
            break;

        default:
            logError("invalid baudrate");
            rc = ERR_FAIL;
            break;
    }

    return(rc);
}
#endif

#if KR_SIMULATOR
//
// Description:
// This handler is called when KR server receives request from client
//
static int svsSocketClientKrSimHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;

    logDebug("KR simulation handler");

    // Validate msg ID
    if(hdr->u.krhdr.msg_id >= MSG_ID_KR_MAX)
    {
        logError("invalid msg ID");
        return(ERR_FAIL);
    }
    // Validate device number
    if(hdr->dev_num != KR_NUM_ALL)
    {   // not a broadcast
        if(hdr->dev_num >= svsKrMaxGet())
        {
            logError("invalid device number");
            return(ERR_FAIL);
        }
    }

    int i, start, end;
    // Update request field
    if(hdr->dev_num == KR_NUM_ALL)
    {
        start = 0;
        end   = svsKrMaxGet();
    }
    else
    {
        start = hdr->dev_num;
        end   = hdr->dev_num + 1;
    }
    for(i=start; i<end; i++)
    {
        if(kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req != 0)
        {   // Validate length, in case application has a mismatch with server
            if(hdr->len == kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req_len)
            {
                memcpy(kr_state[i].id_data_addr[hdr->u.krhdr.msg_id].req, payload, hdr->len);
            }
            else
            {
                logError("req length mismatch");
            }
        }
        else
        {
            logError("req not initialized");
        }
    }

    logInfo("Sending message to KR protocol");

    //
    //  Physical KR simulator
    //
    uint8_t *prsp;
    uint16_t rsp_len;
    kr_addr_t kr_addr;

    kr_addr.octet[0] = 0x01;
    kr_addr.octet[1] = 0x02;
    kr_addr.octet[2] = 0x03;
    kr_addr.octet[3] = 0x04;
    kr_addr.octet[4] = 0x05;
    kr_addr.octet[5] = 0x06;

    switch(hdr->u.krhdr.msg_id)
    {
        case MSG_ID_KR_ADDR_GET:
            {
                kr_address_get_msg_rsp_t rsp;
                prsp = (uint8_t *)&rsp;
                rsp_len = sizeof(rsp);
                rsp.kr_addr.octet[0] = 0x01;
                rsp.kr_addr.octet[1] = 0x02;
                rsp.kr_addr.octet[2] = 0x03;
                rsp.kr_addr.octet[3] = 0x04;
                rsp.kr_addr.octet[4] = 0x05;
                rsp.kr_addr.octet[5] = 0x06;
                memcpy(&kr_addr, &rsp.kr_addr, sizeof(kr_addr));
                rsp.status   = ERR_PASS;
            }
            break;

        case MSG_ID_KR_UNLOCK_CODE_GET:
            {
                kr_unlock_code_get_msg_rsp_t rsp;
                prsp = (uint8_t *)&rsp;
                rsp_len = sizeof(rsp);
                rsp.unlock_code[0] = 1;
                rsp.unlock_code[1] = 2;
                rsp.unlock_code[2] = 3;
                rsp.unlock_code[3] = 2;
                rsp.unlock_code[4] = 1;
                rsp.unlock_code_num = 5;
                rsp.status = ERR_PASS;
            }
            break;

        default:
            logError("not implemented message ID: %d", hdr->u.krhdr.msg_id);
            return(ERR_FAIL);
            break;
    }

    //
    // When data is received
    //
    uint8_t msgID = hdr->u.krhdr.msg_id;
    uint8_t kr_num = 0;
    {
        logDebug("frame done: msg ID %d", msgID);

        if(msgID >= MSG_ID_KR_MAX)
        {
            logError("invalid msg ID: %d", msgID);
            rc = ERR_FAIL;
        }
        else
        {   // Check to see if that was an ARP request
            if(msgID == MSG_ID_KR_ADDR_GET)
            {
                rc = svsKrArpTableUpdate(0, &kr_addr);
                if(rc != ERR_PASS)
                {
                    logError("ARP table not updated");
                }
            }

            // Get the KR number from the KR address
            rc = svsKrPhysicalToVirtual(&kr_addr, &kr_num);
            if(rc != ERR_PASS)
            {
                logError("KR number not found in ARP table");
            }
            else
            {   // we have the KR number, update the response field
            }
        }
    }


    rc = svsSocketSendKr(sockFd, hdr->u.krhdr.msg_id, kr_num, 0, 0, prsp, rsp_len);
    if(rc != ERR_PASS)
    {
        logError("svsSocketSendKr");
    }

    // also send message to callback server
    logDebug("sending to callback server");
    rc = svsSocketSendCallback(svsCallbackSockFd, MODULE_ID_KR, kr_num, hdr->u.krhdr.msg_id, 0, prsp, rsp_len);
    if(rc != ERR_PASS)
    {
        logError("svsSocketSendCallback");
    }

    return(rc);
}
#endif

