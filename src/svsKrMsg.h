#ifndef SVS_KR_MSG_H
#define SVS_KR_MSG_H

#include "svsMsgCom.h"

#define KR_MSG_ADDR_LENGTH         8
#define KR_MSG_PAYLOAD_MAX         1024

#define KR_FRAME_PREAMBLE_SIZE     7
#define KR_FRAME_SOF               0xAB

#if 1

// These settings are based on character timing of 0.086ms (115200 baud)
#ifndef OVERRIDE_DEFAULT_BITRATE
  // These settings are based on character timing of 0.086ms (115200 baud)
  #define KR_BYTE_TIME_US                  86      // time to send one byte
  #define KR_MIN_BACKOFF_US                86      // minimum backoff time
#elif (OVERRIDE_DEFAULT_BITRATE == 115200)
  // These settings are based on character timing of 0.086ms (115200 baud)
  #define KR_BYTE_TIME_US                  86      // time to send one byte
  #define KR_MIN_BACKOFF_US                86      // minimum backoff time
#elif (OVERRIDE_DEFAULT_BITRATE == 38400)
  // These settings are based on character timing of 0.260ms (38400 baud)
  #define KR_BYTE_TIME_US                  260     // time to send one byte
  #define KR_MIN_BACKOFF_US                260     // minimum backoff time
#else
  #error "unhandled bit rate"
#endif
// backoff related constants used in the app and bootblock
#define KR_MIN_LISTEN_US                   5000     // minimum time to listen with no RX activity before transmitting
#define KR_INTER_FRAME_GAP_US              2000    // inter-frame gap used to limit bus utilization
#define KR_FRAME_AVG_TIME_US               2000    // frame average time, used in backoff
#define KR_BACKOFF_FRAME_SIZE              7      // 512bits/8
#define KR_BACKOFF_INTERVAL_US             (KR_BYTE_TIME_US * KR_BACKOFF_FRAME_SIZE)
#define KR_TOTAL_BACKOFF_DELAY_US_MAX      1000000 // 1 second max before we give up waiting for the bus
#define KR_TX_IDLE_TO_RX_US                (KR_BYTE_TIME_US * 2)
#define KR_TX_MAX_RETRY                    10       // 10 retries on a bus collision, or till we've waited too long (see KR_TOTAL_BACKOFF_DELAY_US_MAX)

// this is the value used by linux
#define KR_MAX_INTER_BYTE_DURATION_US      (KR_BYTE_TIME_US * 1000)    // maximum time in between bytes before state machine is reset

// this is the value used by the BDP APP & BOOTBLOCK
#define KR_MAX_INTER_BYTE_DURATION_MS      100//(((KR_BYTE_TIME_US * 5) + 1999) / 1000) // should be at least 2ms
#define KR_MAX_TIMEOUT_MS                  200     // maximum time waiting for no characters

#else
// These settings are based on character timing of 0.086ms (115200 baud)
#define KR_BYTE_TIME_US                    86      // time to send one byte
#define KR_MIN_BACKOFF_US                  86      // minimum backoff time
#define KR_MIN_LISTEN_US                   500     // minimum time to listen with no RX activity before transmitting
#define KR_INTER_FRAME_GAP_US              2000    // inter-frame gap used to limit bus utilization
#define KR_FRAME_AVG_TIME_US               2000    // frame average time, used in backoff
#define KR_MAX_INTER_BYTE_DURATION_US      (KR_BYTE_TIME_US * 1000)    // maximum time in between bytes before state machine is reset
#endif
typedef enum
{
    KR_FRAME_STATE_IDLE = 0,
    KR_FRAME_STATE_PREAMBLE,
    KR_FRAME_STATE_SOF,
    KR_FRAME_STATE_FRAME
} kr_frame_state_t;

typedef enum
{
    //
    // COMMON TO BOOTLOADER, BDP, KR
    //
    MSG_ID_KR_ALL,                         // KEEP FIRST, reserved for registering on all IDs
    MSG_ID_KR_ADDR_GET,                    // request to get the KR address, also used for ARP table
    MSG_ID_KR_ADDR_SET,
    MSG_ID_KR_CHANGE_MODE,
    MSG_ID_KR_GET_MODE,
    MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND,   // when performing application FW upgrades, image is sent in blocks
    MSG_ID_KR_ACK,                         // when a message was not recognized or had some errors when received by BDP,
                                            // this message will be received by the SCU
    MSG_ID_KR_ECHO,                        // echoes data back
    MSG_ID_KR_ECHO_INV,                    // echoes inverted data back
    MSG_ID_KR_COMMS_SET,                   // disable/enable communication on the bus
    MSG_ID_KR_COMMS_GET,
    MSG_ID_KR_PING,
    MSG_ID_KR_RESET,
    MSG_ID_KR_NOP,
    MSG_ID_KR_CONSOLE,
    MSG_ID_KR_LOG,
    MSG_ID_KR_OD,

    MSG_ID_KR_GET_BOOTBLOCK_REV,
    MSG_ID_KR_GET_APPLICATION_REV,
    MSG_ID_KR_GET_STATS,

    MSG_ID_KR_SN_SET,
    MSG_ID_KR_SN_GET,

    //
    // ADD MESSAGE IDs after this...
    //

    MSG_ID_KR_POWER_SET,
    MSG_ID_KR_POWER_GET,

    MSG_ID_KR_RFID_GET,

    MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD,    // when performing _bootblock_ FW upgrades, image is sent in blocks
    MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL,   // unlike the application, the bootblock doesn't get updated until this is received
    MSG_ID_KR_MEMORY_CRC,

    MSG_ID_KR_MAX // KEEP LAST

} kr_msg_id_t;

typedef enum
{
    KR_MSG_FLAG_NO_ACK = 1<<0, // message should not be acknowledged
} kr_msg_flags_t;

typedef struct __attribute__ ((__packed__))
{   // KR physical address
    uint8_t  octet[KR_MSG_ADDR_LENGTH];
} kr_addr_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t      preamble[KR_FRAME_PREAMBLE_SIZE];
    uint8_t      sof;      // start of frame
} svsMsgKrFramePreamble_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t    crc;        // CRC of frame not including CRC field, keep FIRST field
    uint16_t    len;        // payload length
    uint16_t    len_inv;    // payload length inverted
    kr_addr_t   addr;       // source address when receiving from KR, destination when sending from SCU
    uint8_t     msg_id;     // message id
    int32_t     sockFd;     // client socket FD
    uint8_t     flags;      // acknowledge request...
    uint16_t    seq;        // sequence number
    uint16_t    timestamp;  // timestamp in ms set by SCU
} svsMsgKrFrameHeader_t;

typedef struct __attribute__ ((__packed__))
{
    svsMsgKrFrameHeader_t       hdr;
    uint8_t                     payload[KR_MSG_PAYLOAD_MAX];
} svsMsgKrFrame_t;

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
} kr_bus_stats_t;

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
//  KR
// ------------------------------------------------------------------

typedef struct
{
    module_power_state_t    kr_power_state;
} kr_power_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t   kr_power_state;
} kr_power_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} kr_power_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} kr_power_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t   kr_power_state;
    uint8_t   status;
} kr_power_get_msg_rsp_t;

// ------------------------------------------------------------------

#define KR_SN_MAX  6
typedef struct
{
    uint8_t         kr_num;
    uint8_t         sn[KR_SN_MAX];
} kr_sn_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         sn[KR_SN_MAX];
} kr_sn_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} kr_sn_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} kr_sn_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         sn[KR_SN_MAX];
} kr_sn_get_msg_rsp_t;

// ------------------------------------------------------------------

#define KR_ECHO_PAYLOAD_MAX    1023

typedef struct
{
    uint8_t             kr_num;
    uint8_t             payload[KR_ECHO_PAYLOAD_MAX];
} kr_echo_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          payload[KR_ECHO_PAYLOAD_MAX];
} kr_echo_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          payload[KR_ECHO_PAYLOAD_MAX];
    uint8_t          status;
} kr_echo_msg_rsp_t;

// ------------------------------------------------------------------

// Total message size cannot exceed KR_MSG_PAYLOAD_MAX,
// so the packet data field should be 1024-sizeof(uint32_t)-sizeof(uint16_t)

//#define FIRMWARE_IMAGE_BLOCK_RESOLUTION 128
//#define FIRMWARE_IMAGE_BLOCK_MAX        (((MSG_PAYLOAD_MAX-sizeof(uint32_t)-sizeof(uint16_t)) / FIRMWARE_IMAGE_BLOCK_RESOLUTION) * FIRMWARE_IMAGE_BLOCK_RESOLUTION)

typedef struct
{
    uint8_t          kr_num;
    const char      *filename;
    uint8_t          retries;   // 1-254 retries, 255=infinite, 0 is the same as 1
    int              timeout_erase; // if zero, it will use the value passed in timeout_ms
    int              timeout_normal;
} kr_firmware_upgrade_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t    offset;                         // offset in bytes from the start of the bin file
    uint16_t    len;                            // how many bytes in 'data', should always be full
                                                // unless its the last block.  If it is less then full,
                                                // this indicates we're done. If the file size happens
                                                // to be an even multiple of 1000, an extra block of
                                                // length zero needs to be sent
    uint8_t     data[FIRMWARE_IMAGE_BLOCK_MAX]; // a chunk of the bin file we're loading into the KR
} kr_firmware_upgrade_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} kr_firmware_upgrade_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct
{
    uint8_t             kr_num;
} kr_bootblock_install_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             kr_num;
} kr_bootblock_install_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} kr_bootblock_install_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    uint8_t             kr_num;
    //
    // start_address can be:
    // 0x08000000 - 0x0801FFFF  all of flash  (length 128K)
    // 0x08000000 - 0x08003FFF  bootblock     (length 16K)
    // 0x08004000 - 0x0801BFFF  application   (length 96K)
    // 0x0801C000 - 0x0801FFFF  temp storage  (length 16K, bootblock temporary storage before install)
    // 0x08080000 - 0x08080FFF  all of EEPROM (length 4K)
    // 0x20000000 - 0x20003FFF  all of RAM    (length 16K)
    uint32_t            start_address;
    //
    // length can be any (reasonable) value
    // note, care must be taken to prevent hard-faults or other
    // memory access violations.  Accessing memory outside of
    // the specified regions may result in undesirable behavior
    uint32_t            length_in_bytes;
    //
    // The returned 16-bit CRC is calculated using the same CRC algorithm
    // that is used for the BDP protocol (see crc.c)
    uint16_t            returned_crc;
} kr_memory_crc_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             kr_num;
    uint32_t            start_address;
    uint32_t            length_in_bytes;
} kr_memory_crc_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
    uint16_t      crc;
} kr_memory_crc_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} kr_ack_msg_rsp_t;

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
// these values are unimportant, but they should be unique and 32bit
// just to make it less likely to accidentally be in the wrong mode
//
// Ride Debug Mode is when Ride is used as a debugger, it appears
// to erase the EEPROM to zero's, thus making the bootloader _think_
// the memory is blank.  This special case allows the bootloader to
// understand that Ride is has just erased the EEPROM and we're
// debugging.  The application will shortly initialize the EEPROM
// with default values, and 'application_mode' can be assumed
//
// application_mode is obvious... everything is running normally
//
// bootload_mode would be set when a particular KR needs to be
// bootloaded, but hasn't started yet.
//
// bootload_started would indicate that something went wrong with the
// last bootload (ie, it didn't complete, but somehow we got rebooted)
//
// upgrade_mode would get set when the SCU broadcast the 'everyone go
// quiet' message on the bus... thus if that KR gets rebooted
// incorrectly, it will be able to power up into application mode, but
// not transmit anything until told it can.
//
// bootload_done indicate the bootload has successfully completed a
// bootload.  When running the application, this will be the same as
// 'upgrade_mode', in that the bus should remain quiet until the SVS
// sets the value back to 'application_mode'

/*typedef enum   {
               RIDE_DEBUG_MODE  = 0x00000000,
               BOOTLOAD_MODE    = 0x45686524,
               APPLICATION_MODE = 0x97414666,
               UPGRADE_MODE     = 0x34657823,
               BOOTLOAD_STARTED = 0x87436574,
               BOOTLOAD_DONE    = 0xABC56DE8
               } bootload_state_t;
               */

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

#define KR_RFID_UID_MAX  32

typedef enum
{
    KR_RFID_TYPE_ISO15693,
    KR_RFID_TYPE_ISO14443B,

    KR_RFID_TYPE_MAX
} kr_rfid_type_t;

typedef enum
{
    KR_RFID_READER_BIKE = 0,
    KR_RFID_READER_CARD,
    KR_RFID_READER_MAX
} kr_rfid_reader_t;

typedef struct
{
    uint8_t             kr_num;
    kr_rfid_reader_t    rfid_reader;
    kr_rfid_type_t      rfid_type;
    uint8_t             rfid_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_uid[KR_RFID_UID_MAX]; // UID data received
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
    uint8_t         rfid_uid[KR_RFID_UID_MAX];
    uint8_t         status;
} kr_rfid_get_msg_rsp_t;

// ------------------------------------------------------------------

#endif // SVS_KR_MSG_H

