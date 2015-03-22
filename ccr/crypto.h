

#ifndef __CRYPTO_H_
#define __CRYPTO_H_

int generateDESKey(unsigned char *pDESKey);
int generateRandomData(unsigned char *pData, int size);

int validate2DESKey(unsigned char *pDESKey);

int encryptDESKey(unsigned char *pDESClearTextKey, unsigned char *pDESEncryptedKey, unsigned char *pDESEncryptionKey);
int encryptData(unsigned char *in, unsigned char *out, unsigned char *key, int dataLength);
int decryptDESKey(unsigned char *pDESEncryptedKey, unsigned char *pDESDecryptedKey, unsigned char *pDESEncryptionKey);
int decryptTrackData(unsigned char *pMagDataEncryptionKey,
                     unsigned char *pMagDataIV,
                     unsigned char *pEncryptedTrackData,
                     unsigned char *pClearTextTrackData,
                     long dataLength);
#endif
