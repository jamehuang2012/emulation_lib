/*
 * Copyright (C) 2012 MapleLeaf Software, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   diomanager.c: FPGA/GPIO processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "dio.h"
#include "logger.h"
#include "callback.h"

#define MONITOR_INTERVAL    250000 /* usec */


/*
 * The functionality provided by this API is only valid for the SCU target
 * to support the FPGA used to control many of the board functions.
 * To that end the conditional compilation around SCU_BUILD
 * will insure that if the source is built for another platform that there
 * will be no negative impact from using this API.
 */
//#ifdef SCU_BUILD
static volatile fpga_t *fpga;
static volatile uint16_t *data;
//#endif /* SCU_BUILD */
static int devmem;
pthread_once_t dio_init_block = PTHREAD_ONCE_INIT;
static pthread_t dio_thread_id = 0;
static uint8_t monitorActive = 0;
static callback_fn_t dio_cb = NULL;

void dumpme(volatile uint16_t *mem)
{
    int i;

    for (i = 0; (i * 2) < 0x3e; ++i)
    {
        if ((i % 8) == 0)
            printf("\n0x%04x: ", i * 2);
        printf("%04x ", mem[i]);
    }
    printf("\n");
    fflush(stdout);
}

int dio_green_led_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
        fpga->misc_reg.grn_led_en = enable;
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_green_led_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
        rVal = (fpga->misc_reg.grn_led_en ? 1:0);
#endif /* SCU_BUILD */

    return rVal;
}

int dio_red_led_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
        fpga->misc_reg.red_led_en = enable;
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_red_led_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
        rVal = (fpga->misc_reg.red_led_en ? 1:0);
#endif /* SCU_BUILD */

    return rVal;
}

int dio_bdp_bus_a_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
    {
#ifdef USE_BITMASK
        if (enable)
            data[GPIO_OUT_BANK_0] |= MASK_BDPA;
        else
            data[GPIO_OUT_BANK_0] &= ~MASK_BDPA;
#else
        fpga->dio_out_0_15.gpio0 = enable;
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_bdp_bus_a_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_0] & MASK_BDPA) >> BIT_BDPA_POS) ? 0:1);
#else
        rVal = (fpga->dio_out_0_15.gpio0 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

int dio_bdp_bus_b_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
    {
#ifdef USE_BITMASK
        if (enable)
            data[GPIO_OUT_BANK_0] |= MASK_BDPB;
        else
            data[GPIO_OUT_BANK_0] &= ~MASK_BDPB;
#else
        fpga->dio_out_0_15.gpio1 = enable;
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_bdp_bus_b_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_0] & MASK_BDPB) >> BIT_BDPB_POS) ? 0:1);
#else
        rVal = (fpga->dio_out_0_15.gpio1 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

int dio_5v_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
        ALOGI("DIO","[%s] dio 5v set...", __func__,enable);
    if (fpga)
    {
#ifdef USE_BITMASK
        if (enable)
            data[GPIO_OUT_BANK_0] |= MASK_5V;
        else
            data[GPIO_OUT_BANK_0] &= ~MASK_5V;
#else
        fpga->dio_out_0_15.gpio2 = enable;
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_5v_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    ALOGI("DIO","[%s] dio 5v get...", __func__);
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_0] & MASK_5V) >> BIT_5V_POS) ? 1:0);
#else
        rVal = (fpga->dio_out_0_15.gpio2 ? 1:0);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

int dio_modem_power_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
    {
#ifdef USE_BITMASK
        ALOGI("DIO","[%s] modem power set..%d.", __func__,enable);
        if (enable)
            data[GPIO_OUT_BANK_2] &= ~(MASK_MODEM);
        else
            data[GPIO_OUT_BANK_2] |= MASK_MODEM;
#else
        fpga->dio_out_32_47.gpio8 = (enable ? 0:1);
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_modem_power_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
        ALOGI("DIO","[%s] modem power get...", __func__);
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_2] & MASK_MODEM) >> BIT_MODEM_POS) ? 0:1);
#else
        rVal = (fpga->dio_out_32_47.gpio8 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

int dio_wim_power_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
    {
#ifdef USE_BITMASK
        if (enable)
            data[GPIO_OUT_BANK_2] &= ~(MASK_WIM);
        else
            data[GPIO_OUT_BANK_2] |= MASK_WIM;
#else
        fpga->dio_out_32_47.gpio7 = (enable ? 0:1);
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_wim_power_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_2] & MASK_WIM) >> BIT_WIM_POS) ? 0:1);
#else
        rVal = (fpga->dio_out_32_47.gpio7 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

int dio_spare_power_set(uint8_t enable)
{
    int rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
    {
#ifdef USE_BITMASK
        if (enable)
            data[GPIO_OUT_BANK_2] &= ~(MASK_SPARE);
        else
            data[GPIO_OUT_BANK_2] |= MASK_SPARE;
#else
        fpga->dio_out_32_47.gpio5 = (enable ? 0:1);
#endif /* USE_BITMASK */
    }
    else
        rVal = -1;
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_spare_power_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_OUT_BANK_2] & MASK_SPARE) >> BIT_SPARE_POS) ? 0:1);
#else
        rVal = (fpga->dio_out_32_47.gpio5 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_door_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_IN_BANK_0] & MASK_DOOR) >> BIT_DOOR_POS) ? 0:1);
#else
        rVal = (fpga->dio_in_0_15.gpio8 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_spare_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_IN_BANK_0] & MASK_SPARESW) >> BIT_SPARESW_POS) ? 0:1);
#else
        rVal = (fpga->dio_in_0_15.gpio9 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

uint8_t dio_pushbutton_get(void)
{
    uint8_t rVal = 0;

#ifdef SCU_BUILD
    if (fpga)
#ifdef USE_BITMASK
        rVal = (((data[GPIO_IN_BANK_2] & MASK_BUTTON) >> BIT_BUTTON_POS) ? 0:1);
#else
        rVal = (fpga->dio_in_32_47.gpio9 ? 0:1);
#endif /* USE_BITMASK */
#endif /* SCU_BUILD */

    return rVal;
}

static uint8_t gpio_changed(uint8_t *state)
{
    uint8_t change_detected = 0;

//#ifdef SCU_BUILD
#if 1
    /* Detect changes in GPIO state */
    if (!fpga->dio_in_0_15.gpio8 &&
        !(*state & DOOR_OPEN))
    {
        *state |= DOOR_OPEN;
        change_detected = 1;
        ALOGI("DIO","[%s] Station door open...", __func__);
    }
    else if (fpga->dio_in_0_15.gpio8 &&
             (*state & DOOR_OPEN))
    {
        *state &= ~DOOR_OPEN;
        change_detected = 1;
        ALOGI("DIO","[%s] Station door closed...", __func__);
    }

    if (!fpga->dio_in_0_15.gpio9 &&
        !(*state & SPARE_ACTIVE))
    {
        *state |= SPARE_ACTIVE;
        change_detected = 1;
        ALOGI("DIO","[%s] Spare power active", __func__);
    }
    else if (fpga->dio_in_0_15.gpio9 &&
             (*state & SPARE_ACTIVE))
    {
        *state &= ~SPARE_ACTIVE;
        change_detected = 1;
        ALOGI("DIO","[%s] Spare power inactive", __func__);
    }

    if (!fpga->dio_in_32_47.gpio9 &&
        !(*state & TPB_ACTIVE))
    {
        *state |= TPB_ACTIVE;
        change_detected = 1;
#ifdef DEBUG
        dumpme(data);
#endif /* DEBUG */
        ALOGI("DIO","[%s] Button active", __func__);
    }
    else if (fpga->dio_in_32_47.gpio9 &&
             (*state & TPB_ACTIVE))
    {
        *state &= ~TPB_ACTIVE;
        change_detected = 1;
#ifdef DEBUG
        dumpme(data);
#endif /* DEBUG */
        ALOGI("DIO","[%s] Button inactive", __func__);
    }
#endif /* SCU_BUILD */
    return change_detected;
}

static int dio_open_fpga(void)
{
#ifdef SCU_BUILD
	
	fprintf(stderr,"dio open fpga");
    devmem = open("/dev/mem", O_RDWR|O_SYNC);
    if (devmem < 0)
    {
        return -1;
    }

    fpga = (fpga_t *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, TS4800_SYSCON_BASE);
    data = (uint16_t *)fpga;
	
    /* Default all GPIO as inputs */
    data[GPIO_DIR_BANK_0] = data[GPIO_DIR_BANK_1] = data[GPIO_DIR_BANK_2] = data[GPIO_DIR_BANK_3] = 0;

    /*
     * Now setup the direction of the DIO's per Joe's layout (1 == output):
     *
     * DIO_00 - OUT_0: Wake up BDP bus A (active high)
     * DIO_01 - OUT_1: Wake up BDP bus B (active high)
     * DIO_02 - OUT_2: Aux 5V supply for WIM and Modem (active high)
     * DIO_23 - OUT_3: Modem power (active low) - actually DIO_40
     * DIO_24 - OUT_4: WIM power (active low)   - actually DIO_39
     * DIO_26 - OUT_5: Spare power (active low) - actually DIO_37
     * DIO_08 - IN_0:  Door switch (1 == door closed)
     * DIO_09 - IN_1:  Spare
     * DIO_22 - IN_2:  Technician mode pushbutton - actually DIO_41
     *
     */
#ifdef USE_BITMASK
    ALOGD("DIO","[%s] data[GPIO_DIR_BANK_0]: %02x, data[GPIO_DIR_BANK_2]: %02x", __func__, data[GPIO_DIR_BANK_0], data[GPIO_DIR_BANK_2]);
    data[GPIO_DIR_BANK_0] |= MASK_BDPA;
    data[GPIO_DIR_BANK_0] |= MASK_BDPB;
    data[GPIO_DIR_BANK_0] |= MASK_5V;
    data[GPIO_DIR_BANK_2] |= MASK_MODEM;
    data[GPIO_DIR_BANK_2] |= MASK_WIM;
    data[GPIO_DIR_BANK_2] |= MASK_SPARE;
    data[GPIO_DIR_BANK_0] &= ~(MASK_DOOR);
    data[GPIO_DIR_BANK_0] &= ~(MASK_SPARESW);
    data[GPIO_DIR_BANK_2] &= ~(MASK_BUTTON);
    ALOGD("DIO","[%s] data[GPIO_DIR_BANK_0]: %02x, data[GPIO_DIR_BANK_2]: %02x", __func__, data[GPIO_DIR_BANK_0], data[GPIO_DIR_BANK_2]);
#else
    fpga->dio_dir_0_15.gpio0 = 1;   /* OUT_0 - DIO_00 */
    fpga->dio_dir_0_15.gpio1 = 1;   /* OUT_1 - DIO_01 */
    fpga->dio_dir_0_15.gpio2 = 1;   /* OUT_2 - DIO_02 */
    fpga->dio_dir_32_47.gpio8 = 1;  /* OUT_3 - DIO_40 */
    fpga->dio_dir_32_47.gpio7 = 1;  /* OUT_4 - DIO_39 */
    fpga->dio_dir_32_47.gpio5 = 1;  /* OUT_5 - DIO_37 */
    fpga->dio_dir_0_15.gpio8 = 0;   /* IN_0  - DIO_08 */
    fpga->dio_dir_0_15.gpio9 = 0;   /* IN_1  - DIO_09 */
    fpga->dio_dir_32_47.gpio9 = 0;  /* IN_2  - DIO_22 */
#endif /* USE_BITMASK */

    /* And finally disable by default */
#ifdef USE_BITMASK
    ALOGD("DIO","data[GPIO_OUT_BANK_0]: %02x, data[GPIO_OUT_BANK_2]: %02x\n", __func__, data[GPIO_OUT_BANK_0], data[GPIO_OUT_BANK_2]);
    data[GPIO_OUT_BANK_0] &= ~(MASK_BDPA);
    data[GPIO_OUT_BANK_0] &= ~(MASK_BDPB);
    data[GPIO_OUT_BANK_0] &= ~(MASK_5V);
    data[GPIO_OUT_BANK_2] |= MASK_MODEM;
    data[GPIO_OUT_BANK_2] |= MASK_WIM;
    data[GPIO_OUT_BANK_2] |= MASK_SPARE;
    ALOGD("DIO","data[GPIO_OUT_BANK_0]: %02x, data[GPIO_OUT_BANK_2]: %02x\n", __func__, data[GPIO_OUT_BANK_0], data[GPIO_OUT_BANK_2]);
#else
    fpga->dio_out_0_15.gpio0 = 0;
    fpga->dio_out_0_15.gpio1 = 0;
    fpga->dio_out_0_15.gpio2 = 0;
    fpga->dio_out_32_47.gpio8 = 1;
    fpga->dio_out_32_47.gpio7 = 1;
    fpga->dio_out_32_47.gpio5 = 1;
#endif /* USE_BITMASK */
#else // SIMU . SHM 
	
    int fd;
    fd = shm_open("shm-dio",O_RDWR |O_CREAT,(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    ftruncate(fd,4096);


    fpga = (fpga_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    data = (uint16_t *)fpga;
	
#endif /* SCU_BUILD */
    return 0;
}

static void dio_close_fpga(void)
{
#ifdef SCU_BUILD
	
	monitorActive = 0;
    	close(devmem);
#endif /* SCU_BUILD */
}


void dio_close(void)
{
#ifdef SCU_BUILD
	
	monitorActive = 0;
    	close(devmem);
#endif /* SCU_BUILD */
}




static void *dio_monitor(void *arg)
{
    uint8_t gpioState = 0;

    ALOGI("DIO","Init dio monitor thread\n");

    /*
     * Create the callback client, this will send the messages
     * to the callback server.
     */


    monitorActive = 1;
    //usleep(500000);

    while (monitorActive)
    {
        if (gpio_changed(&gpioState))
        {

    		ALOGI("DIO","gpio changed\n");
                // call back
                if (dio_cb != NULL) {
                        ALOGD("DIO","gpio State has been changed,invoke callback");
                        (dio_cb) (0, 0, (uint8_t *) & gpioState, sizeof(gpioState));
                }


        }
        usleep(500000);
    }

    dio_cb = NULL;

    dio_close_fpga();
    ALOGI("DIO","Quit from monitor thread\n");
    return arg;
}


/* dio monitor */
/*
 * Initialize routine
 */
void dio_init_routinue(void)
{
        int result;

	fprintf(stderr,"start initialzing fpga\n");
        result = dio_open_fpga();

        if (result < 0 ) {
                        ALOGE("DIO", "Failed to open and configure FPGA");
                }

        /* create the call back thread */
	ALOGD("DIO","Create dio monitor thread");
         if ((result =
                 pthread_create(&dio_thread_id, NULL, dio_monitor,
                                (void *) NULL))) {
         }
         if (result != 0) {
                ALOGE("DIO","Create Thread error %d\n",result);
         }

	fprintf(stderr,"dio_init is done");
}

/*
 * Initialize the dio , create the dio thread
 *
 */
int dio_init(void)
{
         int status;
         status = pthread_once(&dio_init_block, dio_init_routinue);
         if (status != 0) {
                ALOGE("DIO", "init thread fail\n");
                return -1;
         }


        return 0;
}
/*
 * register the callback funtion of dio
 */

int register_dio_callback( callback_fn_t cb_fn)
{
        dio_cb = cb_fn;
        return 0;
}


/* below is test code */


