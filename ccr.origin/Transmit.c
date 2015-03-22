/**************************************************************************

	Transmit.c

	RS-232C 8bit Protocol Card Reader
	Transmit

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

#include "Transmit.h"
#include "CardReaderManager.h"


void Transmit(
	unsigned long	Port,
	unsigned long	Baudrate,
	unsigned char	Flag)
{
	long		Ret;
	REPLY		Reply_Val;
//	int		Continue;
//	char		Input, CR;
Baudrate = 1;
//	unsigned char	TempDataSize[8];
//	unsigned char	TempDataSizeVal;
//	unsigned char	*pDataSizeIndex = TempDataSize;

	unsigned char	TempSendData[65535];
	unsigned char	SendData[65535];
	unsigned char	SendDataVal;
	unsigned char	*pTempSendData = TempSendData;
	unsigned char	*pSendDataIndex = SendData;
	int		Counter;

	unsigned long	DataSizeToSend;
//	unsigned char 	*pDataToSend;
	unsigned long	SizeOfDataReceived;
	unsigned char	DataReceived[65535];
	unsigned long 	AdditionalErrorCode;

	FILE		*DataFilePtr;
	char		*StringCheck;
	int		TempChar;


	// Buffer Clear
	bzero( TempSendData, sizeof( TempSendData ) );
	bzero( SendData, sizeof( SendData ) );
	bzero( &Reply_Val, sizeof( REPLY ) );


	// Data

	printf( "Input Data ==> " );

	Counter = 0;
	SendDataVal = getchar();
	while( '\n' != SendDataVal )
	{
		*pTempSendData++ = SendDataVal;
		SendDataVal = getchar();
		Counter++;
	}


	// Check File or Data
	StringCheck = strstr( ( char *)TempSendData, ".txt" );

	// Not From File
	if( NULL == StringCheck )
	{
		// Set Data
		MakeBinaryData(
			TempSendData,
			Counter,
			pSendDataIndex,
			&DataSizeToSend
			);
	}
	// From File
	else
	{

		DataFilePtr = fopen( ( char *)TempSendData, "r" );
		if( NULL == DataFilePtr )
		{
			printf( "\nFile Open Error!!\n" );
			return;
		}

		// Clear
		bzero( TempSendData, sizeof( TempSendData ) );
		bzero( SendData, sizeof( SendData ) );
		pTempSendData = TempSendData;
		pSendDataIndex = SendData;
		Counter = 0;

		// Data Read From File

		for( ;; )
		{
			TempChar = fgetc( DataFilePtr );

			if( EOF == TempChar )
			{
				break;
			}

			*pTempSendData++ = ( unsigned char )TempChar;
			Counter++;
		}

		// File Close
		fclose( DataFilePtr );

		// Set Data Length
		MakeBinaryData(
			TempSendData,
			Counter,
			pSendDataIndex,
			&DataSizeToSend
			);
	}


	// IC Card
	if( ICCARD == Flag )
	{
		Ret = RS232C_8_ICCardTransmit(
					Port,
					DataSizeToSend,
					SendData,
					65535,
					&SizeOfDataReceived,
					DataReceived,
					&AdditionalErrorCode,
					&Reply_Val
					);

		printf( "\nICCardTransmit Returned %X\n", (unsigned int)Ret );
	}
	// SAM
	else
	{

		Ret = RS232C_8_SAMTransmit(
					Port,
					DataSizeToSend,
					SendData,
					65535,
					&SizeOfDataReceived,
					DataReceived,
					&AdditionalErrorCode,
					&Reply_Val
					);

		printf( "\nSAMTransmit Returned %X\n", (unsigned int)Ret );
	}

	if( _RS232C_8_NO_ERROR == Ret )
	{
		printf( "\nHEX :\n" );
		DisplayData( _HEX, DataReceived, SizeOfDataReceived );

		printf( "\n\nASCII :\n" );
		DisplayData( _ASCII, DataReceived, SizeOfDataReceived );
	}

	else if( _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR == Ret )
	{
		printf( "Command Code   : %X\n", Reply_Val.message.negativeReply.CommandCode );
		printf( "Parameter Code : %X\n", Reply_Val.message.negativeReply.ParameterCode );
		printf( "ErrorCode1     : %X\n", Reply_Val.message.negativeReply.ErrorCode.E1 );
		printf( "ErrorCode0     : %X\n", Reply_Val.message.negativeReply.ErrorCode.E0 );

		if( 0 != Reply_Val.message.negativeReply.Data.Size )
		{
			printf( "\nHEX :\n" );
			DisplayData(
				_HEX,
				Reply_Val.message.negativeReply.Data.Body,
				Reply_Val.message.negativeReply.Data.Size
				);

			printf( "\n\nASCII :\n" );
			DisplayData(
				_ASCII,
				Reply_Val.message.negativeReply.Data.Body,
				Reply_Val.message.negativeReply.Data.Size
				);
		}
		else
		{
			printf( "\nNo Data\n" );
		}
	}
	else
	{
		if( ICCARD == Flag )
		{
			printf( "\nICCardTransmit Error\nError Code = %x\n", (unsigned int)Ret );
		}
		else
		{
			printf( "\nSAMTransmit Error\nError Code = %x\n", (unsigned int)Ret );
		}

		printf( "AdditionalErrorCode = %x\n", (unsigned int)AdditionalErrorCode );
	}

	printf( "\n" );

	return;
}


