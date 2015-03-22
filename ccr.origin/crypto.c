/*
 * Copyright (C) 2011, 2012 MapleLeaf Software, Inc
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
    crypto.c - Cryptographic related functions.
*/

#include <stdlib.h>
#include <string.h>

#include <openssl/rand.h>
#include <openssl/des.h>

#include "crypto.h"

// Default Master Key Km
// For development firmware

unsigned char defaultKm[16] = {0xCB, 0x9B, 0x6B, 0xCE, 0x86, 0xEA, 0x8F, 0xD3,
                               0xA2, 0xD0, 0xD0, 0x15, 0x85, 0x91, 0xF1, 0x2C};

unsigned char IV[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

int generateRandomData(unsigned char *pData, int size)
{
    unsigned char seedVal[8] = {0x02,0x04,0x06,0x12,0xEF,0xDC};

    RAND_seed(seedVal, 8);
    RAND_bytes(pData, size);

    return 0;
}

int generateDESKey(unsigned char *pDESKey)
{
    // Need to generate a 16 byte DES key to be used with the card reader
    unsigned char seedVal[8] = {0x02,0x04,0x06,0x12,0xEF,0xDC};
    int nRetVal;

    DES_cblock Key1Val;
    DES_cblock Key2Val;

    RAND_seed(seedVal, 8);
//    RAND_bytes(pDESKey, 16);
    nRetVal = DES_random_key(&Key1Val);
    if(!nRetVal)
    {
        return -1;
    }
    nRetVal = DES_random_key(&Key2Val);
    if(!nRetVal)
    {
        return -1;
    }

    memcpy(pDESKey, Key1Val, 8);
    memcpy(pDESKey+8, Key2Val, 8);

    return 0;
}

int validate2DESKey(unsigned char *pDESKey)
{
    int nRetVal ;

    DES_cblock Key1Val;
    DES_cblock Key2Val;
    DES_key_schedule Key1;
    DES_key_schedule Key2;

    memcpy(Key1Val, pDESKey, 8);
    memcpy(Key2Val, pDESKey+8, 8);

    DES_set_odd_parity(&Key1Val);
    DES_set_odd_parity(&Key2Val);

    memcpy(pDESKey, Key1Val, 8);
    memcpy(pDESKey+8, Key2Val, 8);

    nRetVal = DES_set_key_checked(&Key1Val, &Key1);
    if(nRetVal)
    {
        printf("Bad DES Key1\n");
        return -1;
    }

    nRetVal = DES_set_key_checked(&Key2Val, &Key2);
    if(nRetVal)
    {
        printf("Bad DES Key2\n");
        return -1;
    }

    return 0;
}

int encryptData(unsigned char *inBuff, unsigned char *encryptedData, unsigned char *key, int dataLength)
{
    const_DES_cblock inBuf;
    DES_cblock outBuf;

    const_DES_cblock Key1Val;
    const_DES_cblock Key2Val;
    DES_key_schedule Key1;
    DES_key_schedule Key2;

    int blockCount = dataLength / 8;
    int i;

//    memcpy(inBuf, inBuff, 8); // First 8 bytes to encrypt


    memcpy(Key1Val, key, 8);
    memcpy(Key2Val, key+8, 8);

    DES_set_key_unchecked(&Key1Val, &Key1);
    DES_set_key_unchecked(&Key2Val, &Key2);


    // Using the Encryption Key encrypt the clear text key and place encrypted key into the encrypted Key buffer.
    for( i = 0; i < blockCount; i++)
    {
        memcpy(inBuf, inBuff+(i*8), 8); // 8 bytes to encrypt
        DES_ecb2_encrypt(&inBuf, &outBuf, &Key1, &Key2, DES_ENCRYPT);
        memcpy(encryptedData+(i*8), outBuf, 8);

    }

    return 0;
}

// Use ECB to encrypt a key to send to the card reader
int encryptDESKey(unsigned char *pDESClearTextKey,
                  unsigned char *pDESEncryptedKey,
                  unsigned char *pDESEncryptionKey)
{
    const_DES_cblock inBuf;
    DES_cblock outBuf;

    const_DES_cblock Key1Val;
    const_DES_cblock Key2Val;
    DES_key_schedule Key1;
    DES_key_schedule Key2;

    memcpy(inBuf, pDESClearTextKey, 8); // First 8 bytes to encrypt


    memcpy(Key1Val, pDESEncryptionKey, 8);
    memcpy(Key2Val, pDESEncryptionKey+8, 8);

//int DES_set_key_checked(const_DES_cblock *key,DES_key_schedule *schedule);
DES_set_key_unchecked(&Key1Val, &Key1);
DES_set_key_unchecked(&Key2Val, &Key2);


    // Using the Encryption Key encrypt the clear text key and place encrypted key into the encrypted Key buffer.
//    DES_ecb_encrypt(&dcb, &dcb_output, &schedule, enc);
    DES_ecb2_encrypt(&inBuf, &outBuf, &Key1, &Key2, DES_ENCRYPT);
    memcpy(pDESEncryptedKey, outBuf, 8);  // First part of Key to be encrypted.

    memcpy(inBuf, pDESClearTextKey+8, 8); // Second 8 bytes to encrypt

    DES_ecb2_encrypt(&inBuf, &outBuf, &Key1, &Key2, DES_ENCRYPT);
    memcpy(pDESEncryptedKey+8, outBuf, 8);  // Second part of Key to be encrypted.

    return 0;
}

// Use ECB to encrypt a key to send to the card reader
int decryptDESKey(unsigned char *pDESEncryptedKey,
                  unsigned char *pDESDecryptedKey,
                  unsigned char *pDESEncryptionKey) // 16 byte key, 1st 8 are Key1/3, last 8 Key2
{
    int nRetVal = 0;

    const_DES_cblock inBuf;
    DES_cblock outBuf;

    const_DES_cblock Key1Val;
    const_DES_cblock Key2Val;
    DES_key_schedule Key1;
    DES_key_schedule Key2;

    memcpy(inBuf, pDESEncryptedKey, 8); // First 8 bytes to encrypt


    memcpy(Key1Val, pDESEncryptionKey, 8);
    memcpy(Key2Val, pDESEncryptionKey+8, 8);

    //int DES_set_key_checked(const_DES_cblock *key,DES_key_schedule *schedule);
    nRetVal = DES_set_key_checked(&Key1Val, &Key1);
    if(nRetVal)
    {

    }
    nRetVal = DES_set_key_checked(&Key2Val, &Key2);
    if(nRetVal)
    {

    }

    DES_ecb2_encrypt(&inBuf, &outBuf, &Key1, &Key2, DES_DECRYPT);
    memcpy(pDESDecryptedKey, outBuf, 8);

    memcpy(inBuf, pDESEncryptedKey+8, 8); // First 8 bytes to encrypt

    DES_ecb2_encrypt(&inBuf, &outBuf, &Key1, &Key2, DES_DECRYPT);
    memcpy(pDESDecryptedKey+8, outBuf, 8);

    return nRetVal;
}

// Use CBC to decrypt the track data
int decryptTrackData(unsigned char *pMagDataEncryptionKey,
                     unsigned char *pMagDataIV,
                     unsigned char *pEncryptedTrackData,
                     unsigned char *pClearTextTrackData,
                     long dataLength)
{
    DES_key_schedule Key1;
    DES_key_schedule Key2;
    const_DES_cblock Key1Val;
    const_DES_cblock Key2Val;
    DES_cblock ivec;
    int nRetVal = 0;
//    int numberOfBlocks = dataLength / 8;
//    int i;

    memset(ivec, 0, sizeof(ivec));
    memcpy(ivec, pMagDataIV, 8);

    memcpy(Key1Val, pMagDataEncryptionKey, 8);
    memcpy(Key2Val, pMagDataEncryptionKey+8, 8);

    //int DES_set_key_checked(const_DES_cblock *key,DES_key_schedule *schedule);
    nRetVal = DES_set_key_checked(&Key1Val, &Key1);
    if(nRetVal)
    {

    }

    nRetVal = DES_set_key_checked(&Key2Val, &Key2);
    if(nRetVal)
    {

    }

    // Using the encryption Key use 3DES CBC to "decrypt" the encrypted track data and place the clear text track data
    // in the clear text track data buffer.
//    for( i = 0; i < numberOfBlocks; i++ )
    {

        DES_ede2_cbc_encrypt(pEncryptedTrackData,
                             pClearTextTrackData,
                             dataLength,
                             &Key1,
                             &Key2,
                             &ivec,
                             DES_DECRYPT);
    }

    return 0;
}
