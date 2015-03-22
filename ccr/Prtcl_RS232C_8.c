/**************************************************************************

	Prtcl_RS232C_8.c

	RS-232C 8bit Protocol Card Reader Library

***************************************************************************/

#include <stdio.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "Prtcl_RS232C_8.h"
#include "Prtcl_RS232C_8_Def.h"
#include "Prtcl_RS232C_8_FuncDef.h"
#include "RS232C_Interface.h"
#include "CRC_Proc.h"


#define	RS232C_8_LIBRARY_FILENAME	"Prtcl_RS232C_8"

pthread_mutex_t cancel_mutex = PTHREAD_MUTEX_INITIALIZER;
extern int command_step ; /* read card step status*/

// Device Information Buffer
DEVICE_ACCESS_INFORMATION		g_Information[MAX_PORT_NO];

int RS232C_8_IsConnected(unsigned long Port)
{
    return (g_Information[Port].ConnectFlag != 0);
}

long RS232C_8_ConnectDevice(
	unsigned long	Port,
	unsigned long	Baudrate)
{
	int	Ret;
	long	Returned;
	int	Handle;


	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Baudrate Check
	if( ( 115200 != Baudrate ) && ( 38400 != Baudrate ) &&
	    ( 19200 != Baudrate ) && ( 9600 != Baudrate ) )
    {
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Init Proc
	Returned = RS232C_Init();
	if( _RS232C_8_NO_ERROR != Returned )
	{
		MessageToLog(Port,
                    "ConnectDevice Return ( RS232C_Init ) ",
                    PROC_EXIST_DATA,
                    Returned);
		return	Returned;
	}

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_Connect );
	if( Ret )
	{
		MessageToLog(Port,
                    "ConnectDevice Return ( sem_Connect in ) ",
                    PROC_EXIST_DATA,
                    _RS232C_8_CRITICAL_SECTION_ERROR);
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		MessageToLog(Port,
			"ConnectDevice Return ( sem_CommandExecute in ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		sem_post( &g_Information[Port].sem_Connect );
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	// Connect Check
	if( g_Information[Port].ConnectFlag )
	{
		MessageToLog(Port,
			"ConnectDevice Return ",
			PROC_EXIST_DATA,
			_RS232C_8_DEVICE_ALREADY_CONNECTED_ERROR);
		sem_post( &g_Information[Port].sem_CommandExecute );
		sem_post( &g_Information[Port].sem_Connect );
		return	_RS232C_8_DEVICE_ALREADY_CONNECTED_ERROR;
	}


	// Port Open
	MessageToLog( Port, "ConnectDevice", PROC_NO_DATA, 0 );
	if( ( Handle = RS232C_PortOpen( Port, Baudrate, 8 ) ) < 0 )
	{
		MessageToLog( Port, "ConnectDevice errno", PROC_EXIST_DATA, errno );
		MessageToLog(Port,
			"ConnectDevice Return ",
			PROC_EXIST_DATA,
			_RS232C_8_CANNOT_OPEN_PORT_ERROR);
		sem_post( &g_Information[Port].sem_CommandExecute );
		sem_post( &g_Information[Port].sem_Connect );
		return	_RS232C_8_CANNOT_OPEN_PORT_ERROR;
	}

	// Set Port Handle
	g_Information[Port].Handle = Handle;

	// Connect ON
	g_Information[Port].ConnectFlag = TRUE;

	// Cancel Flag ON
	g_Information[Port].CancelFlag = FALSE;

	// Cancel Received Flag ON
	g_Information[Port].CancelReceiveFlag = TRUE;

	// Set Flag
	g_Information[Port].LongData = FALSE;

	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		MessageToLog(Port,
			"ConnectDevice Return ( sem_CommandExecute out ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		RS232C_PortClose( g_Information[Port].Handle );
		g_Information[Port].ConnectFlag = FALSE;
		sem_post( &g_Information[Port].sem_Connect );
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_Connect );
	if( Ret )
	{
		MessageToLog(Port,
			"ConnectDevice Return ( sem_Connect out ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		RS232C_PortClose( g_Information[Port].Handle );
		g_Information[Port].ConnectFlag = FALSE;
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	MessageToLog(Port,
		"ConnectDevice Return ",
		PROC_EXIST_DATA,
		_RS232C_8_NO_ERROR);

	return	_RS232C_8_NO_ERROR;
}



long RS232C_8_DisconnectDevice(
	unsigned long	Port)
{
	long	Returned = _RS232C_8_NO_ERROR;
	int	Ret;


	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Connect Check
	if( !g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR;
	}

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_Connect );
	if( Ret )
	{
		MessageToLog(Port,
			"DisconnectDevice Return ( sem_Connect in ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		g_Information[Port].ConnectFlag = FALSE;
		Returned = _RS232C_8_CRITICAL_SECTION_ERROR;
		goto _ERR_END;
	}

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		MessageToLog(Port,
			"DisconnectDevice Return ( sem_CommandExecute in ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		sem_post( &g_Information[Port].sem_Connect );
		g_Information[Port].ConnectFlag = FALSE;
		Returned = _RS232C_8_CRITICAL_SECTION_ERROR;
		goto _ERR_END;
	}

	// Port Close
	MessageToLog( Port, "DisconnectDevice", PROC_NO_DATA, 0 );
	RS232C_PortClose( g_Information[Port].Handle );

	// Connect OFF
	g_Information[Port].ConnectFlag = FALSE;

	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		MessageToLog(Port,
			"DisconnectDevice Return ( sem_CommandExecute out ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		sem_post( &g_Information[Port].sem_Connect );
		Returned = _RS232C_8_CRITICAL_SECTION_ERROR;
		goto _ERR_END;
	}

	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_Connect );
	if( Ret )
	{
		MessageToLog(Port,
			"DisconnectDevice Return ( sem_Connect out ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		Returned = _RS232C_8_CRITICAL_SECTION_ERROR;
		goto _ERR_END;
	}


_ERR_END:

	MessageToLog(Port,
		"DisconnectDevice Return ",
		PROC_EXIST_DATA,
		_RS232C_8_NO_ERROR);

	RS232C_End( Port );

	return	Returned;
}



long RS232C_8_ExecuteCommand(
	unsigned long 	Port,
	PCOMMAND	pCommand,
	unsigned long	Timeout,
	PREPLY		pReply)
{
	unsigned char	Command_Data[ MAX_DATA_ARRAY_SIZE ];
	unsigned char	*DataIndex = Command_Data;
	unsigned long	Command_Data_Length;
	unsigned short	CrcCode;

	unsigned char	Control_ACK[] = { ACK };
	unsigned long	Control_ACK_Length = sizeof( Control_ACK );
	unsigned char	Control_NAK[] = { NAK };
	unsigned long	Control_NAK_Length = sizeof( Control_NAK );
	long		Returned, ConvertRet;
	unsigned char	ReplyTempBuffer[ MAX_DATA_ARRAY_SIZE ];
	unsigned char	*ReplyTempBufferIndex;
	unsigned long	ReplyLength;
	unsigned long	ReadLength;
	unsigned char	CRC1, CRC2;
	unsigned long	DataSize = 0;
	unsigned char	*pText;
	unsigned char	*pTextEnd;
	unsigned char	*pIterator;
	int		Counter;
	unsigned char	ACKorNAK;
	int		RetryCounter, RetryCounterforCRC;
	int		Ret;
	int		HeaderLength;


	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Connect Check
	if( !g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR;
	}

	// Set Init Value
	pReply->replyType = Unavailable;

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		MessageToLog(Port,
			"ExecuteCommand Return ( sem_CommandExecute in ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	// Cancel Processing Check
	if( g_Information[Port].CancelFlag )
	{
		ConvertRet = _RS232C_8_CANCEL_COMMAND_SESSION_ERROR;
		goto _END;
	}

	// Set Data
	*DataIndex++ = STX;
	*DataIndex++ = ( unsigned char )( ( pCommand->Data.Size + S_HEADER_LEN ) >> 8 );
	*DataIndex++ = ( unsigned char )( pCommand->Data.Size + S_HEADER_LEN );

	if( COMMAND_TAG_LARGE == GetFirstParameterCommandFormatData() )
	{
		*DataIndex++ = COMMAND_TAG_LARGE;
	}
	else if( COMMAND_TAG_SMALL == GetFirstParameterCommandFormatData() )
	{
		*DataIndex++ = COMMAND_TAG_SMALL;
	}
	else
	{
		MessageToLog(Port,
			"ExecuteCommand Return ( COMMAND_TAG ) ",
			PROC_EXIST_DATA,
			_RS232C_8_PARAMETER_ERROR);
		return	_RS232C_8_PARAMETER_ERROR;
	}

	*DataIndex++ = pCommand->CommandCode;
	*DataIndex++ = pCommand->ParameterCode;
	for( Counter = 0; Counter < (int)pCommand->Data.Size; Counter++ )
	{
		*( DataIndex + Counter ) = *( pCommand->Data.pBody + Counter );
	}
	DataIndex += pCommand->Data.Size;
	CrcCode = CrcCcitt( ( unsigned char *)Command_Data, DataIndex - Command_Data );
	*DataIndex++ = ( unsigned char )( CrcCode >> 8 );
	*DataIndex++ = ( unsigned char )( CrcCode );

	// Set Data Length
	Command_Data_Length = pCommand->Data.Size + 6 + 2;

	RetryCounter = 0;

_RETRY_COMMAND:

	// Check Command Retry End
	if( DEFAULT_COMMAND_RETRY < RetryCounter )
	{
		// ACK timeout or NAK overtimes
		if( ACK == ACKorNAK )
		{
			ConvertRet = _RS232C_8_ACK_TIMEOUT;
		}
		else
		{
			ConvertRet = _RS232C_8_NAK_RECEIVE_OVERTIMES;
		}

		goto _END;
	}

	// Write Log
	{
		MessageToLog( Port, "ExecuteCommand Send", PROC_NO_DATA, 0 );
		DataMessageToLog(Port,
				( char *)( Command_Data + 3 ),
				pCommand->Data.Size,
				S_HEADER_LEN);
	}

	// Send Data
	Returned = RS232C_SendData(g_Information[Port].Handle,
			( char *)Command_Data,
			Command_Data_Length);

	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	{
		MessageToLog( Port, "ExecuteCommand Send errno", PROC_EXIST_DATA, errno );
		goto _END;
	}

_RETRY_ACK_RES:

	// Receive ACK
	bzero( ReplyTempBuffer, sizeof( ReplyTempBuffer ) );
 	Returned = RS232C_ReceiveData(g_Information[Port].Handle,
				( char *)ReplyTempBuffer,
				1,
				&ReplyLength,
				DEFAULT_ACK_TIMEOUT);

	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	{
		MessageToLog( Port, "ExecuteCommand ACK Receive errno", PROC_EXIST_DATA, errno );

	 	if( _RS232C_8_REPLY_TIMEOUT == ( ConvertRet = ErrorExchange( Returned ) ) )
	 	{
			MessageToLog( Port, "ExecuteCommand Receive ACK Timeout", PROC_NO_DATA, 0 );
			ACKorNAK = ACK;
			RetryCounter++;
			goto _RETRY_COMMAND;
		}
		else
		{
			ConvertRet = _RS232C_8_FAILED_TO_RECEIVE_ACK_ERROR;
			goto _END;
		}
	}
	else if( NAK == *ReplyTempBuffer )
	{
		ACKorNAK = NAK;
		RetryCounter++;
		goto _RETRY_COMMAND;
	}
	else if( ACK != *ReplyTempBuffer )
	{
		goto _RETRY_ACK_RES;
	}

	// Cancel Received Flag OFF
	g_Information[Port].CancelReceiveFlag = FALSE;

	RetryCounter = 1;
	RetryCounterforCRC = 1;

_RETRY_REPLY:

	// Check Receive Retry End
	if( DEFAULT_RECEIVE_RETRY < RetryCounter )
	{
		ConvertRet = _RS232C_8_CHARACTER_TIMEOUT;
		goto _END;
	}

	// Receive Reply Data ( Get First 1 Byte )
	bzero( ReplyTempBuffer, sizeof( ReplyTempBuffer ) );
 	Returned = RS232C_ReceiveData(g_Information[Port].Handle,
				( char *)ReplyTempBuffer,
				1,
				&ReplyLength,
				Timeout);

	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	{
		MessageToLog( Port, "ExecuteCommand Get First 1 Byte Data Receive errno", PROC_EXIST_DATA, errno );
		goto _END;
	}

	// DLE+EOT Check
	if( DLE == *( ReplyTempBuffer ) )
	{
		// Receive Reply Data ( Get Second 1 byte )
 		Returned = RS232C_ReceiveData(g_Information[Port].Handle,
					( char *)( ReplyTempBuffer + 1 ),
					1,
					&ReplyLength,
					Timeout);

	 	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	 	{
			MessageToLog( Port, "ExecuteCommand DLE + EOT Receive errno", PROC_EXIST_DATA, errno );
			goto _END;
		}

		// DLE+EOT Check
		if( EOT == *( ReplyTempBuffer + 1 ) )
		{
			MessageToLog(Port,
				"ExecuteCommand Receive DLE + EOT",
				PROC_NO_DATA,
				0);
			ConvertRet = _RS232C_8_COMMAND_CANCELED;
		}
		else
		{

			ConvertRet = _RS232C_8_FAILED_TO_RECEIVE_REPLY_ERROR;
		}

		goto _END;
	}

	// Reply Check
	if( STX != *( ReplyTempBuffer ) )
	{
		goto _RETRY_REPLY;
	}

	// Receive Reply Data ( Get Length )
 	Returned = RS232C_ReceiveData(g_Information[Port].Handle,
				( char *)( ReplyTempBuffer + 1 ),
				2,
				&ReplyLength,
				Timeout);

	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	{
		MessageToLog( Port, "ExecuteCommand Get Length Receive errno", PROC_EXIST_DATA, errno );
		goto _END;
	}

	// Get Data Length to Receive
	ReadLength = *( ReplyTempBuffer + 1 );
	ReadLength <<= 8;
	ReadLength |= *( ReplyTempBuffer + 2 );

	// Receive Reply Data ( Get Data )
 	Returned = RS232C_ReceiveData(g_Information[Port].Handle,
				( char *)( ReplyTempBuffer + 3 ),
				ReadLength + 2,
				&ReplyLength,
				Timeout);

	if( ( _RS232C_8_REPLY_TIMEOUT == ( ConvertRet = ErrorExchange( Returned ) ) ) ||
 		( _RS232C_8_CHARACTER_TIMEOUT == ( ConvertRet = ErrorExchange( Returned ) ) ) )
    {
	 	// Send NAK
	 	RS232C_SendData(g_Information[Port].Handle,
			( char *)Control_NAK,
			Control_NAK_Length);

		RetryCounter++;
		goto _RETRY_REPLY;
	}
	else if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	{
		MessageToLog( Port, "ExecuteCommand Get Data Receive errno", PROC_EXIST_DATA, errno );
		goto _END;
	}


	// Set Read All Length
	ReplyLength += 3;


	// Reply Check

	// Minimum Length Check
	if( ReplyLength < 5 )
	{
		ReadLength = 0;
	}

	// Set Length Check
	if( ReplyLength != ( ReadLength + 1 + 2 + 2 ) )
	{
		ReplyTempBufferIndex = ReplyTempBuffer;
		ReplyTempBufferIndex += ReplyLength;
		ReplyLength = 0;

		Returned = RS232C_ReceiveData(g_Information[Port].Handle,
					( char *)ReplyTempBufferIndex,
					MAX_DATA_ARRAY_SIZE,
					&ReplyLength,
					_RS232C_DEFAULT_CHAR_TIMEOUT);

	 	if( ( _RS232C_8_REPLY_TIMEOUT == ( ConvertRet = ErrorExchange( Returned ) ) ) ||
	 	    ( _RS232C_8_CHARACTER_TIMEOUT == ( ConvertRet = ErrorExchange( Returned ) ) ) )
        {
		 	// Send NAK
		 	RS232C_SendData(g_Information[Port].Handle,
					( char *)Control_NAK,
					Control_NAK_Length);

			RetryCounter++;
			goto _RETRY_REPLY;
		}
	 	else if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	 	{
			MessageToLog( Port, "ExecuteCommand Data Receive errno", PROC_EXIST_DATA, errno );
			goto _END;
		}
	}

	// CRC Check
	CrcCode = CrcCcitt( ReplyTempBuffer, ReplyLength - 2 );
	CRC1 = ( unsigned char )( CrcCode >> 8 );
	CRC2 = ( unsigned char )( CrcCode );
	if( !( ( CRC1 == *( ReplyTempBuffer + ReplyLength - 2 ) ) &&
	       ( CRC2 == *( ReplyTempBuffer + ReplyLength - 1 ) ) ) )
    {
		//Retry End Check
		if( DEFAULT_NAK_RETRY < RetryCounterforCRC )
		{
			ConvertRet = _RS232C_8_FAILED_TO_RECEIVE_REPLY_ERROR;
			goto _END;
		}

	 	// Send NAK
		MessageToLog( Port, "ExecuteCommand Send NAK", PROC_NO_DATA, 0 );
	 	RS232C_SendData(g_Information[Port].Handle,
			( char *)Control_NAK,
			Control_NAK_Length);

		RetryCounterforCRC++;

		goto _RETRY_REPLY;
	}

	// Send ACK
	RS232C_SendData(g_Information[Port].Handle,
		( char *)&Control_ACK[0],
		Control_ACK_Length);

_END:

	// Cancel Received Flag ON
	g_Information[Port].CancelReceiveFlag = TRUE;

	if( _RS232C_8_NO_ERROR == ConvertRet )
	{
		pText = ReplyTempBuffer + 3;

		// Set Return Data

		switch( *pText )
		{
			case  POSITIVE_RESPONSE_TAG_LARGE:
			case  POSITIVE_RESPONSE_TAG_SMALL:
				pReply->replyType = PositiveReply;
				break;

			case  NEGATIVE_RESPONSE_TAG_LARGE:
			case  NEGATIVE_RESPONSE_TAG_SMALL:
				pReply->replyType = NegativeReply;
				break;

			default:
				pReply->replyType = Unavailable;
				break;
		}

		pTextEnd = pText + ReplyLength;
		pIterator = pText + 1;

		// PositiveReply
		if( pReply->replyType == PositiveReply )
		{
			// Set Log Header Length
			HeaderLength = R_HEADER_LEN;

			pReply->message.positiveReply.CommandCode = *pIterator++;
			pReply->message.positiveReply.ParameterCode = *pIterator++;
			pReply->message.positiveReply.StatusCode.St1 = *pIterator++;
			pReply->message.positiveReply.StatusCode.St0 = *pIterator++;

			// Only TextData
			DataSize = ReplyLength - 1 - 2 - 5 - 2;

			if( DataSize )
			{
				pReply->message.positiveReply.Data.Size = DataSize;
				for( Counter = 0; Counter < (int)DataSize; Counter++ )
				{
					*( pReply->message.positiveReply.Data.Body + Counter ) =
										*( pIterator + Counter );
				}
			}
			else
			{

				pReply->message.positiveReply.Data.Size = 0;
				bzero( pReply->message.positiveReply.Data.Body, MAX_DATA_ARRAY_SIZE );
			}
		}
		// NegativeReply
		else if( pReply->replyType == NegativeReply )
		{
			// Set Log Header Length
			HeaderLength = R_HEADER_LEN;

			pReply->message.negativeReply.CommandCode = *pIterator++;
			pReply->message.negativeReply.ParameterCode = *pIterator++;
			pReply->message.negativeReply.ErrorCode.E1 = *pIterator++;
			pReply->message.negativeReply.ErrorCode.E0 = *pIterator++;

			// Only TextData
			DataSize = ReplyLength - 1 - 2 - 5 - 2;

			if( DataSize )
			{
				pReply->message.negativeReply.Data.Size = DataSize;
				for( Counter = 0; Counter < (int)DataSize; Counter++ )
				{
					*( pReply->message.negativeReply.Data.Body + Counter ) =
										*( pIterator + Counter );
				}
			}
			else
			{

				pReply->message.negativeReply.Data.Size = 0;
				bzero( pReply->message.negativeReply.Data.Body, MAX_DATA_ARRAY_SIZE );
			}
		}
		else // Others
		{

			ConvertRet = _RS232C_8_FAILED_TO_RECEIVE_REPLY_ERROR;
		}

		// Write Log
		DataMessageToLog( Port, ( char *)pText, DataSize, HeaderLength );
	}
	else
	{

		pReply->replyType = Unavailable;
	}

	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_CommandExecute );
	if( Ret )
	{
		pReply->replyType = Unavailable;
		ConvertRet = _RS232C_8_CRITICAL_SECTION_ERROR;
		MessageToLog(Port,
			"ExecuteCommand Return ( sem_CommandExecute out ) ",
			PROC_EXIST_DATA,
			ConvertRet);
		return	ConvertRet;
	}

	MessageToLog( Port, "ExecuteCommand Return ", PROC_EXIST_DATA, ConvertRet );

	return	ConvertRet;
}



long RS232C_8_CancelCommand(
	unsigned long	Port)
{
	char		Command_DLE_EOT[] = { DLE, EOT };
	unsigned long	Command_DLE_EOT_Length = sizeof( Command_DLE_EOT );
	long		Returned, ConvertRet;
	int		Ret;
	char		ReplyTempBuffer[ MAX_DATA_ARRAY_SIZE ];
	unsigned long	ReplyLength;


	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Connect Check
	if( !g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR;
	}

	// In CriticalSection
	Ret = sem_wait( &g_Information[Port].sem_CommandCancel );
	if( Ret )
	{
		MessageToLog(Port,
			"CancelCommand Return ( sem_CommandCancel in ) ",
			PROC_EXIST_DATA,
			_RS232C_8_CRITICAL_SECTION_ERROR);
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}

	Ret = pthread_mutex_lock(&cancel_mutex);
	if (Ret != 0) {
		return	_RS232C_8_CRITICAL_SECTION_ERROR;
	}
	if (command_step > 1) {

		Ret = pthread_mutex_unlock(&cancel_mutex);
		if (Ret != 0) {
				return	_RS232C_8_CRITICAL_SECTION_ERROR;
		}

		goto _END;
	}
	Ret = pthread_mutex_unlock(&cancel_mutex);
		if (Ret != 0) {
				return	_RS232C_8_CRITICAL_SECTION_ERROR;
		}




	// Cancel Processing Flag ON
	g_Information[Port].CancelFlag = TRUE;

	// Send DLE+EOT
	MessageToLog( Port, "CancelCommand Send", PROC_NO_DATA, 0 );
 	Returned = RS232C_SendData(g_Information[Port].Handle,
				Command_DLE_EOT,
				Command_DLE_EOT_Length);

 	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
 	{
		MessageToLog( Port, "ExecuteCommand Send errno", PROC_EXIST_DATA, errno );
		goto _END;
	}

	// Cancel Received Check
	if( g_Information[Port].CancelReceiveFlag )
	{
		// Receive DLE+EOT
	 	Returned = RS232C_ReceiveData(g_Information[Port].Handle,
					ReplyTempBuffer,
					2,
					&ReplyLength,
					DEFAULT_DLE_EOT_TIMEOUT);

	 	if( _RS232C_8_NO_ERROR != ( ConvertRet = ErrorExchange( Returned ) ) )
	 	{
			MessageToLog( Port, "ExecuteCommand ReceiveData errno", PROC_EXIST_DATA, errno );
			goto _END;
		}
	}

_END:
	// Cancel Processing Flag OFF
	g_Information[Port].CancelFlag = FALSE;



	// Out CriticalSection
	Ret = sem_post( &g_Information[Port].sem_CommandCancel );
	if( Ret )
	{
		ConvertRet = _RS232C_8_CRITICAL_SECTION_ERROR;
		MessageToLog(Port,
			"CancelCommand Return ( sem_CommandCancel out ) ",
			PROC_EXIST_DATA,
			ConvertRet);
		return	ConvertRet;
	}

	MessageToLog( Port, "CancelCommand Return ", PROC_EXIST_DATA, ConvertRet );

	return	ConvertRet;
}



long RS232C_8_ICCardTransmit(
	unsigned long	Port,
	unsigned long	DataSizeToSend,
	unsigned char	*pDataToSend,
	unsigned long 	SizeOfDataToReceive,
	unsigned long	*pSizeOfDataReceived,
	unsigned char	*pDataReceived,
	unsigned long 	*pAdditionalErrorCode,
	PREPLY	 	pReply)
{
	long	Returned;

	*pAdditionalErrorCode = 0;

	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Connect Check
	if( !g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR;
	}

	MessageToLog( Port, "ICCardTransmit Send", PROC_NO_DATA, 0 );

	Returned = _TransmitControl(Port,
                                DataSizeToSend,
                                pDataToSend,
                                SizeOfDataToReceive,
                                pSizeOfDataReceived,
                                pDataReceived,
                                pAdditionalErrorCode,
                                pReply,
                                ICCARD_TRANSMIT_FLAG);

	MessageToLog( Port, "ICCardTransmit Return ", PROC_EXIST_DATA, Returned );

	return	Returned;
}



long RS232C_8_SAMTransmit(
	unsigned long	Port,
	unsigned long	DataSizeToSend,
	unsigned char	*pDataToSend,
	unsigned long 	SizeOfDataToReceive,
	unsigned long	*pSizeOfDataReceived,
	unsigned char	*pDataReceived,
	unsigned long 	*pAdditionalErrorCode,
	PREPLY	 	pReply)
{
	long	Returned;


	*pAdditionalErrorCode = 0;

	// Port Check
	if( ( ( Port < 1 ) || ( MAX_PORT_NO < Port ) ) )
	{
		return	_RS232C_8_PARAMETER_ERROR;
	}

	// Connect Check
	if( !g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR;
	}

	MessageToLog( Port, "SAMTransmit Send", PROC_NO_DATA, 0 );

	Returned = _TransmitControl(Port,
                                DataSizeToSend,
                                pDataToSend,
                                SizeOfDataToReceive,
                                pSizeOfDataReceived,
                                pDataReceived,
                                pAdditionalErrorCode,
                                pReply,
                                SAM_TRANSMIT_FLAG);

	MessageToLog( Port, "SAMTransmit Return ", PROC_EXIST_DATA, Returned );

	return	Returned;
}



long RS232C_8_UpdateFirmware(
	unsigned long		Port,
	unsigned long		Baudrate,
	char 			*pFileName,
	PUPDATE_INIT_COMM	pUpdate_Init_Comm,
	int			CheckRevision,
	unsigned long 		*pAdditionalErrorCode,
	int			DisplayOn)
{
	long	Returned;


	MessageToLog( Port, "DownloadExe", PROC_NO_DATA, 0 );

	// Download Execute
	Returned = DownloadExe(Port,
                            Baudrate,
                            pUpdate_Init_Comm,
                            CheckRevision,
                            pAdditionalErrorCode,
                            DisplayOn,
                            pFileName);

	MessageToLog( Port, "DownloadExe Return ", PROC_EXIST_DATA, Returned );

	return	Returned;
}



void RS232C_8_GetLibraryRevision(
	PLIB_INFORMATION	pLibInfo)
{
	// Clear Buffer
	bzero( pLibInfo, sizeof( LIB_INFORMATION ) );

	// InterfaceLib
	{
		LIB_INFORMATION_RS232C_INTERFACE	LibInfoInterface;

		// Get InterfaceLib
		RS232C_Interface_GetLibraryRevision( &LibInfoInterface );

		// Set Filename
		memcpy(pLibInfo->InterfaceLib.Filename,
			LibInfoInterface.Filename,
			sizeof( LibInfoInterface.Filename ));

		// Set Revison
		memcpy(pLibInfo->InterfaceLib.Revision,
			LibInfoInterface.Revision,
			sizeof( LibInfoInterface.Revision ));
	}

	// CheckLib
	{
		LIB_INFORMATION_CHECK	LibInfoCheck;

		// Get CheckLib
		GetLibraryCheckRevision( &LibInfoCheck );

		// Set Filename
		memcpy(pLibInfo->CheckLib.Filename,
			LibInfoCheck.Filename,
			sizeof( LibInfoCheck.Filename ));

		// Set Revison
		memcpy(pLibInfo->CheckLib.Revision,
			LibInfoCheck.Revision,
			sizeof( LibInfoCheck.Revision ));
	}

	// ProtocolLib
	{
		char	LibraryRevision[16];

		// Get Library Data
		bzero( LibraryRevision, sizeof( LibraryRevision ) );
		GetLibraryRevision( LibraryRevision );

		// Set Filename
		memcpy(pLibInfo->ProtocolLib.Filename,
			RS232C_8_LIBRARY_FILENAME,
			sizeof( RS232C_8_LIBRARY_FILENAME ));

		// Set Revison
		memcpy(pLibInfo->ProtocolLib.Revision,
			LibraryRevision,
			strlen( LibraryRevision ));
	}

	return;
}


