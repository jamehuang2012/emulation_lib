/**************************************************************************

	RS232C_Interface.c

	RS-232C Card Reader Interface Library

***************************************************************************/

#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "logger.h"
#include "RS232C_Interface.h"


#define	_RS232C_INTERFACE_LIBRARY_FILENAME	"RS232C_Interface"
#define	_RS232C_INTERFACE_LIBRARY_REVISION	"3908-01A"


static char BaseDevName[64];

// Define Function
int RS232C_SetBaseDevName( char *pBaseDevName)
{
    strcpy(BaseDevName, pBaseDevName);
    return 0;
}

int RS232C_PortOpen(
	unsigned long Port,
	unsigned long	Baudrate,
	int		Bit)
{
	char		PortName[24];
//	char		TempNumber[8];
	int		Handle;
	struct termios	tio;
	unsigned long	BaudrateTemp;


	bzero( PortName, sizeof( PortName ) );
//	bzero( TempNumber, sizeof( TempNumber ) );

	// Create Port Name
//	strcpy( PortName, PORT_NAME ); // dev/ttyS
strcpy( PortName, BaseDevName); // "/dev/ttyUSB");  BBB: This is now the full /dev/node name
//	sprintf( TempNumber, "%d", (unsigned int)(Port - 1) );
//	strcat( PortName, TempNumber );

	// Port Open
	if( ( Handle = open( PortName, O_RDWR | O_NDELAY | O_NOCTTY ) ) < 0 )
	{
	    printf("Failed to open port %s\n", PortName);
		return	-1;
	}

	if( tcgetattr( Handle, &tio ) < 0 )
	{
	    printf("Failed to get attr for %s\n", PortName);
		close( Handle );
		return	-1;
	}

	tio.c_iflag = 0;
	tio.c_oflag = 0;

	// 7bit or 8bit
	if( 8 == Bit )
	{
		tio.c_cflag = CS8 | CREAD | PARENB | CLOCAL;
	}
	else if( 7 == Bit )
	{
		tio.c_cflag = CS7 | CREAD | PARENB | CLOCAL;
	}

	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;

	switch( Baudrate )
	{
		case 115200 :
			BaudrateTemp = B115200;
			break;

		case 38400 :
			BaudrateTemp = B38400;
			break;

		case 19200 :
			BaudrateTemp = B19200;
			break;

		case 9600 :
			BaudrateTemp = B9600;
			break;

		default :
			BaudrateTemp = B38400;
			break;
	}

	cfsetispeed( &tio, BaudrateTemp );

	if( tcsetattr( Handle, TCSANOW, &tio ) < 0 )
	{
#ifdef PRINT_MSGS
	    printf("Failed to setattr for %s\n", PortName);
#endif
		close( Handle );
		return	-1;
	}

#ifndef SCU_BUILD
	int status;

	if(ioctl(Handle, TIOCMGET, &status) == -1)
	{
	    perror("setRTS: TIOCMGET ");
	}
	else
	{
	    status |= TIOCM_CTS;
	    status |= TIOCM_RTS;
	    if(ioctl(Handle, TIOCMSET, &status) == -1)
	    {
		perror("setRTS: TIOCMSET ");
	    }

	    if(ioctl(Handle, TIOCMGET, &status) == -1)
	    {
            perror("setRTS2: TIOCMGET");
	    }
#ifdef PRINT_MSGS
	    else
	    {
            if(status & TIOCM_CTS)
            {
                printf( "CTS is set\n");
            }

            if(status & TIOCM_RTS)
            {
                printf( "RTS is set\n");
            }
	    }
#endif
	}
#endif /* SCU_BUILD */

	RS232C_PortFlush( Handle );

	return	Handle;
}



void RS232C_PortClose(
	int	Handle)
{
	// Port Close
	close( Handle );
}



void RS232C_PortFlush(
	int	Handle)
{
	fd_set		readfds;
	int		nfds;
	struct timeval	tv;
	unsigned char	Value;


	FD_ZERO( &readfds );
	FD_SET( Handle, &readfds );

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	for( ;; )
	{
		nfds = select( Handle + 1, &readfds, NULL, NULL, &tv );
		if( nfds == 0 )
		{
			return;
		}
		else
		{
			if( FD_ISSET( Handle, &readfds ) )
			{
				if( read( Handle, &Value, 1 ) < 0 )
				{
					fprintf( stderr, "tty read fail\n" );
					return;
				}
			}
		}
	}
}



long RS232C_SendData(
	int		Handle,
	char		*pSendData,
	unsigned long	SizeOfSendData)
{
	ALOGHEX("SND",pSendData,SizeOfSendData);
	if( write( Handle, pSendData, SizeOfSendData ) < 0 )
	{
		return	_RS232C_ERROR_DATA_SEND;
	}

	return	_RS232C_NO_ERROR;
}



long RS232C_ReceiveData(
	int		Handle,
	char		*pReceiveData,
	unsigned long	SizeOfReceiveBuffer,
	unsigned long	*pSizeOfDataReceived,
	int		Timeout)
{
	fd_set			readfds;
	int			nfds;
	struct timeval		tv;
	char			*pDataBuffer;
	int			Counter;
//	struct timeval		ttv;
//	struct timezone		ttz;


	*pSizeOfDataReceived = 0;
	pDataBuffer = pReceiveData;

	FD_ZERO( &readfds );
	FD_SET( Handle, &readfds );

	if( Timeout >= 1000 )
	{

		// Sec
		tv.tv_sec = Timeout / 1000;
		tv.tv_usec = 0;
	}
	else
	{
		// Micro
		tv.tv_sec = 0;
		tv.tv_usec = Timeout * 1000;
	}

	// Check Timeout
	if( Timeout )
	{
		// Set Timeout Value
		nfds = select( Handle + 1, &readfds, NULL, NULL, &tv );
	}

	else
	{
		// Set Infinite
		nfds = select( Handle + 1, &readfds, NULL, NULL, NULL );
	}

	if( nfds == 0 )
	{
		return	_RS232C_ERROR_TIMEOUT;
	}

	// Set Receiving Data Check Time
	tv.tv_sec = 0;
	tv.tv_usec = _RS232C_DEFAULT_DATA_RECEIVE_TIME;

	for( Counter = 0; Counter < (int)SizeOfReceiveBuffer; Counter++ )
	{
		nfds = select( Handle + 1, &readfds, NULL, NULL, &tv );

		if( nfds == 0 )
		{

			return	_RS232C_ERROR_CHARACTER_TIMEOUT;
		}
		else
		{

			if( FD_ISSET( Handle, &readfds ) )
			{

				if( read( Handle, pDataBuffer, 1 ) < 0 )
				{
					return	_RS232C_ERROR_DATA_RECEIVE;
				}

				( *pSizeOfDataReceived )++;
				pDataBuffer++;
			}
		}
	}

	ALOGHEX("RSV",pDataBuffer-(*pSizeOfDataReceived),(*pSizeOfDataReceived));
	return	_RS232C_NO_ERROR;
}



void RS232C_Interface_GetLibraryRevision(
	PLIB_INFORMATION_RS232C_INTERFACE	pLibInfo)
{
	// Clear Buffer
	bzero( pLibInfo, sizeof( LIB_INFORMATION_RS232C_INTERFACE ) );

	// Set Filename
	memcpy(pLibInfo->Filename,
            _RS232C_INTERFACE_LIBRARY_FILENAME,
            strlen( _RS232C_INTERFACE_LIBRARY_FILENAME ));

	// Set Revison
	memcpy(pLibInfo->Revision,
            _RS232C_INTERFACE_LIBRARY_REVISION,
            strlen( _RS232C_INTERFACE_LIBRARY_REVISION ));

	return;
}


