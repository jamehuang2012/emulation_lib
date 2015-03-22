/**************************************************************************

	Prtcl_RS232C_FuncDef.c

	RS-232C 8bit Protocol Card Reader Library Inside Function

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

#include "Prtcl_RS232C_8.h"
#include "Prtcl_RS232C_8_Def.h"
#include "Prtcl_RS232C_8_FuncDef.h"
#include "RS232C_Interface.h"
#include "CRC_Proc.h"
#include "logger.h"

// Device Information Buffer
extern DEVICE_ACCESS_INFORMATION	g_Information[MAX_PORT_NO];



long RS232C_Init( void )
{
	int			Ret1, Ret2, Ret3;
	int			Counter;
	LIB_INFORMATION		LibInfo;
//	long			Detail;
	char			RevisionInfo[128];

	// Write Revision
	{
		char	*pTemp;

		pTemp = RevisionInfo;
		bzero( RevisionInfo, sizeof( RevisionInfo ) );

		RS232C_8_GetLibraryRevision( &LibInfo );

		// InterfaceLib
		{
			strncpy(
				pTemp,
				LibInfo.InterfaceLib.Filename,
				strlen( LibInfo.InterfaceLib.Filename )
				);
			pTemp += strlen( LibInfo.InterfaceLib.Filename );

			strncpy( pTemp, "  Rev ", strlen( "  Rev " ) );
			pTemp += strlen( "  Rev " );

			strncpy(
				pTemp,
				LibInfo.InterfaceLib.Revision,
				strlen( LibInfo.InterfaceLib.Revision )
				);
			pTemp += strlen( LibInfo.InterfaceLib.Revision );

			strncpy( pTemp, "\n", strlen( "\n" ) );
			pTemp += strlen( "\n" );
		}

		// CheckLib
		{
			strncpy(
				pTemp,
				LibInfo.CheckLib.Filename,
				strlen( LibInfo.CheckLib.Filename )
				);
			pTemp += strlen( LibInfo.CheckLib.Filename );

			strncpy( pTemp, "  Rev ", strlen( "  Rev " ) );
			pTemp += strlen( "  Rev " );

			strncpy(
				pTemp,
				LibInfo.CheckLib.Revision,
				strlen( LibInfo.CheckLib.Revision )
				);
			pTemp += strlen( LibInfo.CheckLib.Revision );

			strncpy( pTemp, "\n", strlen( "\n" ) );
			pTemp += strlen( "\n" );
		}

		// ProtocolLib
		{
			strncpy(
				pTemp,
				LibInfo.ProtocolLib.Filename,
				strlen( LibInfo.ProtocolLib.Filename )
				);
			pTemp += strlen( LibInfo.ProtocolLib.Filename );

			strncpy( pTemp, "  Rev ", strlen( "  Rev " ) );
			pTemp += strlen( "  Rev " );

			strncpy(
				pTemp,
				LibInfo.ProtocolLib.Revision,
				strlen( LibInfo.ProtocolLib.Revision )
				);
			pTemp += strlen( LibInfo.ProtocolLib.Revision );

			strncpy( pTemp, "\n", strlen( "\n" ) );
			pTemp += strlen( "\n" );
		}

		//CollectLogExOpen( strlen( RevisionInfo ), RevisionInfo, &Detail );
	}


	// Init CriticalSection
	for( Counter = 0; Counter < MAX_PORT_NO; Counter++ )
	{
		Ret1 = sem_init( &g_Information[Counter].sem_Connect, 0, 1 );
		Ret2 = sem_init( &g_Information[Counter].sem_CommandExecute, 0, 1 );
		Ret3 = sem_init( &g_Information[Counter].sem_CommandCancel, 0, 1 );

		if( Ret1 || Ret2 || Ret3 )
		{
			return	_RS232C_8_CRITICAL_SECTION_ERROR;
		}
	}

	return	_RS232C_8_NO_ERROR;
}



void RS232C_End(
	unsigned long	Port)
{
	int	Counter;
	//long	Detail;

Port = 1;
	// Destroy CriticalSection
	for( Counter = 0; Counter < MAX_PORT_NO; Counter++ )
	{
		sem_destroy( &g_Information[Counter].sem_Connect );
		sem_destroy( &g_Information[Counter].sem_CommandExecute );
		sem_destroy( &g_Information[Counter].sem_CommandCancel );
	}

	//CollectLogExClose( &Detail );

	return;
}



long _TransmitControl(
	unsigned long	Port,
	unsigned long	DataSizeToSend,
	unsigned char	*pDataToSend,
	unsigned long	SizeOfDataToReceive,
	unsigned long	*pSizeOfDataReceived,
	unsigned char	*pDataReceived,
	unsigned long 	*pAdditionalErrorCode,
	PREPLY 		pReply,
	unsigned char	ICCard_SAM)
{
	long		Result;
	int		Complete;
	unsigned long	CurrentDataSize = 0;


	*pAdditionalErrorCode = _RS232C_8_NO_ERROR;

	// Set Long Data or Not Flag
	g_Information[Port].LongData = TRUE;

	// First
	Result = TransmitControl(
				Port,
				TRUE,
				DataSizeToSend,
				pDataToSend,
				&Complete,
				pAdditionalErrorCode,
				pReply,
				ICCard_SAM
				);

	if( _RS232C_8_NO_ERROR != Result )
	{
		goto _END;
	}

	// Data Length Check
	if( SizeOfDataToReceive >=
			( CurrentDataSize + pReply->message.positiveReply.Data.Size ) )
    {
		// Received Data Copy
		memcpy(
			pDataReceived + CurrentDataSize,
			pReply->message.positiveReply.Data.Body,
			pReply->message.positiveReply.Data.Size
			);

		// Increment Received Data Size
		CurrentDataSize += pReply->message.positiveReply.Data.Size;
	}
	else
	{
		// Copy Data Assigned Length
		memcpy(
			pDataReceived + CurrentDataSize,
			pReply->message.positiveReply.Data.Body,
			SizeOfDataToReceive - CurrentDataSize
			);

		// Set Assigned Length
		CurrentDataSize = SizeOfDataToReceive;

		goto _END;
	}

	// After Second, Repeat Until End
	while( !Complete )
	{
		Result = TransmitControl(
					Port,
					FALSE,
					0,
					NULL,
					&Complete,
					pAdditionalErrorCode,
					pReply,
					ICCard_SAM
					);

		if( _RS232C_8_NO_ERROR != Result )
		{
			goto _END;
		}

		// No End
		if( !Complete )
		{
			// Data Length Check
			if( SizeOfDataToReceive >=
				( CurrentDataSize + pReply->message.positiveReply.Data.Size ) )
            {
				// Received Data Copy
				memcpy(
					pDataReceived + CurrentDataSize,
					pReply->message.positiveReply.Data.Body,
					pReply->message.positiveReply.Data.Size
					);

				// Increment Received Data Size
				CurrentDataSize += pReply->message.positiveReply.Data.Size;
			}
			else
			{
				// Copy Data Assigned Length
				memcpy(
					pDataReceived + CurrentDataSize,
					pReply->message.positiveReply.Data.Body,
					SizeOfDataToReceive - CurrentDataSize
					);

				// Set Assigned Length
				CurrentDataSize = SizeOfDataToReceive;

				goto _END;
			}
		}
	}

_END:

	// Set Data Length
	*pSizeOfDataReceived = CurrentDataSize;

	// Clear
	g_Information[Port].LongData = FALSE;

	return	Result;
}



long TransmitControl(
	unsigned long	Port,
	int		Init,
	unsigned long	DataSizeToSend,
	unsigned char	*pDataToSend,
	int		*pComplete,
	unsigned long	*pAdditionalErrorCode,
	PREPLY		pReply,
	unsigned char	ICCard_SAM)
{
	COMMAND		Command;
	long		ReturnValue = _RS232C_8_NO_ERROR;

	*pComplete = FALSE;

	// First Proc
	if( Init )
	{
		// Set Send Data Length
		g_Information[Port].DataSizeToSend = DataSizeToSend;

		// Data Area Allocate
		g_Information[Port].pDataStrage = ( char * )malloc( DataSizeToSend );

		if( NULL == g_Information[Port].pDataStrage )
		{
			ReturnValue = _RS232C_8_TRANSMIT_FAILED_TO_ALLOCATE_MEMORY_REGION_ERROR;
			goto _END;
		}

		// Send Data Copy
		memcpy( g_Information[Port].pDataStrage, pDataToSend, DataSizeToSend );

		// Set Pointer Index
		g_Information[Port].pIndex = g_Information[Port].pDataStrage;

		// Only One Transmit
		if( g_Information[Port].DataSizeToSend <= GetMax_IC_SAM_DataLength() )
		{
			// Automatic communication
			{
				Command.CommandCode = 0x49;
				Command.ParameterCode = 0x04 | ICCard_SAM;
				Command.Data.Size = g_Information[Port].DataSizeToSend;
				Command.Data.pBody = (unsigned char *)g_Information[Port].pIndex;
			}

			*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
									Port,
									&Command,
									DEFAULT_REPLY_TIMEOUT,
									pReply
									);

			if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
			{
				// Failed Execute
				ReturnValue = _RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR;
				goto _END;
			}
			else if( NegativeReply == pReply->replyType )
			{
				// Receive Negative Reply
				ReturnValue = _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR;
				goto _END;
			}
			else if( PositiveReply != pReply->replyType )
			{
				// Unexpected Situation
				ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
				goto _END;
			}
			else if( ( 0x0F | ICCard_SAM ) ==
						pReply->message.positiveReply.ParameterCode )
            {
				// Receive ABORT REQUEST
				ReturnValue = _RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR;
				goto _END;
			}

			// Update Send Data Size
			g_Information[Port].DataSizeToSend = 0;
		}
		// Over Two Transmits
		else
		{
			// Communication1-1
			{
				Command.CommandCode = 0x49;
				Command.ParameterCode = 0x05 | ICCard_SAM;
				Command.Data.Size = GetMax_IC_SAM_DataLength();
				Command.Data.pBody = (unsigned char *)g_Information[Port].pIndex;
			}

			*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
									Port,
									&Command,
									DEFAULT_REPLY_TIMEOUT,
									pReply
									);

			if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
			{
				// Failed Execute
				ReturnValue = _RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR;
				goto _END;
			}
			else if( NegativeReply == pReply->replyType )
			{
				// Receive Negative Reply
				ReturnValue = _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR;
				goto _END;
			}
			else if( PositiveReply != pReply->replyType )
			{
				// Unexpected Situation
				ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
				goto _END;
			}
			else if( ( 0x0F | ICCard_SAM ) ==
						pReply->message.positiveReply.ParameterCode )
            {
				// Receive ABORT REQUEST
				ReturnValue = _RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR;
				goto _END;
			}

			// Set Pointer IndexInformation
			g_Information[Port].pIndex += GetMax_IC_SAM_DataLength();

			// Update Send Data Size
			g_Information[Port].DataSizeToSend -= GetMax_IC_SAM_DataLength();
		}

		// Set ParameterCode
		g_Information[Port].LastTimeParameterCode =
						pReply->message.positiveReply.ParameterCode;
	}
	// After Second Proc
	else
	{

		// No Remain Data
		if( 0 == g_Information[Port].DataSizeToSend )
		{
			if( ( 0x05 | ICCard_SAM ) == g_Information[Port].LastTimeParameterCode )
			{
				// Communication1-3
				{
					Command.CommandCode = 0x49;
					Command.ParameterCode = 0x07 | ICCard_SAM;
					Command.Data.Size = 0;
					Command.Data.pBody = NULL;
				}

				*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
										Port,
										&Command,
										DEFAULT_REPLY_TIMEOUT,
										pReply
										);

				if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
				{
					// Failed Execute
					ReturnValue = _RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR;
					goto _END;
				}
				else if( NegativeReply == pReply->replyType )
				{
					// Receive Negative Reply
					ReturnValue = _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR;
					goto _END;
				}
				else if( PositiveReply != pReply->replyType )
				{
					// Unexpected Situation
					ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
					goto _END;
				}
				else if( ( 0x0F | ICCard_SAM ) ==
							pReply->message.positiveReply.ParameterCode )
                {
					// Receive ABORT REQUEST
					ReturnValue = _RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR;
					goto _END;
				}
			}
			else if( ( ( 0x04 | ICCard_SAM ) ==
						g_Information[Port].LastTimeParameterCode ) ||
				 ( ( 0x06 | ICCard_SAM ) ==
						g_Information[Port].LastTimeParameterCode ) )
            {
				// Set End
				*pComplete = TRUE;
			}
			else
			{
				// Unexpected Situation
				ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
				goto _END;
			}
		}
		// Remain Data Less Equal Than Max Transmit Size
		else if( g_Information[Port].DataSizeToSend <= GetMax_IC_SAM_DataLength() )
		{
			// Communication1-2
			{
				Command.CommandCode = 0x49;
				Command.ParameterCode = 0x06 | ICCard_SAM;
				Command.Data.Size = g_Information[Port].DataSizeToSend;
				Command.Data.pBody = (unsigned char *)g_Information[Port].pIndex;
			}

			*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
									Port,
									&Command,
									DEFAULT_REPLY_TIMEOUT,
									pReply
									);

			if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
			{
				// Failed Execute
				ReturnValue = _RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR;
				goto _END;
			}
			else if( NegativeReply == pReply->replyType )
			{
				// Receive Negative Reply
				ReturnValue = _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR;
				goto _END;
			}
			else if( PositiveReply != pReply->replyType )
			{
				// Unexpected Situation
				ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
				goto _END;
			}
			else if( ( 0x0F | ICCard_SAM ) ==
						pReply->message.positiveReply.ParameterCode )
            {
				// Receive ABORT REQUEST
				ReturnValue = _RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR;
				goto _END;
			}

			// Update Send Data Size
			g_Information[Port].DataSizeToSend = 0;
		}
		// Remain Data Larger Than Max Transmit Size
		else
		{
			// Communication1-1
			{
				Command.CommandCode = 0x49;
				Command.ParameterCode = 0x05 | ICCard_SAM;;
				Command.Data.Size = GetMax_IC_SAM_DataLength();
				Command.Data.pBody = (unsigned char *)g_Information[Port].pIndex;
			}

			*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
									Port,
									&Command,
									DEFAULT_REPLY_TIMEOUT,
									pReply
									);

			if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
			{
				// Failed Execute
				ReturnValue = _RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR;
				goto _END;
			}
			else if( NegativeReply == pReply->replyType )
			{
				// Receive Negative Reply
				ReturnValue = _RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR;
				goto _END;
			}
			else if( PositiveReply != pReply->replyType )
			{
				// Unexpected Situation
				ReturnValue = _RS232C_8_TRANSMIT_UNEXPECTED_ERROR;
				goto _END;
			}
			else if( ( 0x0F | ICCard_SAM ) ==
						pReply->message.positiveReply.ParameterCode )
            {
				// Receive ABORT REQUEST
				ReturnValue = _RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR;
				goto _END;
			}

			// Set Pointer Index
			g_Information[Port].pIndex += GetMax_IC_SAM_DataLength();

			// Update Send Data Size
			g_Information[Port].DataSizeToSend -= GetMax_IC_SAM_DataLength();
		}

		// Set ParameterCode
		g_Information[Port].LastTimeParameterCode =
						pReply->message.positiveReply.ParameterCode;
	}

_END:

	// Error
	if( _RS232C_8_NO_ERROR != ReturnValue )
	{
		*pComplete = TRUE;
	}

	// End
	if( *pComplete )
	{
		// Release Data Area
		free( g_Information[Port].pDataStrage );
	}

	return	ReturnValue;
}



long DownloadGetFileInfo(
	char		*pFileName,
	int		*pFileType,
	unsigned long	*pFileSize)
{
	long	Returned = _RS232C_8_NO_ERROR;
	FILE	*FilePtr;
	int	TempChar;
	char	*pExtension;


	// Get File Type
	pExtension = strrchr( pFileName, '.' );
	if( !pExtension )
	{
		Returned = _RS232C_8_UPDATE_FIRMWARE_UNKNOWN_FILE_TYPE_ERROR;
		goto _EXIT;
	}

	if( !( strcasecmp( pExtension, ".dwl" ) ) )
	{
		*pFileType = DWL_TYPE;
	}

	else if( !( strcasecmp( pExtension, ".bin" ) ) )
	{
		*pFileType = BIN_TYPE;
	}

	else if( !( strcasecmp( pExtension, ".dat" ) ) )
	{
		*pFileType = DAT_TYPE;
	}
	else
	{
		Returned = _RS232C_8_UPDATE_FIRMWARE_UNKNOWN_FILE_TYPE_ERROR;
		goto _EXIT;
	}


	// Open File
	FilePtr = fopen( pFileName, "r" );

	if( NULL == FilePtr )
	{
		printf( "\nFile Open Error!!\n" );
		Returned = _RS232C_8_UPDATE_FIRMWARE_CANNOT_OPEN_FILE_ERROR;
		goto _EXIT;
	}

	// Get File Size
	*pFileSize = 0;

	for( ;; )
	{
		TempChar = fgetc( FilePtr );

		if( EOF == TempChar )
		{
			break;
		}

		(*pFileSize)++;
	}

	// Close File
	fclose( FilePtr );

	if( !( *pFileSize ) )
	{
		printf( "\nFile Read Error!!\n" );
		Returned = _RS232C_8_UPDATE_FIRMWARE_CANNOT_READ_FILE_ERROR;
		goto _EXIT;
	}

_EXIT:

	return	Returned;
}



long DownloadGetFileData(
	char		*pFileName,
	unsigned long	*pTotalNumberOfLines,
	char		*pDataBuffer)
{
	long		Returned = _RS232C_8_NO_ERROR;
	FILE		*FilePtr;
	int		TempChar;
	char		*pIndex;


	// Open File
	FilePtr = fopen( pFileName, "r" );

	if( NULL == FilePtr )
	{
		printf( "\nFile Open Error!!\n" );
		Returned = _RS232C_8_UPDATE_FIRMWARE_CANNOT_OPEN_FILE_ERROR;
		goto _EXIT;
	}

	// Read Data, Change Control Code to NULL Terminal, and Get total Number of Lines

	*pTotalNumberOfLines = 0;
	pIndex = pDataBuffer;

	for( ;; )
	{
		TempChar = fgetc( FilePtr );

		if( EOF == TempChar )
		{
			*pIndex++ = '\0';
			break;
		}

		if( 0x0D == TempChar )
		{
			TempChar = fgetc( FilePtr );

			if( 0x0A == TempChar )
			{
				// Control Code	( Return )
				*pIndex++ = '\0';
				(*pTotalNumberOfLines)++;
			}
		}
		else
		{
			*pIndex++ = TempChar;
		}
	}

	// Close File
	fclose( FilePtr );

_EXIT:

	return	Returned;
}



long DownloadExe(
	unsigned long		Port,
	unsigned long		Baudrate,
	PUPDATE_INIT_COMM	pUpdate_Init_Comm,
	int			CheckRevision,
	unsigned long		*pAdditionalErrorCode,
	int			DisplayOn,
	char			*pFileName)
{
	COMMAND			Command;
	REPLY			Reply;
	unsigned char		RevisionOfUserArea[16];
	long			ReturnValue = _RS232C_8_NO_ERROR;
	int			Count;
	unsigned char		SVInitData[] = { '0', '0', '0', '0', '0' };
	unsigned char		LineBuffer[MAX_DATA_ARRAY_SIZE];
	unsigned char		*pIndex;
	int			Index;
	int			DisplayNo = 10;
	int			DisplayCounter = 0;
	int			FileType;
	char			*pDataBuffer = NULL;
	unsigned long		TotalNumberOfLines;
	char			Revision[ LENGTH_OF_REVISION + 1 ];
	unsigned long		FileSize;
	char			*pTempRev;

	// Connect Check
	if( g_Information[Port].ConnectFlag )
	{
		return	_RS232C_8_UPDATE_FIRMWARE_DEVICE_ALREADY_CONNECTED_ERROR;
	}

	// Connect
	*pAdditionalErrorCode = RS232C_8_ConnectDevice( Port, Baudrate );
	if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
	{
		if( _RS232C_8_DEVICE_ALREADY_CONNECTED_ERROR == *pAdditionalErrorCode )
		{
			// Already Connected
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_DEVICE_ALREADY_CONNECTED_ERROR;
			return	ReturnValue;
		}

		// Other Error
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_CONNECT_DEVICE_FAILED_ERROR;
		return	ReturnValue;
	}

	// Get Download File Infomations
	ReturnValue = DownloadGetFileInfo( pFileName, &FileType, &FileSize );

	if( ReturnValue != _RS232C_8_NO_ERROR )
	{
		goto _EXIT;
	}

	// Allocate Data Area
	pDataBuffer = ( char * )malloc( FileSize );

	if( !pDataBuffer )
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_FAILED_TO_ALLOCATE_MEMORY_REGION_ERROR;
		goto _EXIT;
	}

	// Get Download File Data
	ReturnValue = DownloadGetFileData( pFileName, &TotalNumberOfLines, pDataBuffer );

	if( ReturnValue != _RS232C_8_NO_ERROR )
	{
		goto _EXIT;
	}

	// Get Revision from File
	pTempRev = strstr( pDataBuffer, SUFFIX_OF_FILE_REVISION );
	if( NULL != pTempRev )
	{
		memcpy(
			Revision,
			pTempRev + strlen( SUFFIX_OF_FILE_REVISION ),
			LENGTH_OF_REVISION + 1
			);
	}
	else
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_FILE_CONTENTS_ERROR;
		goto _EXIT;
	}


	// Check Target Mode, SV or USER
	for( ;; )
	{
		// Set Initialize Data for UpdateFirmware
		Command.CommandCode = 0x30;
		Command.ParameterCode = pUpdate_Init_Comm->ParameterCode;
		Command.Data.pBody = pUpdate_Init_Comm->Data.pBody;
		Command.Data.Size = pUpdate_Init_Comm->Data.Size;

		// Initialize
		*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
								Port,
								&Command,
								DEFAULT_REPLY_TIMEOUT,
								&Reply
								);

		if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
			goto _EXIT;
		}

		// Mode Detect
		if( ( NegativeReply == Reply.replyType ) &&
		    ( '0' == Reply.message.negativeReply.ErrorCode.E1 ) &&
		    ( '2' == Reply.message.negativeReply.ErrorCode.E0 ) )
        {
			// Supervisor

			if( CheckRevision )
			{
				// CRD Area Change
				ReturnValue = AreaChange(
							Port,
							Baudrate,
							pAdditionalErrorCode
							);

				if( _RS232C_8_NO_ERROR != ReturnValue )
				{
					goto _EXIT;
				}

				continue;
			}
		}
		else if( ( PositiveReply == Reply.replyType ) ||
			 ( NegativeReply == Reply.replyType ) )
        {
			// User
			if( CheckRevision )
			{
				// Revision
				Command.CommandCode = 0x41;
				Command.ParameterCode = 0x31;
				Command.Data.pBody = NULL;
				Command.Data.Size = 0;

				*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
										Port,
										&Command,
										DEFAULT_REPLY_TIMEOUT,
										&Reply
										);

				if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
				{
					ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
					goto _EXIT;
				}
				else if( NegativeReply == Reply.replyType )
				{
					ReturnValue = _RS232C_8_UPDATE_FIRMWARE_NEGATIVE_REPLY_RECEIVED_ERROR;
					goto _EXIT;
				}
				else if( PositiveReply != Reply.replyType )
				{
					ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
					goto _EXIT;
				}

				memcpy(
					RevisionOfUserArea,
					Reply.message.positiveReply.Data.Body,
					Reply.message.positiveReply.Data.Size
					);

				*( RevisionOfUserArea + Reply.message.positiveReply.Data.Size ) = '\0';

				if( !( strcmp( (char *)RevisionOfUserArea, Revision ) ) )
				{
					ReturnValue = _RS232C_8_UPDATE_FIRMWARE_IDENTICAL_REVISION_ERROR;
					goto _EXIT;
				}
			}

			// CRD Area Change
			ReturnValue = AreaChange( Port, Baudrate, pAdditionalErrorCode );

			if( _RS232C_8_NO_ERROR != ReturnValue )
			{
				goto _EXIT;
			}


			// Supervisor Initialize
			Command.CommandCode = 0x30;
			Command.ParameterCode = 0x30;
			Command.Data.pBody = SVInitData;
			Command.Data.Size = sizeof( SVInitData ) / sizeof( unsigned char );

			*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
									Port,
									&Command,
									DEFAULT_REPLY_TIMEOUT,
									&Reply
									);

			if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
			{
				ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
				goto _EXIT;
			}
			else if( ( NegativeReply == Reply.replyType ) &&
				 ( '0' == Reply.message.negativeReply.ErrorCode.E1 ) &&
				 ( '2' == Reply.message.negativeReply.ErrorCode.E0 ) )
            {
				// Success
			}
			else
			{
				ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
				goto _EXIT;
			}
		}
		else
		{

			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
			goto _EXIT;
		}

		break;
	}


	// Download Start

	// Send Command J0 ( .DAT File )
	if( DAT_TYPE == FileType )
	{
		Command.CommandCode = 0x4A;
		Command.ParameterCode = 0x30;
		Command.Data.Size = 0;

		*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
								Port,
								&Command,
								DEFAULT_REPLY_TIMEOUT,
								&Reply
								);

		if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
			goto _EXIT;
		}
		else if( NegativeReply == Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_NEGATIVE_REPLY_RECEIVED_ERROR;
			goto _EXIT;
		}
		else if( PositiveReply != Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
			goto _EXIT;
		}
	}

	pIndex = (unsigned char *)pDataBuffer;
	for( Count = 0; Count < (int)TotalNumberOfLines; Count++ )
	{
		// First Line
		if( 0 == Count )
		{
			while( '\0' != *pIndex )
			{
				pIndex++;
			}

			pIndex++;
			continue;
		}

		// Set Pointer
		bzero( LineBuffer, sizeof( LineBuffer ) );

		// Set Data
		Index = 0;
		while( '\0' != *pIndex )
		{
			*( LineBuffer + Index++ ) = *pIndex++;
		}

		pIndex++;

		// .WDL File and .Bin File
		if( DAT_TYPE != FileType )
		{
			// Check Line Data
			if( Index < 3 )
			{
				ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_FILE_CONTENTS_ERROR;
				goto _EXIT;
			}

			// Check Head Character
			if( 'C' != *LineBuffer )
			{
				ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_FILE_CONTENTS_ERROR;
				goto _EXIT;
			}

			Command.CommandCode = *( LineBuffer + 1 );
			Command.ParameterCode = *( LineBuffer + 2 );
			Command.Data.Size = Index - 3;
			Command.Data.pBody = LineBuffer + 3;
		}
		// .Dat File
		else
		{

			Command.CommandCode = 0x4A;
			Command.ParameterCode = 0x31;
			Command.Data.Size = Index;
			Command.Data.pBody = LineBuffer;
		}

		*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
								Port,
								&Command,
								DEFAULT_REPLY_TIMEOUT,
								&Reply
								);

		if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
			goto _EXIT;
		}
		else if( NegativeReply == Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_NEGATIVE_REPLY_RECEIVED_ERROR;
			goto _EXIT;
		}
		else if( PositiveReply != Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
			goto _EXIT;
		}

		// Display Progress

		if( DisplayOn )
		{
			if( (int)( ( Count * 100 ) / TotalNumberOfLines ) > DisplayNo )
			{
				for( DisplayCounter = 0;
				     DisplayCounter < DisplayNo;
				     DisplayCounter += 5 )
                {
					printf( "*" );
				}

				printf( " (%d)\n", DisplayNo );
				DisplayNo += 10;
			}
		}
	}

	// Display Progress End
	if( DisplayOn )
	{
		for( DisplayCounter = 0; DisplayCounter < DisplayNo; DisplayCounter += 5 )
		{
			printf( "*" );
		}
		printf( " (100)\n" );
	}


	// Send Command J2 ( .DAT File )
	if( DAT_TYPE == FileType )
	{
		Command.CommandCode = 0x4A;
		Command.ParameterCode = 0x32;
		Command.Data.Size = 0;
		*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
								Port,
								&Command,
								DEFAULT_REPLY_TIMEOUT,
								&Reply
								);

		if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
			goto _EXIT;
		}
		else if( NegativeReply == Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_NEGATIVE_REPLY_RECEIVED_ERROR;
			goto _EXIT;
		}
		else if( PositiveReply != Reply.replyType )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
			goto _EXIT;
		}
	}


	// CRD Area Change
	ReturnValue = AreaChange( Port, Baudrate, pAdditionalErrorCode );

	if( _RS232C_8_NO_ERROR != ReturnValue )
	{
		goto _EXIT;
	}


	// Set Initialize Data for UpdateFirmware
	Command.CommandCode = 0x30;
	Command.ParameterCode = pUpdate_Init_Comm->ParameterCode;
	Command.Data.pBody = pUpdate_Init_Comm->Data.pBody;
	Command.Data.Size = pUpdate_Init_Comm->Data.Size;

	// Initialize
	*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
							Port,
							&Command,
							DEFAULT_REPLY_TIMEOUT,
							&Reply
							);

	if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
		goto _EXIT;
	}

_EXIT:

	// Release Data Area
	if( pDataBuffer )
	{
		free( pDataBuffer );
	}

	// Disconnect
	RS232C_8_DisconnectDevice( Port );

	return	ReturnValue;
}



long AreaChange(
	unsigned long	Port,
	unsigned long	Baudrate,
	unsigned long	*pAdditionalErrorCode)
{
	long		ReturnValue = _RS232C_8_NO_ERROR;
	COMMAND		Command;
	REPLY		Reply;
	int		Count;


	// Switch
	Command.CommandCode = 0x4B;
	Command.ParameterCode = 0x30;
	Command.Data.pBody = NULL;
	Command.Data.Size = 0;

	*pAdditionalErrorCode = RS232C_8_ExecuteCommand(
							Port,
							&Command,
							DEFAULT_REPLY_TIMEOUT,
							&Reply
							);

	if( _RS232C_8_NO_ERROR != *pAdditionalErrorCode )
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR;
		goto _EXIT;
	}
	else if( NegativeReply == Reply.replyType )
	{
		// Received Negative ( No User Area )
	}
	else if( PositiveReply != Reply.replyType )
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR;
		goto _EXIT;
	}


	// Disconnect
	*pAdditionalErrorCode = RS232C_8_DisconnectDevice( Port );

	if( _RS232C_8_NO_ERROR == *pAdditionalErrorCode )
	{
		// Success
	}
	else if( _RS232C_8_DEVICE_NOT_CONNECTED_ERROR == *pAdditionalErrorCode )
	{
		// Regard Success
	}
	else
	{
		ReturnValue = _RS232C_8_UPDATE_FIRMWARE_DISCONNECT_DEVICE_FAILED_ERROR;
		goto _EXIT;
	}

	sleep(1);

	// Reconnect
	for( Count=0 ;; Count++ )
	{
		*pAdditionalErrorCode = RS232C_8_ConnectDevice( Port, Baudrate );

		if( _RS232C_8_NO_ERROR == *pAdditionalErrorCode )
		{
			break;
		}

		else if( Count > 10 )
		{
			ReturnValue = _RS232C_8_UPDATE_FIRMWARE_CONNECT_DEVICE_FAILED_ERROR;
			goto _EXIT;
		}

		sleep(1);
	}

_EXIT:

	return	ReturnValue;
}



long ErrorExchange(
	long	Error)
{
	long	Returned = 0;


	switch( Error )
	{
		case _RS232C_NO_ERROR :
			Returned = _RS232C_8_NO_ERROR;
			break;

		case _RS232C_ERROR_DATA_SEND :
			Returned = _RS232C_8_FAILED_TO_SEND_COMMAND_ERROR;
			break;

		case _RS232C_ERROR_DATA_RECEIVE :
			Returned = _RS232C_8_FAILED_TO_RECEIVE_REPLY_ERROR;
			break;

		case _RS232C_ERROR_TIMEOUT :
			Returned = _RS232C_8_REPLY_TIMEOUT;
			break;

		case _RS232C_ERROR_CHARACTER_TIMEOUT :
			Returned = _RS232C_8_CHARACTER_TIMEOUT;
			break;
	}

	return	Returned;
}



void MessageToLog(
	unsigned long	Port,
	char		*pMessage,
	unsigned long	Data,
	long		Ret)
{
	char	Temp[1024];
	//long	Detail;


	bzero( Temp, sizeof( Temp ) );

	if( PROC_NO_DATA == Data )
	{
		sprintf( Temp, "COM%d> %s", (unsigned int)Port, pMessage );
	}
	else
	{
		sprintf( Temp, "COM%d> %s %x", (unsigned int)Port, pMessage, (unsigned int)Ret );
	}

	//CollectLogExWrite( strlen( Temp ), Temp, &Detail );
	ALOGD("CCR","%s",Temp);

	return;
}



void DataMessageToLog(
	unsigned long	Port,
	char		*pCommand_Data,
	unsigned long	Command_Data_Length,
	unsigned long	HeaderLength)
{
	char		Temp1[64];
	char		CommandMessageLogBuffer[MAX_DATA_ARRAY_SIZE * 5];
	unsigned char	*pPtr = ( unsigned char *)CommandMessageLogBuffer;
	unsigned char	*pCmc = ( unsigned char *)pCommand_Data;
	int		Counter1, Length;
	//long		Detail;


	bzero( Temp1, sizeof( Temp1 ) );
	bzero( CommandMessageLogBuffer, sizeof( CommandMessageLogBuffer ) );

	if( S_HEADER_LEN == HeaderLength )
	{
		sprintf( Temp1, "COM%d> -> ", (unsigned int)Port );
	}
	else
	{
		sprintf( Temp1, "COM%d> <- ", (unsigned int)Port );
	}

	Length = strlen( Temp1 );

	// Set Port Number
	for( Counter1 = 0; Counter1 < Length; Counter1++ )
	{
		*pPtr++ = Temp1[Counter1];
	}

	// Set Header Part with Text
	for( Counter1 = 0; Counter1 < (int)HeaderLength; Counter1++ )
	{
		*pPtr++ = *pCmc++;
	}

	// Set Data Part with Binary
	{
		char	Temp2[4];
		int	Counter2;

		for( Counter1 = 0; Counter1 < (int)Command_Data_Length; Counter1++ )
		{
			if( ( ( Counter1 + 16 ) % 16 ) == 0 )
			{
				*pPtr++ = '\n';

				for( Counter2 = 0; Counter2 < HEADER_SPACE; Counter2++ )
				{
					*pPtr++ = ' ';
				}
			}
			else if( ( ( Counter1 + 8 ) % 8 ) == 0 )
			{
				*pPtr++ = ' ';
				*pPtr++ = '-';
				*pPtr++ = ' ';
			}
			else
			{
				*pPtr++ = ' ';
			}

			sprintf( Temp2, "%02x", *pCmc++);
			*pPtr++ = Temp2[0];
			*pPtr++ = Temp2[1];
		}
	}
/*
	// Write Log
	CollectLogExWrite(
			( char * )pPtr - CommandMessageLogBuffer,
			CommandMessageLogBuffer,
			&Detail
		);
*/
		
}


