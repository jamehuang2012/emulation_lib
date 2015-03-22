/**************************************************************************

	Disconnect.c

	RS-232C 8bit Protocol Card Reader
	Disconnect

***************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "Disconnect.h"
#include "Prtcl_RS232C_8.h"



void Disconnect(
	unsigned long	Port,
	unsigned long	Baudrate)
{
	long	Ret;
Baudrate = 1;
	// Disconnect
	Ret = RS232C_8_DisconnectDevice( Port );

#ifdef PRINT_MSGS
	if( Ret == _RS232C_8_NO_ERROR )
	{
		printf( "Disconnect O.K.\n" );
	}
	else
	{
		printf( "Disconnect Error! \nError Code %X Returned\n", (unsigned int)Ret );
	}
#endif
	return;
}


