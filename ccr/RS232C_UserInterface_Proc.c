/**************************************************************************

	RS232C_UserInterface_Proc.c

	RS232C User Interface Processing Library

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "RS232C_UserInterface_Proc.h"



unsigned long Select_COM_Port( void )
{
	unsigned long	Port;
	char		Input[8], Temp;
	char		*pInput;
	int		Counter;


	// Input Port
	printf( "\n" );
	printf( "Select COM Port No ==> " );

	Counter = 0;
	bzero( Input, sizeof( Input ) );
	pInput = Input;
	Temp = getchar();
	while( '\n' != Temp ) {

		*pInput++ = Temp;
		Temp = getchar();
		Counter++;
	}

	// Transform Character to Number
	Port = atol( Input );

	printf( "\n" );

	return	Port;
}


unsigned long Select_Baudrate( void )
{
	char		Input, CR;
	unsigned long	Baudrate = 38400;


	// Input baudrate
	printf( "1 : 38400\n" );
	printf( "2 : 19200\n" );
	printf( "3 : 9600\n" );
	printf( "4 : 115200\n" );
	printf( "Select Baudrate ==> " );

	for( ;; ) {

		Input = getchar();

		if( Input != '\n' ) {

			if( ( Input < '1' ) || ( Input > '4' ) ) {
				printf( "\nSelect \"CORRECT\" Baudrate No ==> " );
			}
			else {
				CR = getchar();
				break;
			}
		}
	}

	switch( Input ) {

		case '1' :
			Baudrate = 38400;
			break;

		case '2' :
			Baudrate = 19200;
			break;

		case '3' :
			Baudrate = 9600;
			break;

		case '4' :
			Baudrate = 115200;
			break;
	}

	printf( "\n" );

	return	Baudrate;
}


void DisplayData(
	char		Type,
	unsigned char	*Data,
	unsigned long	DataSize
)
{
	int		Counter;
	unsigned char	*pIndex;


	pIndex = Data;

	for( Counter = 1; Counter <= (int)DataSize; Counter++ ) {

		switch( Type ) {

			case _HEX :

				printf( "%.2X ", *pIndex++ & 0xFF );
				break;

			case _ASCII :

				if( isprint( ( int )*pIndex ) ) {

					if( 0x00 != *pIndex ) {
						printf( " %C ", *pIndex & 0xFF );
					}
					else {
						printf( "   " );
					}
				}

				else {
					printf( "-- " );
				}

				pIndex++;

				break;
		}

		if( 0 == ( Counter % 16 ) ) {
			printf( "\n" );
		}

		else if( ( 0 == ( Counter % 8 ) ) && ( Counter != (int)DataSize ) ) {
			printf( "- " );
		}
	}

	return;
}


int Select_Yes_or_No( void )
{
	char	Input, CR;
	int	Ret = 0;


	printf( "Are you ready?\n" );

	// Input Port
	printf( "1 : Yes\n" );
	printf( "2 : No\n" );
	printf( "Select No ==> " );

	for( ;; ) {

		Input = getchar();

		if( Input != '\n' ) {

			if( ( Input < '1' ) || ( Input > '2' ) ) {
				printf( "\nSelect \"CORRECT\" No ==> " );
			}
			else {
				CR = getchar();
				break;
			}
		}
	}

	switch( Input ) {

		case '1' :
			Ret = 1;
			break;

		case '2' :
			Ret = 0;
			break;
 	}

	printf( "\n" );

	return	Ret;
}


void ShowReplyType( REPLY_TYPE replyType )
{
	switch( replyType ) {

		case	PositiveReply :
			printf( "ReplyType : PositiveReply\n" );
			break;

		case	NegativeReply :
			printf( "ReplyType : NegativeReply\n" );
			break;

		case	Unavailable :
			printf( "ReplyType : Unavailable\n" );
			break;

		default:
			printf( "ReplyType : Invalid\n" );
			break;
	}

	return;
}


void MakeBinaryData(
	unsigned char	*OriginalData,
	unsigned long	OriginalDataSize,
	unsigned char	*ConvertedData,
	unsigned long	*ConertedDataSize
)
{
	int		Counter;
	unsigned char	Temp1, Temp2 = 0, Temp3 = 0;
	char		Flag = 0;


	*ConertedDataSize = 0;

	for( Counter = 0; Counter < (int)OriginalDataSize; Counter++ ) {

		Temp1 = *( OriginalData + Counter );

		if( ( Temp1 >= '0' ) && ( Temp1 <= '9' ) ) {
			Temp2 = Temp1 - '0';
		}

		else if( ( Temp1 >= 'A' ) && ( Temp1 <= 'F' ) ) {
			Temp2 = Temp1 - 'A' + 10;
		}

		else if( ( Temp1 >= 'a' ) && ( Temp1 <= 'f' ) ) {
			Temp2 = Temp1 - 'a' + 10;
		}

		if( Flag ) {

			Temp3 += Temp2;
			*( ConvertedData + *ConertedDataSize ) = Temp3;
			(*ConertedDataSize)++;
		}
		else {

			Temp3 = ( Temp2 << 4 );
		}

		Flag = Flag ? 0 : 1;
	}

	return;
}



