
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
#include <svsSocket.h>
#include <svsBdp.h>
#include <svsConfig.h>
#include <crc.h>

//
// NOTES:
// The idea when sending and receiving packets is that the operation of sending is associated with a response
// when performing synchronous messages (SCU->BDP). Each packet will have a sockFd so that the response will be returned
// to the calling client waiting on that socket.
// There is a special case when the client performs a broadcast message, in this case the 'no response' device flag is set
// so that the client does not have to wait for the responses as they may be too many.
//
// The only case when a response is required with a broadcast is when the bus scan is requested (typically upon startup).
// This case is handled specifically in svsBdpScan(). Also the sockFd is set to be -1.
// When receiving asynchronous messages from the BDPs they will have sockFd set to 0.
//
// Now when a message is received there are several checks to be made in order to decide if that message were to be sent
// to the client or put on the Callback queue:
// - The message is checked if it is in the node list based on its sequence number, if not it is logged and ignored
// - The message is verified against its msgID
// - Check if the timeout as set by the client has not been exceeded.
//   If the timeout has been exceeded, the client would most likely have resumed its operations and no longer waiting for the
//   response, so we do not allow that response to be sent back to the client thus avoiding any socket overflows.
//

#define BDP_SIMULATOR           0

static bdp_dev_info_t           bdp_dev_info;
static bdp_state_t              bdp_state[BDP_MAX];
static socket_thread_info_t     socket_thread_info_tx;
static socket_thread_info_t     socket_thread_info_rx;
static int                      svsCallbackSockFd           = -1;
static bdp_node_t               *bdp_node_head              = 0;
static pthread_t                frame_thread;
pthread_mutex_t                 mutexFrameNodeAccess;       // used to protect the node data
//pthread_mutex_t                 mutexFrameSend;             // used to protect the svsBdpFrameSend()

static int svsSocketClientBdpHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
#if BDP_SIMULATOR
static int svsSocketClientBdpSimHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
#endif
static int svsSocketClientBdpDev1Handler(int devFd);
static int svsSocketClientBdpDev2Handler(int devFd);
static int svsBdpTx(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);
static int svsBdpRx(int devFd, uint8_t bus);
static int svsBdpBackoff(uint8_t bus, int64_t timeout_ms);
static bdp_node_t *svsBdpFrameAdd(bdp_node_t *node_head, svsSocketMsgHeader_t *hdr, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload);
static int svsBdpFrameFindAndRemove(bdp_node_t *node_head, uint16_t seq, bdp_node_t *node);
static int svsBdpFrameSend(int64_t timeout_ms, uint16_t dev_num, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload, uint16_t len);
static int svsBdpFrameSendBus(uint8_t bus, int64_t timeout_ms, uint16_t dev_num, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload, uint16_t len);
//static void svsBdpFrameTsentUpdate(bdp_node_t *node);
static void *svsBdpFrameThread(void *arg);

static char *msgIDStrings[] =
{
    "MSG_ID_BDP_ALL",
    "MSG_ID_BDP_ADDR_GET",
    "MSG_ID_BDP_ADDR_SET",
    "MSG_ID_BDP_CHANGE_MODE",
    "MSG_ID_BDP_GET_MODE",
    "MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND",
    "MSG_ID_BDP_ACK",
    "MSG_ID_BDP_ECHO",
    "MSG_ID_BDP_ECHO_INV",
    "MSG_ID_BDP_COMMS_SET",
    "MSG_ID_BDP_COMMS_GET",
    "MSG_ID_BDP_PING",
    "MSG_ID_BDP_RESET",
    "MSG_ID_BDP_NOP",
    "MSG_ID_BDP_CONSOLE",
    "MSG_ID_BDP_LOG",
    "MSG_ID_BDP_OD",
    "MSG_ID_BDP_GET_BOOTBLOCK_REV",
    "MSG_ID_BDP_GET_APPLICATION_REV",
    "MSG_ID_BDP_GET_STATS",
    "MSG_ID_BDP_SN_SET",
    "MSG_ID_BDP_SN_GET",
    "MSG_ID_BDP_POWER_SET",
    "MSG_ID_BDP_POWER_GET",
    "MSG_ID_BDP_RFID_GET",
    "MSG_ID_BDP_JTAG_DEBUG_SET",
    "MSG_ID_BDP_RED_LED_SET",
    "MSG_ID_BDP_RED_LED_GET",
    "MSG_ID_BDP_GRN_LED_SET",
    "MSG_ID_BDP_GRN_LED_GET",
    "MSG_ID_BDP_YLW_LED_SET",
    "MSG_ID_BDP_YLW_LED_GET",
    "MSG_ID_BDP_SWITCH_GET",
    "MSG_ID_BDP_UNLOCK_CODE_GET",
    "MSG_ID_BDP_BUZZER_SET",
    "MSG_ID_BDP_BUZZER_GET",
    "MSG_ID_BDP_DOCK_GET",
    "MSG_ID_BDP_BIKE_LOCK_SET",
    "MSG_ID_BDP_BIKE_LOCK_GET",
    "MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD",
    "MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL",
    "MSG_ID_MEMORY_CRC",
    "MSG_ID_BDP_MOTOR_SET",
    "MSG_ID_BDP_MOTOR_GET",
    "MSG_ID_BDP_RFID_ALL_GET",
    "MSG_ID_BDP_SWITCH_BUSTED_GET",
    "MSG_ID_BDP_COMPATIBILITY_GET",
    "MSG_ID_BDP_COMPATIBILITY_SET",
    "MSG_ID_BDP_UNLOCK_CODE_BIKE_GET",

    "MSG_ID_BDP_MAX"
};

char *msgIDToString(bdp_msg_id_t id)
{
    if(id <= MSG_ID_BDP_MAX)
        return(msgIDStrings[id]);
    return(NULL);
}

int svsBdpServerInit(void)
{
    int rc;
    int i, devFd;

    memset(bdp_state, 0, sizeof(bdp_state_t) * BDP_MAX);
    memset(&socket_thread_info_tx, 0, sizeof(socket_thread_info_t));
    memset(&socket_thread_info_rx, 0, sizeof(socket_thread_info_t));
    memset(&bdp_dev_info, 0, sizeof(bdp_dev_info_t));

    // initial sequence number
    bdp_dev_info.seq_num = 1;   // avoid 0 as it is used for incoming asyn frames
    // head node of linked list holding the frames
    bdp_node_head = svsBdpNodeInit();

    // Get configuration parameters
    svsConfigModuleParamStrGet("bdp", "devname1", BDP_DEV_DEFAULT_NAME1, bdp_dev_info.bdp_bus_dev_info[0].devName, BDP_BUS_DEV_NAME_MAX);
    svsConfigParamIntGet("bdp", "baudrate1", BDP_DEV_DEFAULT_BAUDRATE, &bdp_dev_info.bdp_bus_dev_info[0].baudRate);
    bdp_dev_info.bdp_bus_dev_info[0].devFd    = -1;

    svsConfigModuleParamStrGet("bdp", "devname2", BDP_DEV_DEFAULT_NAME2, bdp_dev_info.bdp_bus_dev_info[1].devName, BDP_BUS_DEV_NAME_MAX);
    svsConfigParamIntGet("bdp", "baudrate2", BDP_DEV_DEFAULT_BAUDRATE, &bdp_dev_info.bdp_bus_dev_info[1].baudRate);
    bdp_dev_info.bdp_bus_dev_info[1].devFd    = -1;

    svsConfigParamIntGet("bdp", "loopback", 0, &bdp_dev_info.loopback_enable);
    if(bdp_dev_info.loopback_enable)
    {
        logInfo("loopback enabled");
    }

    crc16_init();

    //
    // Open and configure the serial ports
    //
    devFd = svsPortOpen(bdp_dev_info.bdp_bus_dev_info[0].devName, bdp_dev_info.bdp_bus_dev_info[0].baudRate);
    if(devFd < 0)
    {
        logError("");
    }
    else
    {
        bdp_dev_info.bdp_bus_dev_info[0].devFd = devFd;
    }

    devFd = svsPortOpen(bdp_dev_info.bdp_bus_dev_info[1].devName, bdp_dev_info.bdp_bus_dev_info[1].baudRate);
    if(devFd < 0)
    {
        logError("");
    }
    else
    {
        bdp_dev_info.bdp_bus_dev_info[1].devFd = devFd;
    }

    // configure BDP server info
    socket_thread_info_tx.name        = "BDP-TX";
    socket_thread_info_tx.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info_tx.port        = SOCKET_PORT_BDP_TX;
    socket_thread_info_tx.devFd1      = -1;
    socket_thread_info_tx.devFd2      = -1;
    socket_thread_info_tx.fnServer    = 0;
#if BDP_SIMULATOR
    socket_thread_info_tx.fnClient    = svsSocketClientBdpSimHandler;
#else
    socket_thread_info_tx.fnClient    = svsSocketClientBdpHandler;
#endif
    socket_thread_info_tx.fnDev1      = 0;
    socket_thread_info_tx.fnDev2      = 0;
    socket_thread_info_tx.fnThread    = svsSocketServerThread;
    socket_thread_info_tx.len_max     = BDP_MSG_PAYLOAD_MAX;

    // create the BDP server
    rc = svsSocketServerCreate(&socket_thread_info_tx);
    if(rc != ERR_PASS)
    {
        logError("failed to start BDP-TX server");
        return(rc);
    }

    socket_thread_info_rx.name        = "BDP-RX";
    socket_thread_info_rx.ipAddr      = SVS_SOCKET_SERVER_LISTEN_IP;
    socket_thread_info_rx.port        = SOCKET_PORT_BDP_RX;
    socket_thread_info_rx.devFd1      = bdp_dev_info.bdp_bus_dev_info[0].devFd;
    socket_thread_info_rx.devFd2      = bdp_dev_info.bdp_bus_dev_info[1].devFd;
    socket_thread_info_rx.fnServer    = 0;
#if BDP_SIMULATOR
    socket_thread_info_rx.fnClient    = 0; //svsSocketClientBdpSimHandler;
#else
    socket_thread_info_rx.fnClient    = 0;
#endif
    socket_thread_info_rx.fnDev1      = svsSocketClientBdpDev1Handler;
    socket_thread_info_rx.fnDev2      = svsSocketClientBdpDev2Handler;
    socket_thread_info_rx.fnThread    = svsSocketServerThread;
    socket_thread_info_rx.len_max     = BDP_MSG_PAYLOAD_MAX;

    // create the BDP server
    rc = svsSocketServerCreate(&socket_thread_info_rx);
    if(rc != ERR_PASS)
    {
        logError("failed to start BDP-RX server");
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
    for(i=0;i<BDP_MAX;i++)
    {
        // MSG_ID_BDP_RED_LED_SET
        bdp_state[i].id_data_addr[MSG_ID_BDP_RED_LED_SET].req = &bdp_state[i].bdp_led_red_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_RED_LED_SET].rsp = &bdp_state[i].bdp_led_red_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GRN_LED_SET].req = &bdp_state[i].bdp_led_grn_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GRN_LED_SET].rsp = &bdp_state[i].bdp_led_grn_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_YLW_LED_SET].req = &bdp_state[i].bdp_led_ylw_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_YLW_LED_SET].rsp = &bdp_state[i].bdp_led_ylw_set_msg_rsp;

        bdp_state[i].id_data_addr[MSG_ID_BDP_RED_LED_SET].req_len = sizeof(bdp_state[i].bdp_led_red_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_RED_LED_SET].rsp_len = sizeof(bdp_state[i].bdp_led_red_set_msg_rsp);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GRN_LED_SET].req_len = sizeof(bdp_state[i].bdp_led_grn_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GRN_LED_SET].rsp_len = sizeof(bdp_state[i].bdp_led_grn_set_msg_rsp);
        bdp_state[i].id_data_addr[MSG_ID_BDP_YLW_LED_SET].req_len = sizeof(bdp_state[i].bdp_led_ylw_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_YLW_LED_SET].rsp_len = sizeof(bdp_state[i].bdp_led_ylw_set_msg_rsp);

        // MSG_ID_BDP_SWITCH_GET
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_GET].req = &bdp_state[i].bdp_switch_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_GET].rsp = &bdp_state[i].bdp_switch_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_GET].req_len = sizeof(bdp_state[i].bdp_switch_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_GET].rsp_len = sizeof(bdp_state[i].bdp_switch_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_BUSTED_GET].req = &bdp_state[i].bdp_switch_busted_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_BUSTED_GET].rsp = &bdp_state[i].bdp_switch_busted_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_BUSTED_GET].req_len = sizeof(bdp_state[i].bdp_switch_busted_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_SWITCH_BUSTED_GET].rsp_len = sizeof(bdp_state[i].bdp_switch_busted_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_GET].req = &bdp_state[i].bdp_compatibility_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_GET].rsp = &bdp_state[i].bdp_compatibility_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_GET].req_len = sizeof(bdp_state[i].bdp_compatibility_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_GET].rsp_len = sizeof(bdp_state[i].bdp_compatibility_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_SET].req = &bdp_state[i].bdp_compatibility_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_SET].rsp = &bdp_state[i].bdp_compatibility_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_SET].req_len = sizeof(bdp_state[i].bdp_compatibility_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMPATIBILITY_SET].rsp_len = sizeof(bdp_state[i].bdp_compatibility_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_SET].req = &bdp_state[i].bdp_power_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_SET].rsp = &bdp_state[i].bdp_power_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_SET].req_len = sizeof(bdp_state[i].bdp_power_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_SET].rsp_len = sizeof(bdp_state[i].bdp_power_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_GET].req = &bdp_state[i].bdp_power_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_GET].rsp = &bdp_state[i].bdp_power_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_GET].req_len = sizeof(bdp_state[i].bdp_power_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_POWER_GET].rsp_len = sizeof(bdp_state[i].bdp_power_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_SET].req = &bdp_state[i].bdp_bike_lock_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_SET].rsp = &bdp_state[i].bdp_bike_lock_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_SET].req_len = sizeof(bdp_state[i].bdp_bike_lock_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_SET].rsp_len = sizeof(bdp_state[i].bdp_bike_lock_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_GET].req = &bdp_state[i].bdp_bike_lock_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_GET].rsp = &bdp_state[i].bdp_bike_lock_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_GET].req_len = sizeof(bdp_state[i].bdp_bike_lock_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_BIKE_LOCK_GET].rsp_len = sizeof(bdp_state[i].bdp_bike_lock_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_BUZZER_SET].req = &bdp_state[i].bdp_buzzer_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BUZZER_SET].rsp = &bdp_state[i].bdp_buzzer_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BUZZER_SET].req_len = sizeof(bdp_state[i].bdp_buzzer_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_BUZZER_SET].rsp_len = sizeof(bdp_state[i].bdp_buzzer_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_ECHO].req = &bdp_state[i].bdp_echo_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_ECHO].rsp = &bdp_state[i].bdp_echo_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_ECHO].req_len = sizeof(bdp_state[i].bdp_echo_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_ECHO].rsp_len = sizeof(bdp_state[i].bdp_echo_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_CONSOLE].req = &bdp_state[i].bdp_console_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_CONSOLE].rsp = &bdp_state[i].bdp_console_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_CONSOLE].req_len = sizeof(bdp_state[i].bdp_console_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_CONSOLE].rsp_len = sizeof(bdp_state[i].bdp_console_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_GET].req = &bdp_state[i].bdp_unlock_code_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_GET].rsp = &bdp_state[i].bdp_unlock_code_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_GET].req_len = sizeof(bdp_state[i].bdp_unlock_code_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_GET].rsp_len = sizeof(bdp_state[i].bdp_unlock_code_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_BIKE_GET].rsp = &bdp_state[i].bdp_unlock_code_bike_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_UNLOCK_CODE_BIKE_GET].rsp_len = sizeof(bdp_state[i].bdp_unlock_code_bike_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_GET].req = &bdp_state[i].bdp_rfid_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_GET].rsp = &bdp_state[i].bdp_rfid_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_GET].req_len = sizeof(bdp_state[i].bdp_rfid_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_GET].rsp_len = sizeof(bdp_state[i].bdp_rfid_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_ALL_GET].req = &bdp_state[i].bdp_rfid_all_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_ALL_GET].rsp = &bdp_state[i].bdp_rfid_all_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_ALL_GET].req_len = sizeof(bdp_state[i].bdp_rfid_all_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_RFID_ALL_GET].rsp_len = sizeof(bdp_state[i].bdp_rfid_all_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_CHANGE_MODE].req = &bdp_state[i].bdp_change_mode_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_CHANGE_MODE].rsp = &bdp_state[i].bdp_change_mode_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_CHANGE_MODE].req_len = sizeof(bdp_state[i].bdp_change_mode_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_CHANGE_MODE].rsp_len = sizeof(bdp_state[i].bdp_change_mode_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_MODE].req = &bdp_state[i].bdp_get_mode_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_MODE].rsp = &bdp_state[i].bdp_get_mode_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_MODE].req_len = sizeof(bdp_state[i].bdp_get_mode_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_MODE].rsp_len = sizeof(bdp_state[i].bdp_get_mode_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND].req = &bdp_state[i].bdp_firmware_upgrade_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND].rsp = &bdp_state[i].bdp_firmware_upgrade_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND].req_len = sizeof(bdp_state[i].bdp_firmware_upgrade_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND].rsp_len = sizeof(bdp_state[i].bdp_firmware_upgrade_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_ACK].rsp = &bdp_state[i].bdp_ack_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_ACK].rsp_len = sizeof(bdp_state[i].bdp_ack_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_ADDR_GET].req = &bdp_state[i].bdp_address_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_ADDR_GET].rsp = &bdp_state[i].bdp_address_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_ADDR_GET].req_len = sizeof(bdp_state[i].bdp_address_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_ADDR_GET].rsp_len = sizeof(bdp_state[i].bdp_address_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_JTAG_DEBUG_SET].req = &bdp_state[i].bdp_jtag_debug_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_JTAG_DEBUG_SET].rsp = &bdp_state[i].bdp_jtag_debug_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_JTAG_DEBUG_SET].req_len = sizeof(bdp_state[i].bdp_jtag_debug_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_JTAG_DEBUG_SET].rsp_len = sizeof(bdp_state[i].bdp_jtag_debug_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_APPLICATION_REV].req = &bdp_state[i].bdp_get_app_rev_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_APPLICATION_REV].rsp = &bdp_state[i].bdp_get_app_rev_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_APPLICATION_REV].req_len = sizeof(bdp_state[i].bdp_get_app_rev_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_APPLICATION_REV].rsp_len = sizeof(bdp_state[i].bdp_get_app_rev_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_BOOTBLOCK_REV].req = &bdp_state[i].bdp_get_bb_rev_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_BOOTBLOCK_REV].rsp = &bdp_state[i].bdp_get_bb_rev_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_BOOTBLOCK_REV].req_len = sizeof(bdp_state[i].bdp_get_bb_rev_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_BOOTBLOCK_REV].rsp_len = sizeof(bdp_state[i].bdp_get_bb_rev_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_STATS].req = &bdp_state[i].bdp_get_stats_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_STATS].rsp = &bdp_state[i].bdp_get_stats_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_STATS].req_len = sizeof(bdp_state[i].bdp_get_stats_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_GET_STATS].rsp_len = sizeof(bdp_state[i].bdp_get_stats_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD].req = &bdp_state[i].bdp_bootblock_upload_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD].rsp = &bdp_state[i].bdp_bootblock_upload_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD].req_len = sizeof(bdp_state[i].bdp_bootblock_upload_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD].rsp_len = sizeof(bdp_state[i].bdp_bootblock_upload_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL].req = &bdp_state[i].bdp_bootblock_install_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL].rsp = &bdp_state[i].bdp_bootblock_install_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL].req_len = sizeof(bdp_state[i].bdp_bootblock_install_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL].rsp_len = sizeof(bdp_state[i].bdp_bootblock_install_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_MEMORY_CRC].req = &bdp_state[i].bdp_memory_crc_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MEMORY_CRC].rsp = &bdp_state[i].bdp_memory_crc_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MEMORY_CRC].req_len = sizeof(bdp_state[i].bdp_memory_crc_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_MEMORY_CRC].rsp_len = sizeof(bdp_state[i].bdp_memory_crc_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_SET].req     = &bdp_state[i].bdp_motor_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_SET].rsp     = &bdp_state[i].bdp_motor_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_SET].req_len = sizeof(bdp_state[i].bdp_motor_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_SET].rsp_len = sizeof(bdp_state[i].bdp_motor_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_GET].req     = &bdp_state[i].bdp_motor_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_GET].rsp     = &bdp_state[i].bdp_motor_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_GET].req_len = sizeof(bdp_state[i].bdp_motor_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_MOTOR_GET].rsp_len = sizeof(bdp_state[i].bdp_motor_get_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_SET].req     = &bdp_state[i].bdp_comms_set_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_SET].rsp     = &bdp_state[i].bdp_comms_set_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_SET].req_len = sizeof(bdp_state[i].bdp_comms_set_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_SET].rsp_len = sizeof(bdp_state[i].bdp_comms_set_msg_rsp);

        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_GET].req     = &bdp_state[i].bdp_comms_get_msg_req;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_GET].rsp     = &bdp_state[i].bdp_comms_get_msg_rsp;
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_GET].req_len = sizeof(bdp_state[i].bdp_comms_get_msg_req);
        bdp_state[i].id_data_addr[MSG_ID_BDP_COMMS_GET].rsp_len = sizeof(bdp_state[i].bdp_comms_get_msg_rsp);

    }

    // Verify the length of the structures, it should not exceed the maximum payload size of the BDP
    int j;
    for(j=0;j<MSG_ID_BDP_MAX;j++)
    {
        if(bdp_state[0].id_data_addr[j].req_len > BDP_MSG_PAYLOAD_MAX)
        {
            logError("req structure too large for msg ID: %d", j);
            exit(1);
        }
        if(bdp_state[0].id_data_addr[j].rsp_len > BDP_MSG_PAYLOAD_MAX)
        {
            logError("rsp structure too large for msg ID: %d", j);
            exit(1);
        }
    }

    // set the preamble
    memset(&bdp_dev_info.preamble.preamble[0], 0xAA, BDP_FRAME_PREAMBLE_SIZE);
    // set the start of frame byte
    bdp_dev_info.preamble.sof = BDP_FRAME_SOF;
    // set the broadcast address
    memset(&bdp_dev_info.addr_broadcast, 0xFF, BDP_MSG_ADDR_LENGTH);

    // start the frame thread: this is the frame manager
    int status;
    status = pthread_create(&frame_thread, 0, svsBdpFrameThread, 0);
    if(-1 == status)
    {
        logError("pthread_create: %s", strerror(errno));
        rc = ERR_FAIL;
    }

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    // add robust attribute in case the calling thread dies while inside the critical section
    // this will prevent other processes from being locked, but will generate errors and exit
    // XXX could find a way to recover, but at least other apps will not block
    // see: http://www.embedded-linux.co.uk/tutorial/mutex_mutandis
    pthread_mutexattr_setrobust_np(&mutexAttr, PTHREAD_MUTEX_ROBUST_NP);
    pthread_mutex_init(&mutexFrameNodeAccess, &mutexAttr);

    return(rc);
}

int svsBdpServerUninit(void)
{
    int rc = ERR_PASS;

    close(bdp_dev_info.bdp_bus_dev_info[0].devFd);
    close(bdp_dev_info.bdp_bus_dev_info[1].devFd);

    pthread_mutex_destroy(&mutexFrameNodeAccess);

    return(rc);
}

//
// Description:
// This handler is called when BDP server receives request from client
//
static int svsSocketClientBdpHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;

    //logDebug("%lld", svsTimeGet_us());
    if(hdr == 0)
    {
        logError("hdr null");
        return(ERR_FAIL);
    }

    // Validate msg ID
    if(hdr->u.bdphdr.msg_id >= MSG_ID_BDP_MAX)
    {
        logError("invalid msg ID");
        return(ERR_FAIL);
    }
    // Validate device number
    if(hdr->dev_num != BDP_NUM_ALL)
    {   // not a broadcast
        if(hdr->dev_num >= svsBdpMaxGet())
        {
            logError("invalid device number %d: ", hdr->dev_num);
            return(ERR_FAIL);
        }
    }

    int i, start, end;
    // Update request field
    if(hdr->dev_num == BDP_NUM_ALL)
    {
        start = 0;
        end   = svsBdpMaxGet();
    }
    else
    {
        start = hdr->dev_num;
        end   = hdr->dev_num + 1;
    }
    for(i=start; i<end; i++)
    {
        if(bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req != 0)
        {   // Validate length, in case application has a mismatch with server
            if(hdr->len == bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req_len)
            {
                if(payload != 0)
                {
                    memcpy(bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req, payload, hdr->len);
                }
                else
                {
                    logWarning("payload null");
                }
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

    //logDebug("Server sending BDP %d %s", hdr->dev_num, msgIDToString(hdr->u.bdphdr.msg_id));

    // send message to BDP protocol
    rc = svsBdpTx(sockFd, hdr, payload);
    if(rc != ERR_PASS)
    {
        logError("");
        return(rc);
    }

    return(rc);
}

//
// Description:
// Called when data becomes available to be read from the device BDP protocol
//
int svsSocketClientBdpDev1Handler(int devFd)
{
    return(svsBdpRx(devFd, 0));
}

//
// Description:
// Called when data becomes available to be read from the device BDP protocol
//
int svsSocketClientBdpDev2Handler(int devFd)
{
    return(svsBdpRx(devFd, 1));
}

//
// Convert the virtual address to physical
//
int svsBdpVirtualToPhysical(uint16_t index, bdp_addr_t *addr)
{
    if(addr == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }
    if((index >= svsBdpMaxGet()) && (index != BDP_NUM_ALL))
    {
        logError("index out of range");
        return(ERR_FAIL);
    }

    if(index == BDP_NUM_ALL)
    {   // broadcast address
        memcpy(addr, &bdp_dev_info.addr_broadcast, sizeof(bdp_addr_t));
    }
    else
    {
        memcpy(addr, &bdp_state[index].bdp_addr, sizeof(bdp_addr_t));
    }

    return(ERR_PASS);
}

//
// Convert the physical address to virtual
//
int svsBdpPhysicalToVirtual(bdp_addr_t *addr, uint16_t *index)
{
    int rc = ERR_FAIL;    // assume not found
    uint16_t i;

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

    // check to see for invalid zero address
    bdp_addr_t addr_tmp;
    memset(&addr_tmp, 0, sizeof(bdp_addr_t));
    if(svsBdpAddrMatch(addr, &addr_tmp) == ERR_PASS)
    {
        logError("physical address {0,0,0,0,0,0} not allowed");
        return(ERR_FAIL);
    }
    // check to see for invalid broadcast address
    if(svsBdpAddrMatch(addr, &bdp_dev_info.addr_broadcast) == ERR_PASS)
    {
        logError("physical address BROADCAST not allowed");
        return(ERR_FAIL);
    }

    // lookup index
    for(i=0; i<BDP_MAX; i++)
    {
        if(svsBdpAddrMatch(&bdp_state[i].bdp_addr, addr) == ERR_PASS)
        {
            *index = i;
            rc = ERR_PASS;
            break;
        }
    }

    if(rc != ERR_PASS)
    {
        logWarning("physical address not found: ");
        for(i=0; i<BDP_MSG_ADDR_LENGTH; i++)
        {
            logDebug("%02X ", addr->octet[i]);
        }
    }
    return(rc);
}

int svsBdpAddrMatch(bdp_addr_t *addr1, bdp_addr_t *addr2)
{
    // returns 0 if identical
    if(memcmp(addr1->octet, addr2->octet, BDP_MSG_ADDR_LENGTH) == 0)
    {
        return(ERR_PASS);
    }
    return(ERR_FAIL);
}

int svsBdpArpDiscover(void)
{
    int rc = ERR_PASS;

    // Send all BDPs broadcast message to get the BDP physical address MSG_ID_BDP_ADDR_GET
    // the ARP table will start to be populated as each BDP finds a slot on the bus to send its response
    // we can then query the number of BDPs by calling svsBdpMaxGet()

    return(rc);
}

int svsBdpArpTableUpdate(uint8_t bus, bdp_addr_t *addr)
{
    int rc = ERR_PASS;
    int i;
    int found = 0;

    if(addr == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }
    /*logDebug("Address to update");
    for(i=0; i<BDP_MSG_ADDR_LENGTH; i++)
    {
        logDebug("%02X ", addr->octet[i]);
    }*/

    // check to see for invalid zero address
    bdp_addr_t addr_tmp;
    memset(&addr_tmp, 0, sizeof(bdp_addr_t));
    if(svsBdpAddrMatch(addr, &addr_tmp) == ERR_PASS)
    {
        logError("physical address {0,0,0,0,0,0} not allowed");
        return(ERR_FAIL);
    }
    // check to see for invalid broadcast address
    // occurs when SCU sends a BROADCAST
    if(svsBdpAddrMatch(addr, &bdp_dev_info.addr_broadcast) == ERR_PASS)
    {
        logError("physical address BROADCAST not allowed");
        return(ERR_FAIL);
    }

    // Make sure the new address does not exist
    for(i=0; i<bdp_dev_info.bdp_max; i++)
    {
        if(memcmp(addr->octet, bdp_state[i].bdp_addr.octet, BDP_MSG_ADDR_LENGTH) == 0)
        {   // address already in the table
            found = 1;
            //logDebug("BDP %d address already in the ARP table", i);
            break;
        }
    }

    if(found == 0)
    {
        if(bdp_dev_info.bdp_max >= BDP_MAX)
        {
            logError("BDP number exceeded limit");
            return(ERR_FAIL);
        }

        // Validate address, should not be all 0s nor FFs XXX

        // Address not found in the ARP table, update the ARP table with the new address
        memcpy(&bdp_state[i].bdp_addr, addr, sizeof(bdp_addr_t));
        bdp_state[i].bdp_bus = bus;
        // Update BDP index
        bdp_dev_info.bdp_max++;
        bdp_dev_info.bdp_bus_dev_info[bus].bdp_max++;
        logInfo("BDP %d address updated into ARP table", i);
        logInfo("Total BDP detected: %d", bdp_dev_info.bdp_max);
        logInfo("Total BDP detected on bus 0: %d", bdp_dev_info.bdp_bus_dev_info[0].bdp_max);
        logInfo("Total BDP detected on bus 1: %d", bdp_dev_info.bdp_bus_dev_info[1].bdp_max);
    }

    return(rc);
}

void svsArpTableFlush(void)
{
    int i;

    // Fill address with 0
    for(i=0; i<bdp_dev_info.bdp_max; i++)
    {
        memset(&bdp_state[i].bdp_addr, 0, sizeof(bdp_addr_t));
    }

    bdp_dev_info.bdp_max = 0;
}

//
// Return the total number of physical BDPs detected
//
uint16_t svsBdpMaxGet(void)
{
    return(bdp_dev_info.bdp_max);
}

//
// Description:
// Read bytes from device (RS485)
// Once the full packet is received, send it to the client socket and to the callback handler
//
int svsBdpRx(int devFd, uint8_t bus)
{
#define SVS_BDP_RX_BUF  256
    int rc = ERR_PASS;
    int cnt, byte;
    uint16_t bdp_num = BDP_NUM_INVALID; // some invalid device number
    uint8_t data;
    uint8_t msgID;
    bdp_bus_dev_info_t *bdp_bus = &bdp_dev_info.bdp_bus_dev_info[bus];
    uint8_t buf[SVS_BDP_RX_BUF];

    // get all bytes available, signaled by the select call (blocking read)
    cnt = read(devFd, buf, SVS_BDP_RX_BUF);
    if (cnt <=0)
    {
        if (cnt == 0)
        {   // no data to read
            return(ERR_PASS);
        }
        else
        {
            // some error occured
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

    if(svsTimeGet_us() - bdp_bus->timestamp_rx_activity_us > BDP_MAX_INTER_BYTE_DURATION_US)
    {   // if the time in between bytes is too long, we reset the state machine
        if(bdp_bus->frame_rx_state != BDP_FRAME_STATE_IDLE)
        {
            logWarning("resetting frame_rx_state");
            bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
            STATS_INC(bdp_bus->bus_stats.rx_err_timeout);
        }
    }

    bdp_bus->timestamp_rx_activity_us = svsTimeGet_us(); // update RX timestamp

    //
    // Process all the bytes in the buffer
    // Will not return till all incoming buffer has been exhausted
    //
    for(byte=0; byte<cnt; byte++)
    {
        // Process one byte at a time
        data = buf[byte];

        // We have one byte

        //logDebug("rx %02X", data);
        STATS_INC(bdp_bus->bus_stats.rx_byte_cnt);

        // Process frame
        switch(bdp_bus->frame_rx_state)
        {
            case BDP_FRAME_STATE_IDLE:
                bdp_bus->frame_rx_crc       = 0;
                bdp_bus->frame_rx_index     = 0;
                bdp_bus->preamble_rx_index  = 0;
                bdp_bus->frame_done         = 0;
                bdp_bus->pframe_rx          = (uint8_t *)&bdp_bus->frame_rx;
                bdp_bus->frame_rx_state     = BDP_FRAME_STATE_PREAMBLE;
                // fall through to next state

            case BDP_FRAME_STATE_PREAMBLE:
                if(data == bdp_dev_info.preamble.preamble[bdp_bus->preamble_rx_index])
                {
                    bdp_bus->preamble_rx_index++;
                    if(bdp_bus->preamble_rx_index >= BDP_FRAME_PREAMBLE_SIZE)
                    {
                        bdp_bus->frame_rx_state = BDP_FRAME_STATE_SOF;
                    }
                }
                else
                {   // no preamble detected
                    bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                    //logDebug("no preamble");
                }
                break;

            case BDP_FRAME_STATE_SOF:
                if(data == bdp_dev_info.preamble.sof)
                {
                    bdp_bus->frame_rx_state = BDP_FRAME_STATE_FRAME;
                }
                else
                {   // no SOF detected
                    bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                    STATS_INC(bdp_bus->bus_stats.rx_err_no_sof);
                    //logDebug("no sof");
                }
                break;

            case BDP_FRAME_STATE_FRAME:
                bdp_bus->pframe_rx[bdp_bus->frame_rx_index++] = data;
                // check to see when CRC field becomes available
                if(bdp_bus->frame_rx_index > sizeof(bdp_bus->frame_rx.hdr.crc))
                {   // CRC field received, start computing CRC on following data
                    bdp_bus->frame_rx_crc = crc16_resume_compute(bdp_bus->frame_rx_crc, &data, 1);
                }
                // check to see when len and len_inv fields become available
                if(bdp_bus->frame_rx_index == (sizeof(bdp_bus->frame_rx.hdr.crc) +
                                               sizeof(bdp_bus->frame_rx.hdr.len) +
                                               sizeof(bdp_bus->frame_rx.hdr.len_inv) + 1))
                {   // length and ~length fields received,
                    // validate the length field since we don't have the full frame to use the CRC for validation
                    if(bdp_bus->frame_rx.hdr.len != (~bdp_bus->frame_rx.hdr.len_inv & 0xFFFF))
                    {   // corrupt length
                        logError("corrupt frame length %d\n", bdp_bus->frame_rx.hdr.len);
                        bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                        STATS_INC(bdp_bus->bus_stats.rx_err_length);
                        rc = ERR_LEN_FAIL;
                    }
                    else
                    {   // length field ok, validate it
                        if(bdp_bus->frame_rx.hdr.len > BDP_MSG_PAYLOAD_MAX)
                        {   // invalid length
                            STATS_INC(bdp_bus->bus_stats.rx_err_length_too_long);
                            logError("frame length out of range %d\n", bdp_bus->frame_rx.hdr.len);
                            bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                            rc = ERR_LEN_TOO_LONG;
                        }
                    }
                }
                // check to see if whole frame has been received (header+payload but not the preamble)
                if(bdp_bus->frame_rx_index == (bdp_bus->frame_rx.hdr.len+sizeof(svsMsgBdpFrameHeader_t)))
                {   // we have the whole frame, check the CRC
                    if(bdp_bus->frame_rx_crc != bdp_bus->frame_rx.hdr.crc)
                    {   // CRC invalid
                        STATS_INC(bdp_bus->bus_stats.rx_err_crc);
                        logError("invalid frame CRC computed %X received %X\n", bdp_bus->frame_rx_crc, bdp_bus->frame_rx.hdr.crc);
                        rc = ERR_CRC_FAIL;
                    }
                    else
                    {   // frame is valid, we are done
                        STATS_INC(bdp_bus->bus_stats.rx_frame_cnt);
                        bdp_bus->frame_done = 1;
                        rc = ERR_PASS;
                    }
                    bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                }
                break;

            default:
                logError("unexpected state %d\n", bdp_bus->frame_rx_state);
                bdp_bus->frame_rx_state = BDP_FRAME_STATE_IDLE;
                rc = ERR_UNEXP;
                break;
        }

        // partial frame
        if(bdp_bus->frame_done != 1)
        {   // partial frame received
            if(rc != ERR_PASS)
            {   // use call back to signal error while receiving frame
            }
            // get next character
            continue;
        }

        //
        // full frame received
        //
        bdp_bus->frame_done = 0;
        msgID               = bdp_bus->frame_rx.hdr.msg_id;

        logDebug("Received frame %d sockFd %d %s at %lld ms", bdp_bus->frame_rx.hdr.seq, bdp_bus->frame_rx.hdr.sockFd, msgIDToString(msgID), svsTimeGet_ms());

        if(msgID >= MSG_ID_BDP_MAX)
        {
            logError("invalid msg ID: %d", msgID);
            rc = ERR_FAIL;
            continue;
        }

        if(bdp_dev_info.loopback_enable == 1)
        {   // when in loopback, replace the BROADCAST address by a valid address so as to emulate a real device
            if(svsBdpAddrMatch(&bdp_bus->frame_rx.hdr.addr, &bdp_dev_info.addr_broadcast) == ERR_PASS)
            {
                memset(bdp_bus->frame_rx.hdr.addr.octet, 0xDD, BDP_MSG_ADDR_LENGTH);
            }
        }

        // Check to see if that was an ARP request
        if(msgID == MSG_ID_BDP_ADDR_GET)
        {   // update the ARP table
            rc = svsBdpArpTableUpdate(bus, &bdp_bus->frame_rx.hdr.addr);
            if(rc != ERR_PASS)
            {
                logError("ARP table not updated");
            }
            // we are done
            continue;
        }

        // Get the BDP number from the BDP address
        rc = svsBdpPhysicalToVirtual(&bdp_bus->frame_rx.hdr.addr, &bdp_num);
        if(rc != ERR_PASS)
        {
            logError("BDP number not found in ARP table");
            rc = ERR_FAIL;
        }
        else
        {
            logDebug("Received frame %d from BDP %d", bdp_bus->frame_rx.hdr.seq, bdp_num);

            // we have the BDP number, update the response field
            if(bdp_state[bdp_num].id_data_addr[msgID].rsp != 0)
            {   // validate length
                if(bdp_bus->frame_rx.hdr.len == bdp_state[bdp_num].id_data_addr[msgID].rsp_len)
                {
                    if(bdp_bus->frame_rx.payload != 0)
                    {
                        memcpy(bdp_state[bdp_num].id_data_addr[msgID].rsp, bdp_bus->frame_rx.payload, bdp_bus->frame_rx.hdr.len);
                    }
                    else
                    {
                        logWarning("payload null");
                    }
                }
                else
                {
                    logError("rsp length mismatch %d %d", bdp_bus->frame_rx.hdr.len, bdp_state[bdp_num].id_data_addr[msgID].rsp_len);
                    rc = ERR_FAIL;
                    // when in loopback the response will have the same length as the request, so we ignore this error
                    if(bdp_dev_info.loopback_enable == 1)
                    {
                        rc = ERR_PASS;
                    }
                }
            }
            else
            {
                logError("rsp not initialized");
                rc = ERR_FAIL;
            }

            // update the stats
            bdp_state[bdp_num].msg_stats.rx_cnt++;
        }

        // Check for any errors
        if(rc != ERR_PASS)
        {   // In case of error, send status to callback with a SVS status message
            logError("");
            continue;
        }

        //
        // All is good, send message to the waiting client if any
        //
        if(bdp_bus->frame_rx.hdr.sockFd > 0)
        {   // sockFd retrieved from incoming message
            bdp_node_t node;
            do
            {
                // Find the request associated with this response frame
                rc = svsBdpFrameFindAndRemove(bdp_node_head, bdp_bus->frame_rx.hdr.seq, &node);
                if(rc != ERR_PASS)
                {   // frame received but was not in the list
                    logError("Received unexpected frame %d from BDP %d msgID %s", bdp_bus->frame_rx.hdr.seq, bdp_num, msgIDToString(msgID));
                    rc = ERR_FAIL;
                    break;
                }

                logDebug("Frame %d roundtrip time %lld ms", node.d.frame.hdr.seq, svsTimeGet_ms() - node.d.tsent_ms);

                // check to see if that was the expected response message ID
                // this check is ignored when MSG_ID_BDP_ACK is received (some handler error was returned from the BDP)
                if((node.d.frame.hdr.msg_id != msgID) && (msgID != MSG_ID_BDP_ACK))
                {
                    logError("Unexpected response from BDP %d, msgID %s received instead of msgID %s",
                             bdp_num, msgIDToString(msgID), msgIDToString(node.d.frame.hdr.msg_id));
                    rc = ERR_FAIL;
                    break;
                }

                if((node.d.timeout_ms == 0) && (node.d.callback == 0))
                {   // should not have received a response for this case
                    logError("Response for frame %d %s unexpected for timeout 0 and callback 0", bdp_bus->frame_rx.hdr.seq, msgIDToString(msgID));
                    rc = ERR_FAIL;
                    break;
                }

                if(/*(node.d.timeout_ms == 0) && */ (node.d.callback != 0))
                {   // timeout is 0, but callback set, so we call the callback, client is not waiting for a response
                    logDebug("Calling callback for frame %d %s ", bdp_bus->frame_rx.hdr.seq, msgIDToString(msgID));
                    rc = svsSocketSendCallback(svsCallbackSockFd, MODULE_ID_BDP, bdp_num, msgID, node.d.callback, bdp_bus->frame_rx.payload, bdp_bus->frame_rx.hdr.len);
                    if(rc != ERR_PASS)
                    {
                        logError("svsSocketSendCallback");
                    }
                    break;
                }

                // check to see if the response time was beyond the allowed time, if so we don't sent to client
                int64_t tdiff = svsTimeGet_ms() - node.d.tsent_client_ms;
                if(tdiff >= node.d.timeout_client_ms)
                {
                    logError("Response expired for frame %d %s timeout expected %lld received %lld",
                             bdp_bus->frame_rx.hdr.seq, msgIDToString(msgID), node.d.timeout_client_ms, tdiff);
                    rc = ERR_FAIL;
                    break;;
                }

                // all conditions were met, now we can send the response to the client
                logDebug("Sending response to client socket %d", bdp_bus->frame_rx.hdr.sockFd);
                rc = svsSocketSendBdp(bdp_bus->frame_rx.hdr.sockFd, msgID, bdp_num, 0, 0, 0, 0, bdp_bus->frame_rx.payload, bdp_bus->frame_rx.hdr.len);
                if(rc != ERR_PASS)
                {
                    logError("svsSocketSendBdp: %d", bdp_bus->frame_rx.hdr.sockFd);
                }
            } while(0);
        }
        else
        {   // check to see if the sockFd is 0, this will signify that it is an asynchronous event
            if(bdp_bus->frame_rx.hdr.sockFd == 0)
            {
                // async message received, send message to callback server
                logDebug("Sending async frame %d to callback server", bdp_bus->frame_rx.hdr.seq);
                rc = svsSocketSendCallback(svsCallbackSockFd, MODULE_ID_BDP, bdp_num, msgID, 0, bdp_bus->frame_rx.payload, bdp_bus->frame_rx.hdr.len);
                logDebug("Sent async frame");
                if(rc != ERR_PASS)
                {
                    logError("svsSocketSendCallback");
                }
            }
            else // sockFd < 0, this is a response for a broadcast message
            {
                logError("unexpected response for a broadcast message from BDP %d, msgID %s", bdp_num, msgIDToString(msgID));
                rc = ERR_FAIL;
            }
        }
    } // end for loop

//    logDebug("");

    return(rc);
}

//
// Send payload to physical BDP
//
int svsBdpTx(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc;
    uint16_t crc;
    svsMsgBdpFrameHeader_t frame_hdr;

    logDebug("");

    if(hdr == 0)
    {
        logError("hdr null");
        return(ERR_FAIL);
    }

    if(bdp_dev_info.active_frames >= 255)
    {
        logWarning("Too many active frames %d, frame will be dropped", bdp_dev_info.active_frames);
        return(ERR_BUSY);
    }

    //
    // fill the frame header
    //

    // Get the physical address from BDP number
    rc = svsBdpVirtualToPhysical(hdr->dev_num, &frame_hdr.addr);
    if(rc != ERR_PASS)
    {
        logError("destination address not found");
        return(rc);
    }

    // When in broadcast, we set the sockFd to -1, this way the responses to the client will be dropped
    if(hdr->dev_num == BDP_NUM_ALL)
    {
        frame_hdr.sockFd = -1;
    }
    else
    {
        frame_hdr.sockFd = sockFd;
    }

    //logDebug("timeout_ms %d cb %X flags %X", hdr->timeout_ms, hdr->u.bdphdr.callback, hdr->u.bdphdr.flags);

    frame_hdr.len       = hdr->len;
    frame_hdr.len_inv   = ~frame_hdr.len;
    frame_hdr.msg_id    = hdr->u.bdphdr.msg_id;
    frame_hdr.flags     = hdr->u.bdphdr.flags;
    frame_hdr.seq       = bdp_dev_info.seq_num;
    frame_hdr.timestamp = svsTimeGet_ms();

    // update sequence number for next frame
    bdp_dev_info.seq_num++;
    // limit the sequence number to 255 due to a BDP limitation (fixed in future release)
    // this limit could be removed once all BDPs deployed have been upgraded with the fix and before
    // removing this limitation so to avoid any incompatibilities
    bdp_dev_info.seq_num = bdp_dev_info.seq_num & 0xFF;
    if(bdp_dev_info.seq_num == 0)
    {   // avoid sequence 0, as it represents incoming async frames
        bdp_dev_info.seq_num = 1;
    }

    // compute the CRC which includes the header and the payload but not the preamble
    crc = crc16_compute((uint8_t *)&(frame_hdr.len), sizeof(frame_hdr)-sizeof(frame_hdr.crc));
    crc = crc16_resume_compute(crc, payload, frame_hdr.len);
    frame_hdr.crc = crc;

    // Insert a copy of the frame into the list
    // the periodic thread will take care of sending it
    svsBdpFrameAdd(bdp_node_head, hdr, &frame_hdr, payload);

    //logDebug("");
    return(rc);
}

static int svsBdpWakeup(int devFd, int bus)
{
    int len;
    int64_t tnow;
    tnow = svsTimeGet_us();

    //logDebug("%lld", svsTimeGet_us());

    // Wakeup the BDPs from sleep by sending a few bytes and waiting for at least 4.23ms

    if(((tnow - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_tx_activity_us) > (BDP_TIME_TO_SLEEP_MS * 1000)) &&
       ((tnow - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_rx_activity_us) > (BDP_TIME_TO_SLEEP_MS * 1000)))
    {   // time since we last sent or received any data on the bus exceeded the BDP sleep time
        uint8_t dummy[3];

        // We set this buffer to 0, this value will release the RS485 drivers from the break mode
        memset(dummy, 0x00, sizeof(dummy));

        //logDebug("Waking up...%lld", svsTimeGet_us());

        // send some bytes to wakeup BDPs
        len = svsDevWrite(devFd, dummy, sizeof(dummy));
        if(len < 0)
        {
            logError("svsDevWrite name %s devFd %d",bdp_dev_info.bdp_bus_dev_info[bus].devName, devFd);
            return(ERR_FAIL);
        }
        // wait for BDPs to wakeup
        usleep(BDP_WAKEUP_TIME_MS*1000);
    }

    return(ERR_PASS);
}

static int svsBdpFrameSend(int64_t timeout_ms, uint16_t dev_num, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload, uint16_t payload_len)
{
    int rc = ERR_PASS;
    int rc1, rc2;

    if(dev_num == BDP_NUM_ALL)
    {
        rc1 = svsBdpFrameSendBus(0, timeout_ms, dev_num, frame_hdr, payload, payload_len);
        if(rc1 != ERR_PASS)
        {
            logError("");
            rc = rc1;
        }
        rc2 = svsBdpFrameSendBus(1, timeout_ms, dev_num, frame_hdr, payload, payload_len);
        if(rc2 != ERR_PASS)
        {
            logError("");
            rc = rc2;
        }
    }
    else
    {
        uint8_t bus = bdp_state[dev_num].bdp_bus;
        rc = svsBdpFrameSendBus(bus, timeout_ms, dev_num, frame_hdr, payload, payload_len);
    }

    return(rc);
}

static int svsBdpFrameSendBus(uint8_t bus, int64_t timeout_ms, uint16_t dev_num, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload, uint16_t payload_len)
{
    int rc = ERR_PASS;
    int len;
    int devFd = bdp_dev_info.bdp_bus_dev_info[bus].devFd;

//    logDebug("bus %d", bus);

    if(devFd < 0)
    {
        return(ERR_FAIL);
    }
    if(frame_hdr == 0)
    {
        logError("frame_hdr 0");
        return(ERR_FAIL);
    }
    // Perform backoff
    rc = svsBdpBackoff(bus, timeout_ms);
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    // Wakeup BDPs if asleep
    rc = svsBdpWakeup(devFd, bus);
    if(rc != ERR_PASS)
    {
        goto _svsBdpFrameSendBus;
    }

    //logDebug("Sending BDP %d frame %d %s", dev_num, frame_hdr->seq, msgIDToString(frame_hdr->msg_id));

    // send the preamble
    len = svsDevWrite(devFd, (uint8_t *)&bdp_dev_info.preamble, sizeof(bdp_dev_info.preamble));
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",bdp_dev_info.bdp_bus_dev_info[bus].devName, bdp_dev_info.bdp_bus_dev_info[bus].devFd);
        rc = ERR_FAIL;
        goto _svsBdpFrameSendBus;
    }
    else
    {
        if(len != sizeof(bdp_dev_info.preamble))
        {
            logError("write incomplete: wrote %d instead of %d", len, sizeof(bdp_dev_info.preamble));
            rc = ERR_FAIL;
            goto _svsBdpFrameSendBus;
        }
    }
    // send the header
    len = svsDevWrite(devFd, (uint8_t *)frame_hdr, sizeof(svsMsgBdpFrameHeader_t));
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",bdp_dev_info.bdp_bus_dev_info[bus].devName, bdp_dev_info.bdp_bus_dev_info[bus].devFd);
        rc = ERR_FAIL;
        goto _svsBdpFrameSendBus;
    }
    else
    {
        if(len != sizeof(svsMsgBdpFrameHeader_t))
        {
            logError("write incomplete: wrote %d instead of %d", len, sizeof(svsMsgBdpFrameHeader_t));
            rc = ERR_FAIL;
            goto _svsBdpFrameSendBus;
        }
    }
    // send the payload
    len = svsDevWrite(devFd, payload, payload_len);
    if(len < 0)
    {
        logError("svsDevWrite name %s devFd %d",bdp_dev_info.bdp_bus_dev_info[bus].devName, bdp_dev_info.bdp_bus_dev_info[bus].devFd);
        rc = ERR_FAIL;
        goto _svsBdpFrameSendBus;
    }
    else
    {
        if(len != payload_len)
        {
            logError("write incomplete: wrote %d instead of %d", len, payload_len);
            rc = ERR_FAIL;
            goto _svsBdpFrameSendBus;
        }
    }

    // record the latest but activity
    bdp_dev_info.bdp_bus_dev_info[bus].timestamp_tx_activity_us = svsTimeGet_us();

    _svsBdpFrameSendBus:

    if(rc == ERR_PASS)
    {
        int total_len;
        total_len = sizeof(bdp_dev_info.preamble) + sizeof(svsMsgBdpFrameHeader_t) + payload_len;
        logDebug("Sent %d bytes on bus %d BDP %d frame %d %s at %lld ms", total_len, bus, dev_num, frame_hdr->seq, msgIDToString(frame_hdr->msg_id), svsTimeGet_ms());
    }
    else
    {
        logDebug("Could not send on bus %d BDP %d frame %d %s at %lld ms", bus, dev_num, frame_hdr->seq, msgIDToString(frame_hdr->msg_id), svsTimeGet_ms());
    }

    return(rc);
}

static int svsBdpBackoff(uint8_t bus, int64_t timeout_ms)
{
    int rc = ERR_PASS;
    int64_t delay, tnow;

    // skip this when loopback is enabled
    if(bdp_dev_info.loopback_enable == 1)
    {
        return(rc);
    }

    // Compute inter-frame gap (delay between frames) to reduce bus over utilization
    tnow = svsTimeGet_us();
    // minimum time we last had some bus activity
    delay = MIN((tnow - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_tx_activity_us),
                (tnow - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_rx_activity_us));
    // check if activity is less than allowed time
    if(delay < BDP_INTER_FRAME_GAP_US)
    {   // not enough delay in between packets, compute delay
        delay = BDP_INTER_FRAME_GAP_US - delay;
        logDebug("bus %d throttling %lld us", bus, delay);
        // perform gap delay
        usleep(delay);
    }

#if 1
    uint16_t j = 0;

    // Check for any line activity, if any perform backoff
    int64_t tactivity;
    while((svsTimeGet_us() - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_rx_activity_us) < BDP_MIN_LISTEN_US)
    {
        tactivity = svsTimeGet_us() - bdp_dev_info.bdp_bus_dev_info[bus].timestamp_rx_activity_us;
        logDebug("Backoff cnt %d last tactivity %lld usec", j, tactivity);
        //
        // Compute backoff delay
        //
        STATS_INC(bdp_dev_info.bdp_bus_dev_info[bus].bus_stats.tx_backoff_cnt);
        // generate random delay
        // - add minimum delay (1 byte duration)
        // - add random number that is a fraction of the average frame size
        // - add an exponential factor that keep on increasing with every iteration
        //delay = ((rand() * ((1<<j) - 1) * BDP_FRAME_AVG_TIME_US) / RAND_MAX); // get random value (0..2^j-1) * 1000
        delay = rand();
        delay = (delay * (20*1000)) / RAND_MAX;    // random value from 0..50000
        //logDebug("delay %lld", delay);
        delay = delay * (1<<j);                     // delay = delay * (1,2,4,8...)
        delay = delay + BDP_FRAME_AVG_TIME_US;      // delay at least BDP_FRAME_AVG_TIME_US
        delay = MIN(delay, (50*1000));              // put a maximum limit to avoid large delays

        logDebug("delay %lld us", delay);

        // get the largest delay computed so far
        if(delay > bdp_dev_info.bdp_bus_dev_info[bus].bus_stats.tx_backoff_delay_max)
        {
            bdp_dev_info.bdp_bus_dev_info[bus].bus_stats.tx_backoff_delay_max = delay;
        }

        if((timeout_ms != 0) && ((delay/1000) >= timeout_ms))
        {
            logWarning("Backoff larger than the timeout %lld ms", delay / 1000);
        }

        j++;
        if(j>16)
        {   // too many retries, cannot send packet
            STATS_INC(bdp_dev_info.bdp_bus_dev_info[bus].bus_stats.tx_err_backoff);
            logError("Backoff delay with too many retries");
            rc = ERR_FAIL;
            break;
        }

        // perform backoff delay
        usleep(delay);
    }
#endif
    return(rc);
}

int svsPortOpen(char *devName, int baudrate)
{
    int rc = ERR_PASS;
    int status;
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

    speed_t speed;
    rc = svsBaudRateToSpeed(baudrate, &speed);
    if(rc != ERR_PASS)
    {
        logError("invalid baudrate setting: %d", baudrate);
        return(-1);
    }

    logInfo("Opening port [%s]", devName);

    // Open port for reading and writing,
    // not as controlling TTY as we don't want to kill the thread if CTRL-C is detected (noise)
    // non-blocking as we are blocking in the select call
    // O_SYNC: the write will block until the data has been completely sent
    devFd = open(devName, O_RDWR | O_NONBLOCK | O_NOCTTY | O_NDELAY /*| O_SYNC*/);
    if(-1 == devFd)
    {
        logError("open: %s %s", devName, strerror(errno));
        return(-1);
    }

    // Make the file descriptor asynchronous (the manual page says only
    // O_APPEND and O_NONBLOCK, will work with F_SETFL...)
    // Reference: http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html
    //fcntl(devFd, F_SETFL, FASYNC);

    struct termios portConfig;
    /* Get the current port attributes and modify them */
    status = tcgetattr(devFd, &portConfig);
    if(status < 0)
    {
        logError("tcgetattr: %s", strerror(errno));
        close(devFd);
        return(-1);
    }

    bzero(&portConfig, sizeof(portConfig)); /* clear struct for new port settings */

    /* http://www.easysw.com/~mike/serial/serial.html
    BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
    CRTSCTS : output hardware flow control
    CS8     : 8n1 (8bit,no parity,1 stopbit)
    CLOCAL  : local connection, no modem contol
    CREAD   : enable receiving characters
    */
    portConfig.c_cflag = speed | CS8 | CLOCAL | CREAD;
    // IGNPAR  : ignore bytes with parity errors
    portConfig.c_iflag = IGNPAR | IGNBRK;
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

    tcflush(devFd, TCIOFLUSH);
    status = tcsetattr(devFd, TCSANOW, &portConfig);
    if(-1 == status)
    {
        logError("tcsetattr: %s", strerror(errno));
        rc = ERR_FAIL;
    }

    if(rc != ERR_PASS)
    {
        //logDebug("closing: %s", devName);
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

#if BDP_SIMULATOR
//
// Description:
// This handler is called when BDP server receives request from client
//
static int svsSocketClientBdpSimHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;

    logDebug("BDP simulation handler");

    // Validate msg ID
    if(hdr->u.bdphdr.msg_id >= MSG_ID_BDP_MAX)
    {
        logError("invalid msg ID");
        return(ERR_FAIL);
    }
    // Validate device number
    if(hdr->dev_num != BDP_NUM_ALL)
    {   // not a broadcast
        if(hdr->dev_num >= svsBdpMaxGet())
        {
            logError("invalid device number");
            return(ERR_FAIL);
        }
    }

    int i, start, end;
    // Update request field
    if(hdr->dev_num == BDP_NUM_ALL)
    {
        start = 0;
        end   = svsBdpMaxGet();
    }
    else
    {
        start = hdr->dev_num;
        end   = hdr->dev_num + 1;
    }
    for(i=start; i<end; i++)
    {
        if(bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req != 0)
        {   // Validate length, in case application has a mismatch with server
            if(hdr->len == bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req_len)
            {
                if(payload != 0)
                {
                    memcpy(bdp_state[i].id_data_addr[hdr->u.bdphdr.msg_id].req, payload, hdr->len);
                }
                else
                {
                    logWarning("payload null");
                }
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

    logInfo("Sending message to BDP protocol");

    //
    //  Physical BDP simulator
    //
    uint8_t *prsp;
    uint16_t rsp_len;
    bdp_addr_t bdp_addr;

    bdp_addr.octet[0] = 0x01;
    bdp_addr.octet[1] = 0x02;
    bdp_addr.octet[2] = 0x03;
    bdp_addr.octet[3] = 0x04;
    bdp_addr.octet[4] = 0x05;
    bdp_addr.octet[5] = 0x06;

    switch(hdr->u.bdphdr.msg_id)
    {
        case MSG_ID_BDP_ADDR_GET:
            {
                bdp_address_get_msg_rsp_t rsp;
                prsp = (uint8_t *)&rsp;
                rsp_len = sizeof(rsp);
                rsp.bdp_addr.octet[0] = 0x01;
                rsp.bdp_addr.octet[1] = 0x02;
                rsp.bdp_addr.octet[2] = 0x03;
                rsp.bdp_addr.octet[3] = 0x04;
                rsp.bdp_addr.octet[4] = 0x05;
                rsp.bdp_addr.octet[5] = 0x06;
                memcpy(&bdp_addr, &rsp.bdp_addr, sizeof(bdp_addr));
                rsp.status   = ERR_PASS;
            }
            break;

        case MSG_ID_BDP_UNLOCK_CODE_GET:
            {
                bdp_unlock_code_get_msg_rsp_t rsp;
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
            logError("not implemented message ID: %d", hdr->u.bdphdr.msg_id);
            return(ERR_FAIL);
            break;
    }

    //
    // When data is received
    //
    uint8_t msgID = hdr->u.bdphdr.msg_id;
    uint16_t bdp_num = 0;
    {
        logDebug("frame done: msg ID %s", msgIDToString(msgID));

        if(msgID >= MSG_ID_BDP_MAX)
        {
            logError("invalid msg ID: %d", msgID);
            rc = ERR_FAIL;
        }
        else
        {   // Check to see if that was an ARP request
            if(msgID == MSG_ID_BDP_ADDR_GET)
            {
                rc = svsBdpArpTableUpdate(0, &bdp_addr);
                if(rc != ERR_PASS)
                {
                    logError("ARP table not updated");
                }
            }

            // Get the BDP number from the BDP address
            rc = svsBdpPhysicalToVirtual(&bdp_addr, &bdp_num);
            if(rc != ERR_PASS)
            {
                logError("BDP number not found in ARP table");
            }
            else
            {   // we have the BDP number, update the response field
            }
        }
    }

    rc = svsSocketSendBdp(sockFd, hdr->u.bdphdr.msg_id, bdp_num, 0, 0, 0, 0, prsp, rsp_len);
    if(rc != ERR_PASS)
    {
        logError("svsSocketSendBdp");
    }

    // also send message to callback server
    //logDebug("sending to callback server");
    rc = svsSocketSendCallback(svsCallbackSockFd, MODULE_ID_BDP, bdp_num, hdr->u.bdphdr.msg_id, 0, prsp, rsp_len);
    if(rc != ERR_PASS)
    {
        logError("svsSocketSendCallback");
    }

    return(rc);
}
#endif

int svsBdpPowerGetLocal(uint16_t dev_num, bdp_power_set_msg_req_t **req)
{
    if(dev_num >= BDP_MAX)
    {
        return(ERR_FAIL);
    }
    *req = bdp_state[dev_num].id_data_addr[MSG_ID_BDP_POWER_SET].req;

    return(ERR_PASS);
}

// ---------------------------------------------------------------------------
// Frame management routines
// - Uses linked list to keep copies of frames sent
// - Searches linked lists using the sequence number as the search key
// - Perioically checks for frame timeouts and retries when no response
// ---------------------------------------------------------------------------

// NOTE:
// It would have been simpler to use the libevent or libev libraries
// but the documentation and the amount of effort to make them thread safe was too much
//
// libevent
// To install: sudo apt-get install libevent-dev
// Reference: http://linux.die.net/man/3/event
//            http://blogger.popcnt.org/2007/11/trinity-choice-nsock-over-libevent.html
//            http://www.wangafu.net/~nickm/libevent-book/Ref3_eventloop.html
//
// libev
//            http://linux.die.net/man/3/ev
//            http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#EXAMPLE_PROGRAM
//


//
// This function takes the frame data and adds it to the linked list managing the frames.
// All frames to be sent are added. It is up to the periodic thread and the RX thread to remove them from
// the list.
//
bdp_node_t *svsBdpFrameAdd(bdp_node_t *node_head, svsSocketMsgHeader_t *hdr, svsMsgBdpFrameHeader_t *frame_hdr, uint8_t *payload)
{
    bdp_node_t *node = 0;
    bdp_frame_info_t bdp_frame_info;

//    logDebug("Adding frame %d", frame_hdr->seq);
    if(hdr == 0)
    {
        logError("hdr null pointer");
        return(0);
    }
    if(frame_hdr == 0)
    {
        logError("frame_hdr null pointer");
        return(0);
    }

    memset(&bdp_frame_info, 0, sizeof(bdp_frame_info_t));

    if(hdr->dev_num == BDP_NUM_ALL)
    {
        logDebug("frame with broadcast %d", frame_hdr->seq);
        bdp_frame_info.bcast = 1;
    }

    // update the frame info
    bdp_frame_info.dev_num              = hdr->dev_num;
    bdp_frame_info.state                = 0; // idle
    bdp_frame_info.tsent_client_ms      = hdr->tsent_ms;
    bdp_frame_info.timeout_client_ms    = hdr->timeout_ms;
    bdp_frame_info.tsent_ms             = 0;  // will be updated after sending the frame
    bdp_frame_info.timeout_ms           = hdr->u.bdphdr.timeout_ms;
    bdp_frame_info.callback             = hdr->u.bdphdr.callback;

    // save actual frame going to the BDP
    memcpy(&bdp_frame_info.frame.hdr, frame_hdr, sizeof(svsMsgBdpFrameHeader_t));
    if(payload != 0)
    {
        memcpy(bdp_frame_info.frame.payload, payload, hdr->len);
    }
    else
    {
        logWarning("payload null");
    }

    // Add all of this to the list
    pthread_mutex_lock(&mutexFrameNodeAccess);

    node = svsBdpNodeAddFromTail(node_head, &bdp_frame_info);

    pthread_mutex_unlock(&mutexFrameNodeAccess);

    //logDebug("Added node with frame %d", frame_hdr->seq);

    return(node);
}

//
// Initialize the linked list head.
//
bdp_node_t *svsBdpNodeInit(void)
{
    bdp_node_t *node;

    // create node, this node will not be used to hold data,
    // but helps avoid handling special cases for add and remove
    node = (bdp_node_t *)malloc(sizeof(bdp_node_t));
    if(node == 0)
    {
        logError("malloc failed");
        return(0);
    }

    node->next = 0;
    node->prev = 0;

    return(node);
}

bdp_node_t *svsBdpNodeAddFromHead(bdp_node_t *node_head, bdp_frame_info_t *d)
{
    bdp_node_t *node_new = 0;

    // create node
    node_new = (bdp_node_t *)malloc(sizeof(bdp_node_t));
    if(node_new == 0)
    {
        logError("malloc failed");
        return(node_new);
    }
    if(d == 0)
    {
        logError("d null");
        return(0);
    }

    // add it to the head of the list
    node_new->next = node_head->next;
    node_new->prev = node_head;
    // update the node that was pointing to the head
    if(node_head->next)
    {
        node_head->next->prev = node_new;
    }
    // update head
    node_head->next = node_new;
    // assign data
    memcpy(&node_new->d, d, sizeof(bdp_frame_info_t));

    return(node_new);
}

bdp_node_t *svsBdpNodeAddFromTail(bdp_node_t *node_head, bdp_frame_info_t *d)
{
    bdp_node_t *node = 0;

    if(d == 0)
    {
        logError("d null");
        return(0);
    }

    node = node_head;
    while(node->next != 0)
    {
        node = node->next;
    }

    // create node
    node->next = (bdp_node_t *)malloc(sizeof(bdp_node_t));
    if(node->next == 0)
    {
        logError("malloc failed");
        return(0);
    }

    // add it to the tail of the list
    node->next->prev = node;
    node             = node->next;
    node->next       = 0;

    // assign data
    memcpy(&node->d, d, sizeof(bdp_frame_info_t));

    bdp_dev_info.active_frames++;

    return(node);
}

void svsBdpNodeRemove(bdp_node_t *node_head, bdp_node_t *node)
{
    if(node == 0)
    {
        logDebug("node null");
        return;
    }
    if(node_head == node)
    {   // this is the head node
        logError("cannot delete head");
        return;
    }

    if(node->prev)
    {
        node->prev->next = node->next;
    }
    if(node->next)
    {
        node->next->prev = node->prev;
    }

    memset(node, 0, sizeof(bdp_node_t));

    free(node);

    if(bdp_dev_info.active_frames > 0)
    {
        bdp_dev_info.active_frames--;
    }
}

bdp_node_t *svsBdpNodeFind(bdp_node_t *node_head, uint16_t seq)
{
    bdp_node_t *node_curr;

    if(node_head == 0)
    {
        logError("node_head null");
        return(0);
    }

    node_curr = node_head->next;

    while(node_curr)
    {
        if(node_curr->d.frame.hdr.seq == seq)
        {   // node found
            return(node_curr);
        }
        node_curr = node_curr->next;
    }
    // node not found
    return(0);
}

/*static int svsBdpFrameFind(bdp_node_t *node_head, uint16_t seq, bdp_node_t **pnode, bdp_node_t *node)
{
    logDebug("frame find %d", seq);

    pthread_mutex_lock(&mutexFrameNodeAccess);
    logDebug("lock");
    // lookup node by sequence number
    *pnode = svsBdpNodeFind(node_head, seq);
    if(*pnode == 0)
    {
        return(ERR_FAIL);
    }
    // copy the node so the caller can use the data at any time and avoid any contention from other threads
    memcpy(node, *pnode, sizeof(bdp_node_t));

    pthread_mutex_unlock(&mutexFrameNodeAccess);

    logDebug("timeout %lld retry %d seq %d", node->d.timeout, node->d.retry, node->d.frame.hdr.seq);

    return(ERR_PASS);
}*/

void svsBdpFrameRemove(bdp_node_t *node_head, bdp_node_t *node)
{
    if(node_head == 0)
    {
        logError("node_head null");
        return;
    }

    pthread_mutex_lock(&mutexFrameNodeAccess);

    svsBdpNodeRemove(node_head, node);

    pthread_mutex_unlock(&mutexFrameNodeAccess);

    return;
}

static int svsBdpFrameFindAndRemove(bdp_node_t *node_head, uint16_t seq, bdp_node_t *node)
{
    int rc = ERR_PASS;
    bdp_node_t *pnode;

    pthread_mutex_lock(&mutexFrameNodeAccess);

    if(node_head == 0)
    {
        logDebug("");
        rc = ERR_FAIL;
        goto _svsBdpFrameFindAndRemove;
    }

    // lookup node by sequence number
    pnode = svsBdpNodeFind(node_head, seq);
    if(pnode == 0)
    {
        rc = ERR_FAIL;
        goto _svsBdpFrameFindAndRemove;
    }
    // copy the node so the caller can use the data at any time and avoid any contention from other threads
    if(node)
    {   // copy if not 0
        memcpy(node, pnode, sizeof(bdp_node_t));
    }

    //logDebug("Removing frame %d", pnode->d.frame.hdr.seq);
    svsBdpNodeRemove(node_head, pnode);

    _svsBdpFrameFindAndRemove:
    pthread_mutex_unlock(&mutexFrameNodeAccess);

    return(rc);
}

static void *svsBdpFrameThread(void *arg)
{
    int rc;
    bdp_node_t *node;
    bdp_node_t *temp;

    while(1)
    {
        pthread_mutex_lock(&mutexFrameNodeAccess);

        // check each node if it exceeded its timeout or if it is in the list but not yet active
        node = bdp_node_head->next;

        while(node)
        {
            if(node->d.state == 0)
            {   // frame not sent yet, see if another frame with same dev_num is already active
                temp = bdp_node_head->next;
                uint8_t send = 1;

                while(temp)
                {
                    if(node != temp) // skip this node
                    {
                        // look for an active frame
                        if(temp->d.state == 1)
                        {   // check to see if it has the same dev_num
                            if(temp->d.dev_num == node->d.dev_num)
                            {   // active frame found with same dev_num, we cannot send the frame
                                send = 0;
                                break;
                            }
                        }
                    }
                    // get the next node
                    temp = temp->next;
                }

                if(send == 1)
                {   // no other frame found that has same dev_num and is active, send the frame
                    rc = svsBdpFrameSend(node->d.timeout_ms, node->d.dev_num, &node->d.frame.hdr, node->d.frame.payload, node->d.frame.hdr.len);
                    if(rc != ERR_PASS)
                    {
                        logError("");
                    }
                    // update the frame info, in case we cannot send the frame, we set it as active and retry it as usual...
                    node->d.state    = 1;               // set state to active
                    node->d.tsent_ms = svsTimeGet_ms(); // set time sent
                    node->d.retry    = 0;

                    //
                    // To avoid having stale frames we need to remove the frames from the linked list that have the following
                    // characteristics:
                    // - timeout of 0
                    // - broadcast frames (typically there is no response for those)
                    //
                    // For added cleanup, we could check for stale and active frames.
                    //

                    if(node->d.dev_num == BDP_NUM_ALL)
                    {   // for a broadacst, remove frame from the list
                        logDebug("Removing frame %d", node->d.frame.hdr.seq);
                        temp = node->prev;
                        svsBdpNodeRemove(bdp_node_head, node);
                        node = temp;
                    }
                    else
                    {
                        if((node->d.frame.hdr.flags & BDP_MSG_FRAME_FLAG_NO_ACK) ||
                           (node->d.timeout_ms == 0))
                        {   // for a request with no response, remove frame from the list
                            logDebug("Removing frame %d", node->d.frame.hdr.seq);
                            temp = node->prev;
                            svsBdpNodeRemove(bdp_node_head, node);
                            node = temp;
                        }
                    }
                }
                node = node->next;
                continue;
            }

            //
            //  For the active frames...
            //

            //logDebug("timeout %lld retry %d seq %d", node->d.timeout, node->d.retry, node->d.frame.hdr.seq);

            int64_t tnow;
            tnow = svsTimeGet_ms();

            // check if the timeout has been exceeded
            if((tnow - node->d.tsent_ms) >= node->d.timeout_ms)
            {
                logDebug("Frame %d timed out: diff %lld ms", node->d.frame.hdr.seq, tnow - node->d.tsent_ms);
                //logDebug("timeout %lld ms retry %d seq %d", node->d.timeout_ms, node->d.retry);
                // check if the retry has been exceeded
                if(node->d.retry < BDP_RETRY_MAX)
                {   // not exceeded but timed out, we resend
                    logDebug("Resending frame %d", node->d.frame.hdr.seq);
                    // Send the frame again
                    rc = svsBdpFrameSend(node->d.timeout_ms, node->d.dev_num, &node->d.frame.hdr, node->d.frame.payload, node->d.frame.hdr.len);
                    if(rc != ERR_PASS)
                    {
                        logError("");
                    }
                    node->d.tsent_ms = svsTimeGet_ms();
                    node->d.retry++;
                }
                else
                {   // retried enough times, cannot deliver frame, remove the frame from the list
                    logWarning("No response for frame %d after %d retries", node->d.frame.hdr.seq, node->d.retry);
                    temp = node->prev;
                    svsBdpNodeRemove(bdp_node_head, node);
                    node = temp;
                }
            }

            node = node->next;
        }

        pthread_mutex_unlock(&mutexFrameNodeAccess);

        // process node
        usleep(10*1000);
    }

    return(arg);
}

void svsBdpNodePrint(bdp_node_t *node)
{
    if(node == 0)
    {
        return;
    }

    logDebug("node              %08X", node);
    logDebug("next              %08X", node->next);
    logDebug("prev              %08X", node->prev);
    logDebug("dev_num           %d",   node->d.dev_num);
    logDebug("state             %d",   node->d.state);
    logDebug("tsent             %lld", node->d.tsent_ms);
    logDebug("tsent client      %lld", node->d.tsent_client_ms);
    logDebug("timeout           %lld", node->d.timeout_ms);
    logDebug("timeout client    %lld", node->d.timeout_client_ms);
    logDebug("retry             %d",   node->d.retry);
    logDebug("seq               %d",   node->d.frame.hdr.seq);
    logDebug("bcast             %d",   node->d.bcast);
    logDebug("bcast rsp         %d",   node->d.bcast_rsp);
}


