// --------------------------------------------------------------------------------------
// Module     : CRC
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#ifndef CRC_H
#define CRC_H

#include "svsErr.h"  // for err_t enum

//#define CRC_8_ENABLE
#define CRC_16_ENABLE

void crc16_init(void);
uint16_t crc16_compute(uint8_t *bytes, uint32_t len);
uint16_t crc16_resume_compute(uint16_t prev_crc, uint8_t *bytes, uint32_t len);
err_t    crc16_file(char *filename, uint16_t *returned_crc, uint32_t *returned_length);

void crc8_init(void);
uint16_t crc8_compute(uint8_t *bytes, uint32_t len);
uint16_t crc8_resume_compute(uint8_t prev_crc, uint8_t *bytes, uint32_t len);


#endif // CRC_H
