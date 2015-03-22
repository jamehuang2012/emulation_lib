/**************************************************************************

	APITest.h

	RS-232C 8bit Protocol Card Reader
	API Test Application Header File

***************************************************************************/

#ifndef	_CARD_READER_MANAGER_H_
#define	_CARD_READER_MANAGER_H_

#include "RS232C_UserInterface_Proc.h"
#include "Prtcl_RS232C_8.h"

#define SUCCESS 0
#ifndef bool
    #define bool unsigned char
    #define true 1
    #define false 0
#endif

extern int ClientAsyncSocket;
extern unsigned long Port;
extern unsigned long Baudrate;
extern bool bAcquireCardState;
extern bool bShutdown;
bool bThreadExiting;

void ReleaseResources(void);

#endif	// _CARD_READER_MANAGER_H_

