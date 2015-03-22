#ifndef SVS_MSG_COM_H
#define SVS_MSG_COM_H

//
// Common definitions are defined here
//


//
//
//
#if defined(APP_BDP)
//    #include "svsBdpMsg.h"
#elif defined(APP_KR)
//    #include "svsKrMsg.h"
#elif defined(APP_BOOTLOADER)

#else   // SCU
#endif

#if 0
//
//  In this file we share common definitions for the communication protocol between:
// - BDP
// - KR
// - Bootloader
//

#define MSG_COM_ADDR_LENGTH             6
#define MSG_COM_PAYLOAD_MAX             1024

#define MSG_COM_FRAME_PREAMBLE_SIZE     7
#define MSG_COM_FRAME_SOF               0xAB

// These settings are based on character timing of 0.086ms (115200 baud)
#define MSG_COM_BYTE_TIME_US                    86      // time to send one byte
#define MSG_COM_MIN_BACKOFF_US                  86      // minimum backoff time
#define MSG_COM_MIN_LISTEN_US                   500     // minimum time to listen with no RX activity before transmitting
#define MSG_COM_INTER_FRAME_GAP_US              2000    // inter-frame gap used to limit bus utilization
#define MSG_COM_FRAME_AVG_TIME_US               2000    // frame average time, used in backoff
#define MSG_COM_MAX_INTER_BYTE_DURATION_US      (KR_BYTE_TIME_US * 1000)    // maximum time in between bytes before state machine is reset

typedef enum
{
    MSG_COM_FRAME_STATE_IDLE = 0,
    MSG_COM_FRAME_STATE_PREAMBLE,
    MSG_COM_FRAME_STATE_SOF,
    MSG_COM_FRAME_STATE_FRAME
} msg_com_frame_state_t;

// Common message IDs
#define MSG_COM_ID                          \
    MSG_COM_ID_ALL,                         \   // KEEP FIRST, reserved for registering on all IDs
    MSG_COM_ID_ADDR_GET,                    \   // request to get the KR address, also used for ARP table
    MSG_COM_ID_ADDR_SET,                    \
    MSG_COM_ID_CHANGE_MODE,                 \
    MSG_COM_ID_GET_MODE,                    \
    MSG_COM_ID_FIRMWARE_IMAGE_BLOCK_SEND,   \    // when performing FW upgrades, image is sent in blocks
    MSG_COM_ID_ACK,                         \   // when a message was not recognized or had some errors when received by BDP,
                                            \   // this message will be received by the SCU
    MSG_COM_ID_SN_SET,                      \
    MSG_COM_ID_SN_GET,                      \
    MSG_COM_ID_POWER_SET,                   \
    MSG_COM_ID_POWER_GET,                   \
    MSG_COM_ID_RFID_GET,                    \
    MSG_COM_ID_ECHO,                        \            // echoes data back
    MSG_COM_ID_ECHO_INV,                    \        // echoes inverted data back
    MSG_COM_ID_COMMS_DISABLE,               \   // disable communication on the bus
    MSG_COM_ID_COMMS_ENABLE,                \    // enable communication on the bus
    MSG_COM_ID_PING,                        \
    MSG_COM_ID_RESET,                       \
    MSG_COM_ID_NOP,                         \
    MSG_COM_ID_CONSOLE,                     \
    MSG_COM_ID_LOG,                         \
    MSG_COM_ID_OD,                          \
    MSG_COM_ID_GET_BOOTBLOCK_REV,           \
    MSG_COM_ID_GET_APPLICATION_REV

typedef enum
{
    MSG_COM_FLAG_NO_ACK = 1<<0, // message should not be acknowledged
} msg_com_flags_t;

typedef struct __attribute__ ((__packed__))
{   // KR physical address
    uint8_t  octet[MSG_COM_ADDR_LENGTH];
} msg_com_addr_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t      preamble[MSG_COM_FRAME_PREAMBLE_SIZE];
    uint8_t      sof;      // start of frame
} svsComMsgFramePreamble_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t    crc;        // CRC of frame not including CRC field, keep FIRST field
    uint16_t    len;        // payload length
    uint16_t    len_inv;    // payload length inverted
    kr_addr_t   addr;       // source address when receiving from KR, destination when sending from SCU
    uint8_t     msg_id;     // message id
    int32_t     sockFd;     // client socket FD
    uint8_t     flags;      // acknowledge request...
    uint8_t     seq;        // sequence number
    uint16_t    timestamp;  // timestamp in ms set by SCU
} svsComMsgFrameHeader_t;

typedef struct __attribute__ ((__packed__))
{
    svsComMsgFrameHeader_t      hdr;
    uint8_t                     payload[MSG_COM_PAYLOAD_MAX];
} svsComMsgFrame_t;

typedef struct
{
    uint8_t             backoff_cnt;        // number of times backoffs that occured so far
    int32_t             backoff_delay_max;  // maximum delay for backoffs
    uint32_t            err_backoff;        // failed to send frame due to many backoff/retries
    uint32_t            frame_tx_cnt;       // frames transmitted so far
    uint32_t            frame_rx_cnt;       // frames received so far
    uint32_t            err_length;         // corrupt length counter
    uint32_t            err_crc;            // bad crc counter
    uint32_t            err_tx_queue_full;  // frame TX queue was full while sending
    uint32_t            err_rx_timeout;     // timedout waiting for byte inside a frame
} msg_com_bus_stats_t;

// ------------------------------------------------------------------
//  NOTES ON ADDING NEW KR MESSAGES
// ------------------------------------------------------------------
//
// For each new KR message ID perform the following
//
// 1. Add entry in: kr_msg_id_t
// 2. Create corresponding api, req and rsp structures
// 3. Create corresponding API function call
// 4. Put req and rsp structures into kr_state_t
// 5. Add entry in for loop in svsKrServerInit()
//
// ------------------------------------------------------------------

// ------------------------------------------------------------------

// Total message size cannot exceed KR_MSG_PAYLOAD_MAX,
// so the packet data field should be 1024-sizeof(uint32_t)-sizeof(uint16_t)

#define MSG_COM_FIRMWARE_IMAGE_BLOCK_RESOLUTION 128
#define MSG_COM_FIRMWARE_IMAGE_BLOCK_MAX        (((MSG_COM_PAYLOAD_MAX-sizeof(uint32_t)-sizeof(uint16_t)) / MSG_COM_FIRMWARE_IMAGE_BLOCK_RESOLUTION) * MSG_COM_FIRMWARE_IMAGE_BLOCK_RESOLUTION)

typedef struct
{
    uint8_t          kr_num;
    const char      *filename;
    uint8_t          retries;   // 1-254 retries, 255=infinite, 0 is the same as 1
    int              timeout_erase; // if zero, it will use the value passed in timeout_ms
    int              timeout_normal;
} msg_com_firmware_upgrade_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t    offset;                             // offset in bytes from the start of the bin file
    uint16_t    len;                                // how many bytes in 'data', should always be full
                                                    // unless its the last block.  If it is less then full,
                                                    // this indicates we're done. If the file size happens
                                                    // to be an even multiple of 1000, an extra block of
                                                    // length zero needs to be sent
    uint8_t     data[MSG_COM_FIRMWARE_IMAGE_BLOCK_MAX];  // a chunk of the bin file we're loading into the KR
} msg_com_firmware_upgrade_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} msg_com_firmware_upgrade_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} msg_com_ack_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct
{
    uint8_t             kr_num;
    kr_addr_t           kr_addr;
} kr_address_t;

typedef struct __attribute__ ((__packed__))
{
    kr_addr_t          kr_addr;
} kr_address_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
} kr_address_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} kr_address_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    kr_addr_t          kr_addr;
    uint8_t             status;
} kr_address_get_msg_rsp_t;

// ------------------------------------------------------------------
typedef struct
{
    uint8_t          kr_num;
    bootload_state_t mode;
} kr_change_mode_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t            mode;  // see bootload_state_t for possible values
} kr_change_mode_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
} kr_change_mode_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct
{
    uint8_t          kr_num;
    bootload_state_t mode;    // returned value when status=ERR_PASS
} kr_get_mode_t;

typedef struct __attribute__ ((__packed__))
{
    // nothing for the request side of the packet
} kr_get_mode_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
    uint32_t            mode;  // see bootload_state_t for possible values
} kr_get_mode_msg_rsp_t;

// ------------------------------------------------------------------
typedef struct
{
    uint32_t         rev;    // returned value when status=ERR_PASS
} kr_get_rev_t;

typedef struct __attribute__ ((__packed__))
{
    // nothing for the request side of the packet
} kr_get_rev_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
    uint32_t            rev;   // SW rev as an integer... supplied as a.bb.cc = abbcc
} kr_get_rev_msg_rsp_t;

// ------------------------------------------------------------------

#define MSG_COM_RFID_UID_MAX  32

typedef enum
{
    MSG_COM_RFID_TYPE_ISO15693,
    MSG_COM_RFID_TYPE_ISO14443B,
    MSG_COM_RFID_TYPE_MAX
} msg_com_rfid_type_t;

typedef enum
{
    MSG_COM_RFID_READER_BIKE = 0,
    MSG_COM_RFID_READER_CARD,
    MSG_COM_RFID_READER_MAX
} msg_com_rfid_reader_t;

typedef struct
{
    uint8_t             kr_num;
    kr_rfid_reader_t    rfid_reader;
    kr_rfid_type_t      rfid_type;
    uint8_t             rfid_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_uid[MSG_COM_RFID_UID_MAX]; // UID data received
} kr_rfid_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         rfid_reader;
} kr_rfid_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         rfid_reader;
    uint8_t         rfid_type;
    uint8_t         rfid_uid_len;
    uint8_t         rfid_uid[MSG_COM_RFID_UID_MAX];
    uint8_t         status;
} kr_rfid_get_msg_rsp_t;

// ------------------------------------------------------------------
#endif

#endif // SVS_MSG_COM_H

