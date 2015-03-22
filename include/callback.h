#ifndef __CALLBACK__H
#define __CALLBACK__H


#include <stdint.h>

typedef int (* callback_fn_t)(uint8_t msg_id, uint8_t dev_num, uint8_t *payload, uint16_t len);
#endif 
