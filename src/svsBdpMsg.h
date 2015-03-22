#ifndef SVS_BDP_MSG_H
#define SVS_BDP_MSG_H

#include "svsMsgCom.h"

#define BDP_MSG_ADDR_LENGTH         8
#define BDP_MSG_PAYLOAD_MAX         1024

#define BDP_FRAME_PREAMBLE_SIZE     7
#define BDP_FRAME_SOF               0xAB

#define BDP_WAKEUP_TIME_MS          10      // BDP wakeup within 4.23ms of bus activity so we choose any larger value
#define BDP_TIME_TO_SLEEP_MS        10000   // time BDP takes to go into sleep mode see PM_TIME_TO_SLEEP_MS in pm.h

// These settings are based on character timing of 0.086ms (115200 baud)
#ifndef OVERRIDE_DEFAULT_BITRATE
// These settings are based on character timing of 0.086ms (115200 baud)
    #define BDP_BYTE_TIME_US                  86      // time to send one byte
    #define BDP_MIN_BACKOFF_US                86      // minimum backoff time
#elif (OVERRIDE_DEFAULT_BITRATE == 115200)
// These settings are based on character timing of 0.086ms (115200 baud)
    #define BDP_BYTE_TIME_US                  86      // time to send one byte
    #define BDP_MIN_BACKOFF_US                86      // minimum backoff time
#elif (OVERRIDE_DEFAULT_BITRATE == 38400)
// These settings are based on character timing of 0.260ms (38400 baud)
    #define BDP_BYTE_TIME_US                  260     // time to send one byte
    #define BDP_MIN_BACKOFF_US                260     // minimum backoff time
#else
    #error "unhandled bit rate"
#endif
// backoff related constants used in the app and bootblock
#define BDP_MIN_LISTEN_US                   5000    // minimum time to listen with no RX activity before transmitting
#define BDP_INTER_FRAME_GAP_US              2000    // inter-frame gap used to limit bus utilization
#define BDP_FRAME_AVG_TIME_US               5000    // frame average time, used in backoff
#define BDP_BACKOFF_FRAME_SIZE              7      // 512bits/8
#define BDP_BACKOFF_INTERVAL_US             (BDP_BYTE_TIME_US * BDP_BACKOFF_FRAME_SIZE)
#define BDP_TOTAL_BACKOFF_DELAY_US_MAX      1000000 // 1 second max before we give up waiting for the bus
#define BDP_TX_IDLE_TO_RX_US                (BDP_BYTE_TIME_US * 2)
#define BDP_TX_MAX_RETRY                    10       // 10 retries on a bus collision, or till we've waited too long (see BDP_TOTAL_BACKOFF_DELAY_US_MAX)

// this is the value used by linux
#define BDP_MAX_INTER_BYTE_DURATION_US      (BDP_BYTE_TIME_US * 1000)    // maximum time in between bytes before state machine is reset

// this is the value used by the BDP APP & BOOTBLOCK
#define BDP_MAX_INTER_BYTE_DURATION_MS      100//(((BDP_BYTE_TIME_US * 5) + 1999) / 1000) // should be at least 2ms
#define BDP_MAX_TIMEOUT_MS                  100     // maximum time waiting for no characters

typedef enum
{
    BDP_FRAME_STATE_IDLE = 0,
    BDP_FRAME_STATE_PREAMBLE,
    BDP_FRAME_STATE_SOF,
    BDP_FRAME_STATE_FRAME
} bdp_frame_state_t;

typedef enum
{
    //
    // We need to make sure the definitions don't change, as that will create a mismatch and communication would fail
    //

    //
    // COMMON TO BOOTLOADER, BDP, KR
    //
    MSG_ID_BDP_ALL,                         // KEEP FIRST, reserved for registering on all IDs
    MSG_ID_BDP_ADDR_GET,                    // request to get the BDP address, also used for ARP table
    MSG_ID_BDP_ADDR_SET,
    MSG_ID_BDP_CHANGE_MODE,
    MSG_ID_BDP_GET_MODE,
    MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND,   // when performing application FW upgrades, image is sent in blocks
    MSG_ID_BDP_ACK,                         // when a message was not recognized or had some errors when received by BDP,
    // this message will be received by the SCU
    MSG_ID_BDP_ECHO,                        // echoes data back
    MSG_ID_BDP_ECHO_INV,                    // echoes inverted data back
    MSG_ID_BDP_COMMS_SET,                   // disable/enable communication on the bus
    MSG_ID_BDP_COMMS_GET,
    MSG_ID_BDP_PING,
    MSG_ID_BDP_RESET,
    MSG_ID_BDP_NOP,
    MSG_ID_BDP_CONSOLE,
    MSG_ID_BDP_LOG,
    MSG_ID_BDP_OD,

    MSG_ID_BDP_GET_BOOTBLOCK_REV,
    MSG_ID_BDP_GET_APPLICATION_REV,
    MSG_ID_BDP_GET_STATS,

    MSG_ID_BDP_SN_SET,                      // NOT IN USE: but canno remove will cause incompatibility
    MSG_ID_BDP_SN_GET,                      // NOT IN USE: but canno remove will cause incompatibility

    //
    // ADD MESSAGE IDs after this...
    //

    MSG_ID_BDP_POWER_SET,
    MSG_ID_BDP_POWER_GET,

    MSG_ID_BDP_RFID_GET,

    MSG_ID_BDP_JTAG_DEBUG_SET,

    MSG_ID_BDP_RED_LED_SET,
    MSG_ID_BDP_RED_LED_GET,
    MSG_ID_BDP_GRN_LED_SET,
    MSG_ID_BDP_GRN_LED_GET,
    MSG_ID_BDP_YLW_LED_SET,
    MSG_ID_BDP_YLW_LED_GET,

    MSG_ID_BDP_SWITCH_GET,

    MSG_ID_BDP_UNLOCK_CODE_GET,   // returns series of KEY presses the user has entered

    MSG_ID_BDP_BUZZER_SET,
    MSG_ID_BDP_BUZZER_GET,

    MSG_ID_BDP_DOCK_GET,                  // NOT IN USE
    MSG_ID_BDP_BIKE_LOCK_SET,
    MSG_ID_BDP_BIKE_LOCK_GET,

    MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD,    // when performing _bootblock_ FW upgrades, image is sent in blocks
    MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL,   // unlike the application, the bootblock doesn't get updated until this is received
    MSG_ID_BDP_MEMORY_CRC,

    MSG_ID_BDP_MOTOR_SET,
    MSG_ID_BDP_MOTOR_GET,

    MSG_ID_BDP_RFID_ALL_GET,
    MSG_ID_BDP_SWITCH_BUSTED_GET,
    MSG_ID_BDP_COMPATIBILITY_GET,
    MSG_ID_BDP_COMPATIBILITY_SET,
    MSG_ID_BDP_UNLOCK_CODE_BIKE_GET,    // returns series of KEY presses along with bike RFID

    MSG_ID_BDP_MAX // KEEP LAST

} bdp_msg_id_t;

typedef enum
{
    BDP_MSG_FRAME_FLAG_NO_ACK = 1<<0, // message should not be acknowledged
    BDP_MSG_FRAME_FLAG_BDP    = 1<<1, // message incoming from other BDP devices
} bdp_msg_frame_flags_t;

#define BDP_MSG_FRAME_FLAGS_MASK  (0x00)

typedef enum
{
    BDP_MSG_CTRL_PROMIS     = 1<<0, // promiscuous, accept all messages withough checking for address
    BDP_MSG_CTRL_NO_BACKOFF = 1<<1, // disable backoff
    BDP_MSG_CTRL_BROADCAST  = 1<<2, // use broadcast address instead of local
    BDP_MSG_CTRL_TX_DISABLE = 1<<3, // disable sending any data
} bdp_msg_ctrl_flags_t;

#define BDP_MSG_CTRL_FLAGS_MASK  (0x00)

typedef struct __attribute__ ((__packed__))
{   // BDP physical address
    uint8_t  octet[BDP_MSG_ADDR_LENGTH];
} bdp_addr_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t      preamble[BDP_FRAME_PREAMBLE_SIZE];
    uint8_t      sof;      // start of frame
} svsMsgBdpFramePreamble_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t    crc;        // CRC of frame not including CRC field, keep FIRST field
    uint16_t    len;        // payload length
    uint16_t    len_inv;    // payload length inverted
    bdp_addr_t  addr;       // source address when receiving from BDP, destination when sending from SCU
    uint8_t     msg_id;     // message id
    int32_t     sockFd;     // client socket FD
    uint8_t     flags;      // acknowledge request...
    uint16_t    seq;        // sequence number
    uint16_t    timestamp;  // timestamp in ms set by SCU
} svsMsgBdpFrameHeader_t;

typedef struct __attribute__ ((__packed__))
{
    svsMsgBdpFrameHeader_t      hdr;
    uint8_t                     payload[BDP_MSG_PAYLOAD_MAX];
} svsMsgBdpFrame_t;

typedef struct
{
    uint32_t            tx_backoff_cnt;         // number of times backoffs that occured so far
    int32_t             tx_backoff_delay_max;   // maximum delay for backoffs
    uint32_t            tx_err_backoff;         // failed to send frame due to many backoff/retries
    uint32_t            tx_frame_cnt;           // frames transmitted so far

    uint32_t            rx_byte_cnt;            // bytes received so far
    uint32_t            rx_frame_cnt;           // frames received so far
    uint32_t            rx_err_length;          // corrupt length counter
    uint32_t            rx_err_length_too_long; // packet length out of range
    uint32_t            rx_err_crc;             // bad crc counter
    uint32_t            rx_err_timeout;         // timedout waiting for byte inside a frame
    uint32_t            rx_err_no_sof;          // no SOF detected after receiving the preamble
} bdp_bus_stats_t;

// ------------------------------------------------------------------
//  NOTES ON ADDING NEW BDP MESSAGES
// ------------------------------------------------------------------
//
// For each new BDP message ID perform the following
//
// 1. Add entry in: bdp_msg_id_t
// 2. Create corresponding api, req and rsp structures
// 3. Create corresponding API function call
// 4. Put req and rsp structures into bdp_state_t
// 5. Add entry in for loop in svsBdpServerInit()
//
// ------------------------------------------------------------------

// ------------------------------------------------------------------
//  BDP
// ------------------------------------------------------------------

typedef enum
{
    BDP_LED_STATE_OFF,
    BDP_LED_STATE_ON
} bdp_led_state_t;

typedef enum
{
    BDP_LED_COLOR_GREEN,
    BDP_LED_COLOR_RED,
    BDP_LED_COLOR_YELLOW,
    BDP_LED_COLOR_MAX,
} bdp_led_color_t;

typedef struct // API structure
{
    uint8_t         bdp_num;
    bdp_led_color_t bdp_led_color;
    bdp_led_state_t bdp_led_state;
} bdp_led_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t bdp_led_state; // bdp_led_state_t, using uint8_t instead
} bdp_led_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t status;
} bdp_led_set_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    BDP_SWITCH_STATE_OFF,
    BDP_SWITCH_STATE_ON
} bdp_switch_state_t;

typedef enum
{
    BDP_SWITCH_BIKE,
    BDP_SWITCH_BUSTED,
    BDP_SWITCH_CARD,
    BDP_SWITCH_OPEN,    // limit switch
    BDP_SWITCH_CLOSED,  // limit switch
    BDP_SWITCH_MAX,
} bdp_switch_type_t;

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_switch_state_t  bdp_switch_state[BDP_SWITCH_MAX];
} bdp_switch_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_switch_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t bdp_switch_state[BDP_SWITCH_MAX];
} bdp_switch_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_switch_state_t  state;
} bdp_switch_busted_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_switch_busted_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t state;
} bdp_switch_busted_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct // API structure
{
    uint32_t        rev;   // SW rev as an integer... supplied as aa.bb.cc = aabbcc
    uint8_t         bdp_num;
} bdp_compatibility_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t        rev;   // SW rev as an integer... supplied as aa.bb.cc = aabbcc
} bdp_compatibility_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} bdp_compatibility_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_compatibility_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t        rev;   // SW rev as an integer... supplied as aa.bb.cc = aabbcc
    uint8_t         status;
} bdp_compatibility_get_msg_rsp_t;

// ------------------------------------------------------------------

#define BDP_SN_MAX  6
typedef struct // API structure
{
    uint8_t         bdp_num;
    uint8_t         sn[BDP_SN_MAX];
} bdp_sn_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         sn[BDP_SN_MAX];
} bdp_sn_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} bdp_sn_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_sn_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         sn[BDP_SN_MAX];
} bdp_sn_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    MODULE_POWER_STATE_NOT_AVAILABLE,
    MODULE_POWER_STATE_ON,
    MODULE_POWER_STATE_OFF,
    MODULE_POWER_STATE_STANDBY
} module_power_state_t;

typedef struct // API structure
{
    uint8_t                 bdp_num;
    module_power_state_t    bdp_power_state;
} bdp_power_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t   bdp_power_state;
} bdp_power_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} bdp_power_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_power_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t   bdp_power_state;
    uint8_t   status;
} bdp_power_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    BDP_BIKE_LOCK_ENGAGE,
    BDP_BIKE_LOCK_DISENGAGE,
    BDP_BIKE_LOCK_FORCE_ENGAGE,
    BDP_BIKE_LOCK_FORCE_DISENGAGE,
    BDP_BIKE_LOCK_UNKNOWN,          // set when performing a get: switches both inactive
    BDP_BIKE_LOCK_INVALID,          // set when performing a get: switches both reporting active
} bdp_bike_lock_state_t;

typedef struct // API structure
{
    uint8_t                 bdp_num;
    bdp_bike_lock_state_t   bdp_bike_lock_state;
} bdp_bike_lock_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t   bdp_bike_lock_state;
} bdp_bike_lock_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} bdp_bike_lock_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_bike_lock_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t  bdp_bike_lock_state;
    uint8_t   status;
} bdp_bike_lock_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    BDP_BUZZER_STATE_OFF,
    BDP_BUZZER_STATE_ON,
    BDP_BUZZER_STATE_BEEP
} bdp_buzzer_state_t;

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_buzzer_state_t  bdp_buzzer_state;
    //int16_t             freq_hz;    // if set to negative, the setting will be ignored
} bdp_buzzer_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          bdp_buzzer_state;
    //int16_t          freq_hz;
} bdp_buzzer_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t                     status;
} bdp_buzzer_set_msg_rsp_t;

// ------------------------------------------------------------------

#define BDP_ECHO_PAYLOAD_MAX    255 //1023

typedef struct // API structure
{
    uint8_t             bdp_num;
    uint8_t             payload[BDP_ECHO_PAYLOAD_MAX];
} bdp_echo_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             payload[BDP_ECHO_PAYLOAD_MAX];
} bdp_echo_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             payload[BDP_ECHO_PAYLOAD_MAX];
    uint8_t             status;
} bdp_echo_msg_rsp_t;

// ------------------------------------------------------------------

#define BDP_UNLOCK_CODE_MAX  5
typedef struct // API structure
{
    uint8_t         bdp_num;
    uint8_t         unlock_code_num;                    // number of keys pressed
    uint8_t         unlock_code[BDP_UNLOCK_CODE_MAX];   // list of keys pressed
} bdp_unlock_code_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_unlock_code_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         unlock_code_num;
    uint8_t         unlock_code[BDP_UNLOCK_CODE_MAX];
    uint8_t         status;
} bdp_unlock_code_get_msg_rsp_t;

// ------------------------------------------------------------------

// Total message size cannot exceed BDP_MSG_PAYLOAD_MAX,
// so the packet data field should be 1024-sizeof(uint32_t)-sizeof(uint16_t)

#define FIRMWARE_IMAGE_BLOCK_RESOLUTION 128
#define FIRMWARE_IMAGE_BLOCK_MAX        (((BDP_MSG_PAYLOAD_MAX-sizeof(uint32_t)-sizeof(uint16_t)) / FIRMWARE_IMAGE_BLOCK_RESOLUTION) * FIRMWARE_IMAGE_BLOCK_RESOLUTION)

typedef struct // API structure
{
    uint8_t          bdp_num;
    const char      *filename;
    uint8_t          retries;   // 1-254 retries, 255=infinite, 0 is the same as 1
    int              timeout_erase; // if zero, it will use the value passed in timeout_ms
    int              timeout_normal;
} bdp_firmware_upgrade_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t    offset;                         // offset in bytes from the start of the bin file
    uint16_t    len;                            // how many bytes in 'data', should always be full
                                                // unless its the last block.  If it is less then full,
                                                // this indicates we're done. If the file size happens
                                                // to be an even multiple of 1000, an extra block of
                                                // length zero needs to be sent
    uint8_t     data[FIRMWARE_IMAGE_BLOCK_MAX]; // a chunk of the bin file we're loading into the BDP
} bdp_firmware_upgrade_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} bdp_firmware_upgrade_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct // API structure
{
    uint8_t             bdp_num;
} bdp_bootblock_install_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             bdp_num;
} bdp_bootblock_install_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} bdp_bootblock_install_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    uint8_t             bdp_num;
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
} bdp_memory_crc_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             bdp_num;
    uint32_t            start_address;
    uint32_t            length_in_bytes;
} bdp_memory_crc_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
    uint16_t      crc;
} bdp_memory_crc_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    uint8_t       status;
} bdp_ack_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_addr_t          bdp_addr;
} bdp_address_t;

typedef struct __attribute__ ((__packed__))
{
    bdp_addr_t          bdp_addr;
} bdp_address_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
} bdp_address_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_address_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    bdp_addr_t          bdp_addr;
    uint8_t             status;
} bdp_address_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    BDP_JTAG_DEBUG_STATE_OFF,
    BDP_JTAG_DEBUG_STATE_ON
} bdp_jtag_debug_state_t;

typedef struct // API structure
{
    uint8_t                 bdp_num;
    bdp_jtag_debug_state_t  bdp_jtag_debug_state;
} bdp_jtag_debug_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          bdp_jtag_debug_state;
} bdp_jtag_debug_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t                     status;
} bdp_jtag_debug_set_msg_rsp_t;

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
// bootload_mode would be set when a particular BDP needs to be
// bootloaded, but hasn't started yet.
//
// bootload_started would indicate that something went wrong with the
// last bootload (ie, it didn't complete, but somehow we got rebooted)
//
// upgrade_mode would get set when the SCU broadcast the 'everyone go
// quiet' message on the bus... thus if that BDP gets rebooted
// incorrectly, it will be able to power up into application mode, but
// not transmit anything until told it can.
//
// bootload_done indicate the bootload has successfully completed a
// bootload.  When running the application, this will be the same as
// 'upgrade_mode', in that the bus should remain quiet until the SVS
// sets the value back to 'application_mode'
typedef enum
{
    RIDE_DEBUG_MODE  = 0x00000000,
    BOOTLOAD_MODE    = 0x45686524,
    APPLICATION_MODE = 0x97414666,
    UPGRADE_MODE     = 0x34657823,
    BOOTLOAD_STARTED = 0x87436574,
    BOOTLOAD_DONE    = 0xABC56DE8
} bootload_state_t;
typedef struct // API structure
{
    uint8_t          bdp_num;
    bootload_state_t mode;
} bdp_change_mode_t;

typedef struct __attribute__ ((__packed__))
{
    uint32_t            mode;  // see bootload_state_t for possible values
} bdp_change_mode_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
} bdp_change_mode_msg_rsp_t;

typedef struct // API structure
{
    uint8_t          bdp_num;
    bootload_state_t mode;    // returned value when status=ERR_PASS
} bdp_get_mode_t;

typedef struct __attribute__ ((__packed__))
{
    // nothing for the request side of the packet
} bdp_get_mode_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
    uint32_t            mode;  // see bootload_state_t for possible values
} bdp_get_mode_msg_rsp_t;


// ------------------------------------------------------------------
// since protocol_stats_t isn't part of this header, and I figure this
// could be added to for generic debugging, it seems easier for now
// to have the GET_STATS command just return an array of numbers,
// and it is up to the consummer as to how to interpret them
#define MAX_GET_STATS  32 // maximum number of uint32_t stats returned
typedef struct // API structure
{
    uint8_t          bdp_num;
    uint32_t         stats[MAX_GET_STATS];
} bdp_get_stats_t;

typedef struct __attribute__ ((__packed__))
{
    // nothing for the request side of the packet
} bdp_get_stats_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
    uint32_t            stats[MAX_GET_STATS];
} bdp_get_stats_msg_rsp_t;
// ------------------------------------------------------------------

typedef struct // API structure
{
    uint8_t          bdp_num;
    uint32_t         rev;    // returned value when status=ERR_PASS
} bdp_get_rev_t;

typedef struct __attribute__ ((__packed__))
{
    // nothing for the request side of the packet
} bdp_get_rev_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
    uint32_t            rev;   // SW rev as an integer... supplied as a.bb.cc = abbcc
} bdp_get_rev_msg_rsp_t;

// ------------------------------------------------------------------

#define BDP_RFID_UID_MAX  32

typedef enum
{
    BDP_RFID_TYPE_ISO15693,
    BDP_RFID_TYPE_ISO14443B,
    BDP_RFID_TYPE_MAX
} bdp_rfid_type_t;

typedef enum
{
    BDP_RFID_READER_BIKE = 0,
    BDP_RFID_READER_CARD,
    BDP_RFID_READER_MAX
} bdp_rfid_reader_t;

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_rfid_reader_t   rfid_reader;
    bdp_rfid_type_t     rfid_type;
    uint8_t             rfid_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_uid[BDP_RFID_UID_MAX]; // UID data received
} bdp_rfid_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         rfid_reader;
} bdp_rfid_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         rfid_reader;
    uint8_t         rfid_type;
    uint8_t         rfid_uid_len;
    uint8_t         rfid_uid[BDP_RFID_UID_MAX];
    uint8_t         status;
} bdp_rfid_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct __attribute__ ((__packed__))
{
    // Unlock code
    uint8_t         unlock_code_num;
    uint8_t         unlock_code[BDP_UNLOCK_CODE_MAX];
    uint8_t         unlock_code_status;
    // Bike
    uint8_t         rfid_bike_reader;
    uint8_t         rfid_bike_type;
    uint8_t         rfid_bike_uid_len;               // total number of UID data bytes received
    uint8_t         rfid_bike_uid[BDP_RFID_UID_MAX]; // UID data received
    uint8_t         rfid_bike_status;

} bdp_unlock_code_bike_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef struct // API structure
{
    uint8_t             bdp_num;

    bdp_rfid_reader_t   rfid_bike_reader;
    bdp_rfid_type_t     rfid_bike_type;
    uint8_t             rfid_bike_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_bike_uid[BDP_RFID_UID_MAX]; // UID data received

    bdp_rfid_reader_t   rfid_card_reader;
    bdp_rfid_type_t     rfid_card_type;
    uint8_t             rfid_card_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_card_uid[BDP_RFID_UID_MAX]; // UID data received
} bdp_rfid_all_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_rfid_all_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    // Bike
    uint8_t             rfid_bike_reader;
    uint8_t             rfid_bike_type;
    uint8_t             rfid_bike_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_bike_uid[BDP_RFID_UID_MAX]; // UID data received
    uint8_t             rfid_bike_status;
    // Card
    uint8_t             rfid_card_reader;
    uint8_t             rfid_card_type;
    uint8_t             rfid_card_uid_len;               // total number of UID data bytes received
    uint8_t             rfid_card_uid[BDP_RFID_UID_MAX]; // UID data received
    uint8_t             rfid_card_status;

} bdp_rfid_all_get_msg_rsp_t;

// ------------------------------------------------------------------

#define BDP_CONSOLE_PAYLOAD_MAX    1

typedef struct // API structure
{
    uint8_t             bdp_num;
    uint8_t             payload[BDP_CONSOLE_PAYLOAD_MAX];
} bdp_console_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          payload[BDP_CONSOLE_PAYLOAD_MAX];
} bdp_console_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t          payload[BDP_CONSOLE_PAYLOAD_MAX];
    uint8_t          status;
} bdp_console_msg_rsp_t;

// ------------------------------------------------------------------

// Default values
#define BDP_MOTOR_TIMEOUT_MS            1500
#define BDP_MOTOR_DRIVER_ON_TIMEOUT_MS  500     // time to keep driver on after the first timeout
#define BDP_MOTOR_DRIVER_OFF_TIMEOUT_MS 200     // time to keep driver off
#define BDP_MOTOR_RETRY_AFTER_TIMEOUT   0       // retry count after initial timeout (handles fuse/heat issue)
// set it to 0 to disable this functionality
typedef struct // API structure
{
    uint8_t             bdp_num;
    uint16_t            timeout_ms;             // time to allow motor to be on
    uint16_t            driver_on_timeout;      // time to keep driver on after the first timeout
    uint16_t            driver_off_timeout;     // time to keep driver off
    uint8_t             retry_after_timeout;    // retry count after initial timeout (handles fuse/heat issue)
} bdp_motor_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t            timeout_ms;
    uint16_t            driver_on_timeout;
    uint16_t            driver_off_timeout;
    uint8_t             retry_after_timeout;
} bdp_motor_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t         status;
} bdp_motor_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_motor_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t            timeout_ms;
    uint16_t            driver_on_timeout;
    uint16_t            driver_off_timeout;
    uint8_t             retry_after_timeout;
    uint8_t             status;
} bdp_motor_get_msg_rsp_t;

// ------------------------------------------------------------------

typedef enum
{
    BDP_COMMS_ENABLE = 0,
    BDP_COMMS_DISABLE
} bdp_comms_enable_t;

typedef struct // API structure
{
    uint8_t             bdp_num;
    bdp_comms_enable_t  tx_ctrl;    // enable/disable sending data
} bdp_comms_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t            tx_ctrl;
} bdp_comms_set_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             status;
} bdp_comms_set_msg_rsp_t;

typedef struct __attribute__ ((__packed__))
{
} bdp_comms_get_msg_req_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t             tx_ctrl;
    uint8_t             status;
} bdp_comms_get_msg_rsp_t;

// ------------------------------------------------------------------

#endif // SVS_BDP_MSG_H

