#ifndef SVSMSG_H
#define SVSMSG_H

/* These are only used if you're using the test rs232Component */
#define MSG_Q_BDP_TX_SIM "/svsBdpTxSimQ"
#define MSG_Q_BDP_RX_SIM "/svsBdpRxSimQ"

#define MSG_Q_BDP_TX     "/svsMsgBdpTxQ"
#define MSG_Q_SCU_TX     "/svsScuTxQ"
#define MSG_Q_CCR_TX     "/svsCcrTxQ"
#define MSG_Q_SCR_TX     "/svsScrTxQ"
#define MSG_Q_PRT_TX     "/svsPrtTxQ"
#define MSG_Q_PMS_TX     "/svsPmsTxQ"
#define MSG_Q_CB         "/svsCbQ"
#define MSG_Q_BDP_RX     "/svsMsgBdpRxQ"

#if 0

/* These values will be in the 8th octet of svsAddr_t */
/* The lower 6 octets will be reserved for bdpID_t */
    #define MSG_ADDR_INVALID  0x00
    #define MSG_ADDR_BDP      0x01
    #define MSG_ADDR_SCU      0x02
    #define MSG_ADDR_WIM      0x04
    #define MSG_ADDR_CCR      0x08
    #define MSG_ADDR_SCR      0x10
    #define MSG_ADDR_PRT      0x20
    #define MSG_ADDR_PMS      0x40
    #define MSG_ADDR_BDP_ALL  0x80

    #define MAX_MSG_PAYLOAD 1024

/* BDP specific definitions */

    #define BDP_BUTTON1_MASK 0x01
    #define BDP_BUTTON2_MASK 0x02
    #define BDP_BUTTON3_MASK 0x04
    #define BDP_BUTTONA_MASK 0x08 // Broken/Aux button

/* Generic MSGs used across all svsAddrs */
    #define SVS_NAK_MSG 0xFF000000

/* BDP->SCU Messages */
    #define BDP_BUTTON_MSG       0x01 // 1-byte mask
    #define BDP_PASSCODE_MSG     0x02 // 5-byte passCode_t
    #define BDP_FW_VERSION       0x03 // 4-byte fwVer_t
    #define BDP_ALARM_MSG        0x04 // 1-byte stateType_t
    #define BDP_RFID_MSG         0x05 // TODO: Need to define the payload length
    #define BDP_BIKE_RFID_MSG    0x06 // TODO: Need to define the payload length

/* SCU->BDP Messages (overlaid on top of svsMsg.payload
   These are also returned to the SCU as ACKs and state
   updates. */
    #define BDP_DOCK_MSG         0x10 // 1-byte stateType_t
    #define BDP_LED_MSG          0x11 // 1-byte ledValType_t
    #define BDP_BUZZER_MSG       0x12 // 1-byte stateType_t
    #define BDP_BUTTON_GET_STATE 0x13
    #define BDP_GET_FW_VERSION   0x15
    #define BDP_UPDATE_FIRMWARE  0xFD
    #define BDP_DISABLE_DEBUG    0xFE
    #define BDP_ENABLE_DEBUG     0xFF

/* WIM->SCU Messages */
    #define WIM_GPS_DATA         0x0100

/* SCU->WIM Messages */
    #define WIM_GET_GPS_DATA     0x1000

/* CCR->SCU Messages */

/* SCU->CCR Messages */

/* Printer->SCU Messages */

/* SCU->Printer Messages */

/* PMS->SCU Messages */

/* SCU->PMS Messages */
#endif

//-----------------------------------------------------------

#define SVS_MSG_PAYLOAD_MAX     (1024*2)


typedef struct //__attribute__ ((__packed__))
{
    //module_id_t     id;
    uint16_t        len;
} svsMsgHeader_t;

typedef struct //__attribute__ ((__packed__))
{
    svsMsgHeader_t  header;
    uint8_t         payload[BDP_MSG_PAYLOAD_MAX];
} svsMsg_t;


int svsMsgInit(int flagInit);
int svsMsgUninit(void);
int svsMsgUninitAll(void);

int svsMsgBdpProcess(int bdp_num, bdp_msg_id_t msgID, uint8_t *req_payload, uint16_t req_len, uint8_t *rsp_payload, uint16_t rsp_len, int timeout_ms);
uint32_t svsMsgBdpSeqNumGet(void);
void svsMsgDump(FILE *stream, svsMsg_t *msg);


#endif /* SVSMSG_H */
