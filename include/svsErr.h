// --------------------------------------------------------------------------------------
// Module     : SVS_ERR
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#ifndef SVS_ERR_H
#define SVS_ERR_H

typedef enum
{
    ERR_PASS = 0,
    ERR_FAIL,
    ERR_INV_PARAM,
    ERR_INV_MSG_ID,
    ERR_INV_ADDR,
    ERR_BUSY,
    ERR_READY,
    ERR_TIMEOUT,
    ERR_OVERRUN,
    ERR_UNDERRUN,
    ERR_TOO_LONG,
    ERR_TOO_SHORT,
    ERR_MSG_TOO_SHORT,
    ERR_IN_PROGRESS,
    ERR_NO_HANDLER,
    ERR_FILE_DESC,
    ERR_SOCK_DISC,
    ERR_SOCK_FAIL,
    ERR_CRC_FAIL,
    ERR_LEN_FAIL,
    ERR_LEN_TOO_LONG,
    ERR_FIRMWARE_FAILED_ERASE,
    ERR_FIRMWARE_FAILED_PROGRAM,
    ERR_FIRMWARE_FAILED_VERIFY,
    ERR_FIRMWARE_ADDRESS_OUT_OF_RANGE,
    ERR_UNEXP,
    ERR_NOT_IMPLEMENTED,
    ERR_RX_ACTIVITY,
    ERR_DONE,
    ERR_UNKNOWN,
    ERR_COMMS_TIMEOUT,
    ERR_CTRL_C_PRESSED,

} err_t;

#define SVS_ERROR_STR   {ERR_PASS,                          "PASS"},                        \
                        {ERR_FAIL,                          "FAIL"},                        \
                        {ERR_TIMEOUT,                       "TIMEOUT"},                     \
                        {ERR_INV_MSG_ID,                    "INVALID MSG ID"},              \
                        {ERR_INV_PARAM,                     "INVALID PARAM"},               \
                        {ERR_SOCK_DISC,                     "SOCKET DISCONNECT"},           \
                        {ERR_SOCK_FAIL,                     "SOCKET FAILURE"},              \
                        {ERR_CRC_FAIL,                      "CRC FAILURE"},                 \
                        {ERR_LEN_FAIL,                      "LEN FAILURE"},                 \
                        {ERR_LEN_TOO_LONG,                  "LEN TOO LONG"},                \
                        {ERR_UNEXP,                         "UNEXPECTED"},                  \
                        {ERR_NOT_IMPLEMENTED,               "NOT IMPLEMENTED"},             \
                        {ERR_FIRMWARE_FAILED_ERASE,         "FAILED FIRMWARE ERASE"},       \
                        {ERR_FIRMWARE_FAILED_PROGRAM,       "FAILED FIRMWARE PROGRAM"},     \
                        {ERR_FIRMWARE_FAILED_VERIFY,        "FAILED FIRMWARE VERIFY"},      \
                        {ERR_FIRMWARE_ADDRESS_OUT_OF_RANGE, "ADDRESS OUT OF RANGE"},        \
                        {ERR_INV_ADDR,                      "INVALID ADDR"},                \
                        {ERR_COMMS_TIMEOUT,                 "COMMS TIMEOUT"},



typedef struct
{
    err_t   code;
    char    *str;
} svs_err_t;


#endif // SVS_ERR_H
