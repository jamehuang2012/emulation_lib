/**************************************************************************

	Connect.c

	RS-232C 8bit Protocol Card Reader
	ConnectDevice

***************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "Connect.h"
#include "Prtcl_RS232C_8.h"

int Connect(
	unsigned long	Port,
	unsigned long	Baudrate)
{
	long	Ret;

	// Connect
	Ret = RS232C_8_ConnectDevice( Port, Baudrate );

	if( Ret == _RS232C_8_NO_ERROR )
	{
#ifdef PRINT_MSGS
		printf( "Connect O.K.\n" );
#endif
		return 0;
	}
	else
	{
#ifdef PRINT_MSGS
		printf( "Connect Error! \nError Code %X Returned\n", (unsigned int)Ret );
#endif
	}

	return -1;
}


