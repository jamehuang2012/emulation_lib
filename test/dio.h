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
 *   dio.h: FPGA/GPIO processing
 */

#ifndef _DIO_H
#define _DIO_H

#include <stdint.h>
//#include "callback.h"
#if 0
typedef unsigned char bool;
#define false 0
#define true  1
#endif


/* Base address of the system configuration register (Syscon) */
#define TS4800_SYSCON_BASE 0xB0010000

/*
 * There is an issue with using the bitmap data structures. Most of them
 * work but a couple do not. In addition, from the time of the initial
 * implementation, some bits that were working no longer seem to work. So
 * until there is time to troubleshoot this issue, the USE_BITMASK define
 * (specified in the Makefile) will override the use of the bitmap structures
 * by instead manipulating the 16-bit values.
 */
#ifdef USE_BITMASK
#define BIT_BDPA_POS    0
#define BIT_BDPB_POS    1
#define BIT_5V_POS      2
#define BIT_MODEM_POS   8
#define BIT_WIM_POS     7
#define BIT_SPARE_POS   5
#define BIT_DOOR_POS    8
#define BIT_SPARESW_POS 9
#define BIT_BUTTON_POS  9

#define MASK_BDPA       (1 << BIT_BDPA_POS)
#define MASK_BDPB       (1 << BIT_BDPB_POS)
#define MASK_5V         (1 << BIT_5V_POS)
#define MASK_MODEM      (1 << BIT_MODEM_POS)
#define MASK_WIM        (1 << BIT_WIM_POS)
#define MASK_SPARE      (1 << BIT_SPARE_POS)
#define MASK_DOOR       (1 << BIT_DOOR_POS)
#define MASK_SPARESW    (1 << BIT_SPARESW_POS)
#define MASK_BUTTON     (1 << BIT_BUTTON_POS)

#define GPIO_IN_BANK_0 16
#define GPIO_IN_BANK_1 20
#define GPIO_IN_BANK_2 24
#define GPIO_IN_BANK_3 28

#define GPIO_OUT_BANK_0 17
#define GPIO_OUT_BANK_1 21
#define GPIO_OUT_BANK_2 25
#define GPIO_OUT_BANK_3 29

#define GPIO_DIR_BANK_0 18
#define GPIO_DIR_BANK_1 22
#define GPIO_DIR_BANK_2 26
#define GPIO_DIR_BANK_3 30
#endif /* USE_BITMASK */
#define DOOR_OPEN       0x01
#define SPARE_ACTIVE    0x02
#define TPB_ACTIVE      0x04


/* Data structures that define the register layout */
typedef struct
{
    unsigned version:4;
    unsigned submodel:4;
    unsigned reserved:4;
    unsigned bootmode:1;
    unsigned sdpower:1;
    unsigned strap_low_signal:1;
    unsigned reset:1;
} __attribute__((packed)) reset_reg_t;

typedef struct
{
    unsigned feed:2;
    unsigned reserved:14;
} __attribute__((packed)) watchdog_reg_t;

typedef struct
{
    unsigned can0_en:1;
    unsigned can1_en:1;
    unsigned xuart0_en:1;
    unsigned xuart3_en:1;
    unsigned xuart4_en:1;
    unsigned xuart5_en:1;
    unsigned ts_en:1;
    unsigned spi_en:1;
    unsigned motor_en:1;
    unsigned sound_en:1;
    unsigned reserved:1;
    unsigned irq5_en:1;
    unsigned irq6_en:1;
    unsigned irq7_en:1;
    unsigned red_led_en:1;
    unsigned grn_led_en:1;
} __attribute__((packed)) misc_reg_t;

typedef struct
{
    unsigned serial_out:1;
    unsigned csn:1;
    unsigned serial_in:1;
    unsigned clock:1;
    unsigned reserved:12;
} __attribute__((packed)) lattice_reg_t;

typedef struct
{
    unsigned mode1_bootstrap:1;
    unsigned mode2_bootstrap:1;
    unsigned scratch:2;
    unsigned reset_sw_en:1;
    unsigned mode3_bootstrap:1;
    unsigned pll_phase:5;
    unsigned reserved:1;
    unsigned xuart4_rs422_en:1;
    unsigned xuart0_rs422_en:1;
    unsigned input_ctr0_en:1;
    unsigned input_ctr1_en:1;
} __attribute__((packed)) mode_reg_t;

typedef struct
{
    uint16_t gpio0:1;
    uint16_t gpio1:1;
    uint16_t gpio2:1;
    uint16_t gpio3:1;
    uint16_t gpio4:1;
    uint16_t gpio5:1;
    uint16_t gpio6:1;
    uint16_t gpio7:1;
    uint16_t gpio8:1;
    uint16_t gpio9:1;
    uint16_t gpio10:1;
    uint16_t gpio11:1;
    uint16_t gpio12:1;
    uint16_t gpio13:1;
    uint16_t gpio14:1;
    uint16_t gpio15:1;
} __attribute__((packed)) gpio_reg_t;

typedef struct
{
    uint16_t        modeid_reg;             /* 0x00 */
    reset_reg_t     reset_reg;              /* 0x02 */
    uint16_t        counter_72MHz_lower;    /* 0x04 */
    uint16_t        counter_72MHz_upper;    /* 0x06 */
    uint16_t        counter_1MHz_lower;     /* 0x08 */
    uint16_t        counter_1MHz_upper;     /* 0x0a */
    uint16_t        random_number;          /* 0x0c */
    watchdog_reg_t  watchdog_reg;           /* 0x0e */
    misc_reg_t      misc_reg;               /* 0x10 */
    uint16_t        muxbus_reg;             /* 0x12 */
    lattice_reg_t   lattice_reg;            /* 0x14 */
    mode_reg_t      mode_reg;               /* 0x16 */
    uint16_t        reserved1;              /* 0x18 */
    uint16_t        input_ctr0_reg;         /* 0x1a */
    uint16_t        input_ctr1_reg;         /* 0x1c */
    uint16_t        test_reg;               /* 0x1e */
    gpio_reg_t      dio_in_0_15;            /* 0x20 */
    gpio_reg_t      dio_out_0_15;           /* 0x22 */
    gpio_reg_t      dio_dir_0_15;           /* 0x24 */
    uint16_t        reserved2;              /* 0x26 */
    gpio_reg_t      dio_in_16_31;           /* 0x28 */
    gpio_reg_t      dio_out_16_31;          /* 0x2a */
    gpio_reg_t      dio_dir_16_31;          /* 0x2c */
    uint16_t        reserved3;              /* 0x2e */
    gpio_reg_t      dio_in_32_47;           /* 0x30 */
    gpio_reg_t      dio_out_32_47;          /* 0x32 */
    gpio_reg_t      dio_dir_32_47;          /* 0x34 */
    uint16_t        reserved4;              /* 0x36 */
    gpio_reg_t      dio_in_48_55;           /* 0x38 - gpio8 - gpio15 unusued */
    gpio_reg_t      dio_out_48_55;          /* 0x3a - gpio8 - gpio15 unusued */
    gpio_reg_t      dio_dir_48_55;          /* 0x3c - gpio8 - gpio15 unusued */
} __attribute__((packed)) fpga_t;

/* Only for simulation usage */
#define DIO_QUEUE_NAME "/usr/scu/bin/dio_queue"
#define DIO_QUEUE_MAX_SIZE 1024

#endif /* _DIO_H */
