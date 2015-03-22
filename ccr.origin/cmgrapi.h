/*
 * cmgrapi.h
 *
 *  Created on: Jul 11, 2013
 *      Author: james
 */

#ifndef CMGRAPI_H_
#define CMGRAPI_H_
#include <stdint.h>
#include "cardrdrdatadefs.h"
#include "callback.h"


void *acquireCardThread(void *arg);
void *msgrCardThread(void *arg);
void ccr_callback_send_signal(ccr_cardMgrResult_t *ccr_result);
void *ccr_callback_thread(void *arg);
int exit_ccr(void);
int stop_ccr(void);
int start_ccr(void);
int ccr_callback_register(callback_fn_t callback);
int ccr_init(void);

int ccr_power_on(void);
int ccr_power_off(void);

#endif /* CMGRAPI_H_ */
