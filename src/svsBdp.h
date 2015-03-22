#ifndef SVS_BDP_H
#define SVS_BDP_H

#include <termios.h>
#include <svsBdpMsg.h>

#define BDP_NUM_ALL                 (0xFF)      // allows sending to all available BDPs
#define BDP_NUM_INVALID             (0xFEFE)    // used as an invalid BDP number
#define BDP_MAX                     256         // maximum number of BDPs allowed in the system
#define BDP_BUS_DEV_MAX             2           // maximum number of physical buses in the system
#define BDP_BUS_DEV_NAME_MAX        64
#define BDP_DEV_DEFAULT_BAUDRATE    115200
#define BDP_DEV_DEFAULT_NAME1       "" // set to "" when not in use (see configuration file)
#define BDP_DEV_DEFAULT_NAME2       "" // set to "" when not in use (see configuration file)
#define BDP_RETRY_MAX               2
#define BDP_TIMEOUT_MIN_MS          100

typedef struct // socket header
{
    uint8_t         msg_id;     // message id
    uint8_t         flags;      // acknowledge request...
    callback_fn_t   callback;   // callback function requested by client called upon receival of a response
    int64_t         timeout_ms; // timeout for BDP processing (includes message roundtrip)
} svsMsgBdpHeader_t;

typedef struct
{   // stores the req/rsp structure addresses, to be used by message handlers
    // this facilitates the message lookup
    void        *req;       // request
    void        *rsp;       // response
    uint16_t    req_len;    // request length, used for validation
    uint16_t    rsp_len;    // response length, used for validation
} id_data_addr_t;

typedef struct
{
    uint32_t            tx_cnt;
    uint32_t            rx_cnt;
} bdp_msg_stats_t;

typedef struct
{
    // Control and monitoring
    bdp_addr_t                  bdp_addr;   // physical address
    uint8_t                     bdp_bus;    // physical bus
    bdp_msg_stats_t             msg_stats;
    id_data_addr_t              id_data_addr[MSG_ID_BDP_MAX];

    uint8_t                     bdp_valid;          // used to signal the BDP has a valid address/MSN and is not a duplicate

    //
    // Message Data
    // each message ID will have two structures definitions: req and rsp
    //
    bdp_led_set_msg_req_t               bdp_led_red_set_msg_req;
    bdp_led_set_msg_rsp_t               bdp_led_red_set_msg_rsp;
    bdp_led_set_msg_req_t               bdp_led_grn_set_msg_req;
    bdp_led_set_msg_rsp_t               bdp_led_grn_set_msg_rsp;
    bdp_led_set_msg_req_t               bdp_led_ylw_set_msg_req;
    bdp_led_set_msg_rsp_t               bdp_led_ylw_set_msg_rsp;

    bdp_switch_get_msg_req_t            bdp_switch_get_msg_req;
    bdp_switch_get_msg_rsp_t            bdp_switch_get_msg_rsp;

    bdp_switch_busted_get_msg_req_t     bdp_switch_busted_get_msg_req;
    bdp_switch_busted_get_msg_rsp_t     bdp_switch_busted_get_msg_rsp;

    bdp_compatibility_set_msg_req_t     bdp_compatibility_set_msg_req;
    bdp_compatibility_set_msg_rsp_t     bdp_compatibility_set_msg_rsp;
    bdp_compatibility_get_msg_req_t     bdp_compatibility_get_msg_req;
    bdp_compatibility_get_msg_rsp_t     bdp_compatibility_get_msg_rsp;

    bdp_sn_set_msg_req_t                bdp_sn_set_req;
    bdp_sn_set_msg_rsp_t                bdp_sn_set_rsp;
    bdp_sn_get_msg_req_t                bdp_sn_get_req;
    bdp_sn_get_msg_rsp_t                bdp_sn_get_rsp;

    bdp_power_set_msg_req_t             bdp_power_set_msg_req;
    bdp_power_set_msg_rsp_t             bdp_power_set_msg_rsp;
    bdp_power_get_msg_req_t             bdp_power_get_msg_req;
    bdp_power_get_msg_rsp_t             bdp_power_get_msg_rsp;

    bdp_bike_lock_set_msg_req_t         bdp_bike_lock_set_msg_req;
    bdp_bike_lock_set_msg_rsp_t         bdp_bike_lock_set_msg_rsp;
    bdp_bike_lock_get_msg_req_t         bdp_bike_lock_get_msg_req;
    bdp_bike_lock_get_msg_rsp_t         bdp_bike_lock_get_msg_rsp;

    bdp_buzzer_set_msg_req_t            bdp_buzzer_set_msg_req;
    bdp_buzzer_set_msg_rsp_t            bdp_buzzer_set_msg_rsp;

    bdp_echo_msg_req_t                  bdp_echo_msg_req;
    bdp_echo_msg_rsp_t                  bdp_echo_msg_rsp;

    bdp_console_msg_req_t               bdp_console_msg_req;
    bdp_console_msg_rsp_t               bdp_console_msg_rsp;

    bdp_unlock_code_get_msg_req_t       bdp_unlock_code_get_msg_req;
    bdp_unlock_code_get_msg_rsp_t       bdp_unlock_code_get_msg_rsp;

    bdp_unlock_code_bike_get_msg_rsp_t  bdp_unlock_code_bike_get_msg_rsp;

    bdp_rfid_get_msg_req_t              bdp_rfid_get_msg_req;
    bdp_rfid_get_msg_rsp_t              bdp_rfid_get_msg_rsp;

    bdp_rfid_all_get_msg_req_t          bdp_rfid_all_get_msg_req;
    bdp_rfid_all_get_msg_rsp_t          bdp_rfid_all_get_msg_rsp;

    bdp_change_mode_msg_req_t           bdp_change_mode_msg_req;
    bdp_change_mode_msg_rsp_t           bdp_change_mode_msg_rsp;

    bdp_get_mode_msg_req_t              bdp_get_mode_msg_req;
    bdp_get_mode_msg_rsp_t              bdp_get_mode_msg_rsp;

    bdp_firmware_upgrade_msg_req_t      bdp_firmware_upgrade_msg_req;
    bdp_firmware_upgrade_msg_rsp_t      bdp_firmware_upgrade_msg_rsp;

    bdp_ack_msg_rsp_t                   bdp_ack_msg_rsp;

    bdp_address_get_msg_req_t           bdp_address_get_msg_req;
    bdp_address_get_msg_rsp_t           bdp_address_get_msg_rsp;

    bdp_jtag_debug_set_msg_req_t        bdp_jtag_debug_set_msg_req;
    bdp_jtag_debug_set_msg_rsp_t        bdp_jtag_debug_set_msg_rsp;

    bdp_get_rev_msg_req_t               bdp_get_app_rev_msg_req;
    bdp_get_rev_msg_rsp_t               bdp_get_app_rev_msg_rsp;

    bdp_get_rev_msg_req_t               bdp_get_bb_rev_msg_req;
    bdp_get_rev_msg_rsp_t               bdp_get_bb_rev_msg_rsp;

    bdp_get_stats_msg_req_t             bdp_get_stats_msg_req;
    bdp_get_stats_msg_rsp_t             bdp_get_stats_msg_rsp;

    bdp_firmware_upgrade_msg_req_t      bdp_bootblock_upload_msg_req;
    bdp_firmware_upgrade_msg_rsp_t      bdp_bootblock_upload_msg_rsp;

    bdp_bootblock_install_msg_req_t     bdp_bootblock_install_msg_req;
    bdp_bootblock_install_msg_rsp_t     bdp_bootblock_install_msg_rsp;

    bdp_memory_crc_msg_req_t            bdp_memory_crc_msg_req;
    bdp_memory_crc_msg_rsp_t            bdp_memory_crc_msg_rsp;

    bdp_motor_set_msg_req_t             bdp_motor_set_msg_req;
    bdp_motor_set_msg_rsp_t             bdp_motor_set_msg_rsp;
    bdp_motor_get_msg_req_t             bdp_motor_get_msg_req;
    bdp_motor_get_msg_rsp_t             bdp_motor_get_msg_rsp;

    bdp_comms_set_msg_req_t             bdp_comms_set_msg_req;
    bdp_comms_set_msg_rsp_t             bdp_comms_set_msg_rsp;
    bdp_comms_get_msg_req_t             bdp_comms_get_msg_req;
    bdp_comms_get_msg_rsp_t             bdp_comms_get_msg_rsp;

} bdp_state_t;

typedef struct
{
    char                    devName[BDP_BUS_DEV_NAME_MAX];
    int                     devFd;
    int                     baudRate;
    uint16_t                bdp_max;                    // maximum number of available BDP devices on this bus
    svsMsgBdpFrame_t        frame_rx;                   // holds frame data incoming from the BDP bus
    uint8_t                 *pframe_rx;                 // pointer in frame_rx
    uint16_t                preamble_rx_index;          // index of next byte of preamble
    uint16_t                frame_rx_crc;               // frame CRC
    uint16_t                frame_rx_index;             // index of next byte of frame
    uint16_t                frame_rx_remaining;         // remaining
    bdp_frame_state_t       frame_rx_state;             // RX state used for state machine
    uint8_t                 frame_done;                 // frame complete flag
    int64_t                 timestamp_rx_activity_us;   // timestamp of latest bus activity, used for backoff
    int64_t                 timestamp_tx_activity_us;   // timestamp of latest bus activity, used for backoff
    bdp_bus_stats_t         bus_stats;
} bdp_bus_dev_info_t;

typedef struct
{
    bdp_bus_dev_info_t          bdp_bus_dev_info[BDP_BUS_DEV_MAX];
    uint16_t                    bdp_max;    // maximum number of available BDP devices on all buses
    uint16_t                    seq_num;    // message sequence number
    int16_t                     active_frames;    // current number of active frames being managed
    svsMsgBdpFramePreamble_t    preamble;
    bdp_addr_t                  addr_broadcast;
    int                         loopback_enable;    // 0: disable, 1: enable
} bdp_dev_info_t;

typedef struct
{
    uint16_t        dev_num;            // BDP device number
    uint8_t         state;              // frame state: idle=0(awaiting to be sent), active=1(sent and awaiting a timeout)
    int64_t         tsent_client_ms;    // time the client sent the message
    int64_t         timeout_client_ms;  // time client would wait for the response,
                                        // beyond that the server will not send the response to the client
    int64_t         tsent_ms;           // time last sent
    int64_t         timeout_ms;         // time when reached to retry
    uint8_t         retry;              // retry counter
    uint8_t         bcast;              // broadcast frame when 1
    uint8_t         bcast_rsp;          // number of responses to a broadcast request
    callback_fn_t   callback;           // callback function to call upon a response (if not 0)
    svsMsgBdpFrame_t frame;             // the frame to sent and retry upon a timeout
} bdp_frame_info_t;

typedef struct bdp_node
{
    bdp_frame_info_t    d;
    struct bdp_node     *next;
    struct bdp_node     *prev;
} bdp_node_t;


int svsBdpServerInit(void);
int svsBdpServerUninit(void);

int svsBdpVirtualToPhysical(uint16_t index, bdp_addr_t *addr);
int svsBdpPhysicalToVirtual(bdp_addr_t *addr, uint16_t *index);
int svsBdpAddrMatch(bdp_addr_t *addr1, bdp_addr_t *addr2);
int svsBdpArpTableUpdate(uint8_t bus, bdp_addr_t *addr);
void svsArpTableFlush(void);

uint16_t svsBdpMaxGet(void);

int svsDevWrite(int devFd, uint8_t *data, int len);
int svsPortOpen(char *devName, int baudrate);
int svsBaudRateToSpeed(int baudrate, speed_t *speed);

int svsBdpPowerGetLocal(uint16_t dev_num, bdp_power_set_msg_req_t **req);

char *msgIDToString(bdp_msg_id_t id);

// Frame management functions
bdp_node_t *svsBdpNodeInit(void);
bdp_node_t *svsBdpNodeAddFromTail(bdp_node_t *node_head, bdp_frame_info_t *d);
void svsBdpNodeRemove(bdp_node_t *node_head, bdp_node_t *node);
bdp_node_t *svsBdpNodeFind(bdp_node_t *node_head, uint16_t seq);
void svsBdpNodePrint(bdp_node_t *node);

#endif // SVS_BDP_H

