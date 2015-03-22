#ifndef SVS_KR_H
#define SVS_KR_H

#include <svsKrMsg.h>

#define KR_NUM_ALL                 (0xFF)  // allows sending to all available KRs
#define KR_MAX                     1
#define KR_BUS_DEV_MAX             1       // physical bus
#define KR_BUS_DEV_NAME_MAX        64
#define KR_DEV_DEFAULT_BAUDRATE    115200
#define KR_DEV_DEFAULT_NAME1       ""   // set to "" when not in use (see configuration file)

typedef struct // socket header
{
    uint8_t     msg_id;    // message id
    uint8_t     flags;     // acknowledge request...
} svsMsgKrHeader_t;


typedef struct
{
    int64_t time_sent;     // time message was sent
    int64_t time_received; // time message was received (msg with same ID and sequence)
    int64_t time_response; // time duration to get a response (received - sent)
} kr_msgid_stats_t;

typedef struct
{
    uint32_t            tx_cnt;
    uint32_t            rx_cnt;
    kr_msgid_stats_t    msgid_stats[MSG_ID_KR_MAX];    // stats per message ID
} kr_msg_stats_t;

typedef struct
{
    // Control and monitoring
    kr_addr_t                   kr_addr;   // physical address
    uint8_t                     kr_bus;    // physical bus
    kr_msg_stats_t              msg_stats;
    id_data_addr_t              id_data_addr[MSG_ID_KR_MAX];

    //
    // Message Data
    // each message ID will have two structures definitions: req and rsp
    //
    kr_sn_set_msg_req_t        kr_sn_set_req;
    kr_sn_set_msg_rsp_t        kr_sn_set_rsp;
    kr_sn_get_msg_req_t        kr_sn_get_req;
    kr_sn_get_msg_rsp_t        kr_sn_get_rsp;

    kr_power_set_msg_req_t     kr_power_set_msg_req;
    kr_power_set_msg_rsp_t     kr_power_set_msg_rsp;
    kr_power_get_msg_req_t     kr_power_get_msg_req;
    kr_power_get_msg_rsp_t     kr_power_get_msg_rsp;

    kr_echo_msg_req_t          kr_echo_msg_req;
    kr_echo_msg_rsp_t          kr_echo_msg_rsp;

    kr_rfid_get_msg_req_t              kr_rfid_get_msg_req;
    kr_rfid_get_msg_rsp_t              kr_rfid_get_msg_rsp;

    kr_change_mode_msg_req_t           kr_change_mode_msg_req;
    kr_change_mode_msg_rsp_t           kr_change_mode_msg_rsp;

    kr_get_mode_msg_req_t              kr_get_mode_msg_req;
    kr_get_mode_msg_rsp_t              kr_get_mode_msg_rsp;

    kr_firmware_upgrade_msg_req_t      kr_firmware_upgrade_msg_req;
    kr_firmware_upgrade_msg_rsp_t      kr_firmware_upgrade_msg_rsp;

    kr_ack_msg_rsp_t                   kr_ack_msg_rsp;

    kr_address_get_msg_req_t           kr_address_get_msg_req;
    kr_address_get_msg_rsp_t           kr_address_get_msg_rsp;

    kr_get_rev_msg_req_t               kr_get_app_rev_msg_req;
    kr_get_rev_msg_rsp_t               kr_get_app_rev_msg_rsp;

    kr_get_rev_msg_req_t               kr_get_bb_rev_msg_req;
    kr_get_rev_msg_rsp_t               kr_get_bb_rev_msg_rsp;

    kr_firmware_upgrade_msg_req_t      kr_bootblock_upload_msg_req;
    kr_firmware_upgrade_msg_rsp_t      kr_bootblock_upload_msg_rsp;

    kr_bootblock_install_msg_req_t     kr_bootblock_install_msg_req;
    kr_bootblock_install_msg_rsp_t     kr_bootblock_install_msg_rsp;

    kr_memory_crc_msg_req_t            kr_memory_crc_msg_req;
    kr_memory_crc_msg_rsp_t            kr_memory_crc_msg_rsp;
} kr_state_t;

typedef struct
{
    char                    devName[KR_BUS_DEV_NAME_MAX];
    int                     devFd;
    int                     baudRate;
    uint8_t                 kr_max;                    // maximum number of available KR devices on this bus
    svsMsgKrFrame_t         frame_rx;                   // holds frame data incoming from the KR bus
    uint8_t                 *pframe_rx;                 // pointer in frame_rx
    uint16_t                preamble_rx_index;          // index of next byte of preamble
    uint16_t                frame_rx_crc;               // frame CRC
    uint16_t                frame_rx_index;             // index of next byte of frame
    uint16_t                frame_rx_remaining;         // remaining
    kr_frame_state_t        frame_rx_state;             // RX state used for state machine
    uint8_t                 frame_done;                 // frame complete flag
    int64_t                 timestamp_rx_activity_us;   // timestamp of latest bus activity, used for backoff
    int64_t                 timestamp_tx_activity_us;   // timestamp of latest bus activity, used for backoff
    kr_bus_stats_t          bus_stats;
} kr_bus_dev_info_t;

typedef struct
{
    kr_bus_dev_info_t           kr_bus_dev_info[KR_BUS_DEV_MAX];
    uint8_t                     kr_max;    // maximum number of available KR devices on all buses
    uint32_t                    seq_num;    // message sequence number
    svsMsgKrFramePreamble_t     preamble;
    kr_addr_t                   addr_broadcast;
    int                         loopback_enable;    // 0: disable, 1: enable
} kr_dev_info_t;

int svsKrServerInit(void);
int svsKrServerUninit(void);

int svsKrVirtualToPhysical(uint8_t index, kr_addr_t *addr);
int svsKrPhysicalToVirtual(kr_addr_t *addr, uint8_t *index);
int svsKrAddrMatch(kr_addr_t *addr1, kr_addr_t *addr2);
int svsKrArpTableUpdate(uint8_t bus, kr_addr_t *addr);
void svsArpTableFlush(void);

uint8_t svsKrMaxGet(void);

#endif // SVS_KR_H

