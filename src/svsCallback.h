#ifndef SVS_CALLBACK_H
#define SVS_CALLBACK_H

#define CALLBACK_MSG_PAYLOAD_MAX     1024
#define REGISTERED_APPS_MAX            10
#include "callback.h"
#ifndef callback_fn_t
//typedef int (* callback_fn_t)(uint8_t msg_id, uint8_t dev_num, uint8_t *payload, uint16_t len);
#endif

typedef enum
{
    MSG_ID_CB_APP = 253,

    MSG_ID_CB_MAX
} cb_msg_id_t;

typedef struct
{
    uint8_t         msg_id;     // message id
    callback_fn_t   callback;   // callback function called upon response from device
} svsMsgCallbackHeader_t;

typedef struct
{
    svsMsgCallbackHeader_t  header;
    uint8_t                 payload[CALLBACK_MSG_PAYLOAD_MAX];
} svsMsgCallback_t;

int svsCallbackServerInit(void);
int svsCallbackServerUninit(void);

int svsCallbackInit(void);
int svsCallbackRegister(module_id_t module_id, uint16_t msg_id, callback_fn_t callback);


#endif // SVS_CALLBACK_H

