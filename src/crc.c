// --------------------------------------------------------------------------------------
// Module     : CRC
// Description: CRC check module
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Personica, Inc. www.personica.com
// Copyright (c) 2011, Personica, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <stdint.h>

#include <crc.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef CRC_16_ENABLE

static const uint16_t polynomial = 0xA001;
static uint16_t table[256];

//----------------------------------------------------------------------------
// Functions: crc16_init ()
// Description:
// Initialize the CRC table
// Reference: http://sanity-free.org/134/standard_crc_16_in_csharp.html
//----------------------------------------------------------------------------
void crc16_init(void)
{
    uint16_t value;
    uint16_t temp, i;
    uint8_t j;

    for(i = 0; i < sizeof(table)/sizeof(uint16_t); i++)
    {
        value = 0;
        temp  = i;

        for(j = 0; j < 8; j++)
        {
            if(((value ^ temp) & 0x0001) != 0)
            {
                value = (uint16_t)((value >> 1) ^ polynomial);
            }
            else
            {
                value >>= 1;
            }
            temp >>= 1;
        }
        table[i] = value;
    }
}

//----------------------------------------------------------------------------
// Functions: crc16_compute ()
//
// Description:
//
// Args
// bytes: pointer to buffer
// len: length of buffer
//
// Return
// 16-bit CRC value
//----------------------------------------------------------------------------
uint16_t crc16_compute(uint8_t *bytes, uint32_t len)
{
    uint16_t crc = 0;
    uint32_t i;
    uint8_t  index;

    for(i = 0; i < len; i++)
    {
        index = (uint8_t)(crc ^ bytes[i]);
        crc   = (uint16_t)((crc >> 8) ^ table[index]);
    }

    return(crc);
}

//----------------------------------------------------------------------------
// Functions: crc16_resume_compute ()
//
// Description: Based on previous CRC resume computation of new one
//
// Args
// prev_crc: previous crc
// bytes: pointer to buffer
// len: length of buffer
//
// Return
// 16-bit CRC value
//----------------------------------------------------------------------------
uint16_t crc16_resume_compute(uint16_t prev_crc, uint8_t *bytes, uint32_t len)
{
    uint16_t crc = prev_crc;
    uint32_t i;
    uint8_t  index;

    for(i = 0; i < len; i++)
    {
        index = (uint8_t)(crc ^ bytes[i]);
        crc   = (uint16_t)((crc >> 8) ^ table[index]);
    }

    return(crc);
}

//----------------------------------------------------------------------------
// Functions: crc16_file ()
//
// Description: Compute the 16bit CRC of a file
//
// Args
// filename
// returned_crc
// returned_length
//
// Return
// ERR_PASS if the file could be openned, or an appropriate error message
//----------------------------------------------------------------------------
err_t    crc16_file(char *filename, uint16_t *returned_crc, uint32_t *returned_length)
{
    uint16_t crc = 0;
    uint32_t length = 0;
    uint32_t read_bytes = 0;
    uint8_t  payload[256];
    err_t    rc = ERR_PASS;
    FILE     *imgFd;

    imgFd = fopen(filename, "r");
    if(imgFd == 0)
    {
        rc = ERR_FAIL;
    }
    else
    {
        do
	{
            read_bytes = fread(payload, sizeof(payload[0]), sizeof(payload), imgFd);
            crc        = crc16_resume_compute(crc, payload, read_bytes);
            length    += read_bytes;
            if(ferror(imgFd))
	    {
                rc = ERR_FAIL;
	    }
          
        } while ((read_bytes > 0) && (rc == ERR_PASS));
    }

    if (rc == ERR_PASS)
    {
        *returned_crc    = crc;
        *returned_length = length;
    }

    return (rc);
}

#endif // CRC_16_ENABLE

#ifdef CRC_8_ENABLE

static const uint16_t poly8 = 0xd5; // x8 + x7 + x6 + x4 + x2 + 1
static uint8_t  table8[256];

//----------------------------------------------------------------------------
// Functions: crc8_init ()
// Description:
// Initialize the CRC table
// Reference: http://sanity-free.org/146/crc8_implementation_in_csharp.html
//----------------------------------------------------------------------------
void crc8_init(void)
{
    uint16_t temp, i;
    uint8_t j;

    for(i=0; i<sizeof(table8)/sizeof(uint8_t); i++)
    {
        temp = i;
        for(j=0; j<8; j++)
        {
            if((temp & 0x80) != 0)
            {
                temp = ( temp << 1 ) ^ poly8;
            }
            else
            {
                temp <<= 1;
            }
        }
        table8[i] = (uint8_t)temp;
    }
}

//----------------------------------------------------------------------------
// Functions: crc8_compute ()
//
// Description:
//
// Args
// bytes: pointer to buffer
// len: length of buffer
//
// Return
// 8-bit CRC value
//----------------------------------------------------------------------------
uint16_t crc8_compute(uint8_t *bytes, uint32_t len)
{
    uint8_t crc = 0;
    uint32_t i;

    for(i=0; i<len; i++)
    {
        crc = table8[crc ^ bytes[i]];
    }
    return(crc);
}

//----------------------------------------------------------------------------
// Functions: crc8_resume_compute ()
//
// Description: Based on previous CRC resume computation of new one
//
// Args
// prev_crc: previous crc
// bytes: pointer to buffer
// len: length of buffer
//
// Return
// 8-bit CRC value
//----------------------------------------------------------------------------
uint16_t crc8_resume_compute(uint8_t crc, uint8_t *bytes, uint32_t len)
{
    uint32_t i;

    for(i=0; i<len; i++)
    {
        crc = table8[crc ^ bytes[i]];
    }
    return(crc);
}
#endif // CRC_8_ENABLE

