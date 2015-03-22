#ifndef SVSPROTOCOL_BDP_H
#define SVSPROTOCOL_BDP_H

#define PROTOCOL_BDP_DEV_MAX    2

#define FRAME_PAYLOAD_MAX       1024
#define FRAME_PREAMBLE_SIZE     7
#define FRAME_SOF               0xAB

typedef struct
{
    char        devName[PROTOCOL_BDP_DEV_MAX][64];
    int         devFd[PROTOCOL_BDP_DEV_MAX];
    int         baudRate;
    pthread_t   pthreadTx;
    pthread_t   pthreadRx;
    mqd_t       rxQ;
    mqd_t       txQ;

} svs_protocol_bdp_info_t;

typedef struct __attribute__ ((__packed__))
{
    uint8_t      preamble[FRAME_PREAMBLE_SIZE];
    uint8_t      sof;      // start of frame
} frame_preamble_t;

typedef struct __attribute__ ((__packed__))
{
    uint16_t    crc;        // CRC of whole frame not including the preamble (KEEP FIRST FIELD)
    uint16_t    len;        // payload length
    uint16_t    len_inv;    // inverted payload length
    uint8_t     addr_src[ID_MAX];   // source address
    uint8_t     addr_dst[ID_MAX];   // destination address
    uint16_t    timestamp;  // local timestamp when sending (for BDP), remote timestamp when receiving (for SCU)
} frame_hdr_t;

typedef struct __attribute__ ((__packed__))
{
    frame_hdr_t      frame_hdr;
    uint8_t          payload[FRAME_PAYLOAD_MAX];
} frame_rx_t;

typedef enum
{
    PROTOCOL_FRAME_STATE_IDLE,
    PROTOCOL_FRAME_STATE_PREAMBLE,
    PROTOCOL_FRAME_STATE_SOF,
    PROTOCOL_FRAME_STATE_FRAME
} protocol_frame_state_t;


int svsProtocolBdpInit(int flagInit);
int svsProtocolBdpUnInit(void);
int svsProtocolBaudRateToSpeed(int baudrate, speed_t *speed);
int svsProtovolBdpPortOpen(char *devName, int baudrate);

#endif // SVSPROTOCOL_BDP_H
