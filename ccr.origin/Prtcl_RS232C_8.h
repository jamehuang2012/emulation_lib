/**************************************************************************

	Prtcl_RS232C_8.h

	RS-232C 8bit Protocol Card Reader Library
	Header File

***************************************************************************/

#ifndef	_PROTOCOL_RS232C_8_H_
#define	_PROTOCOL_RS232C_8_H_

#include "RS232C_Def.h"


#ifdef __cplusplus
extern "C"
{
#endif



//----------------------------------------------------------------------
//	Return codes of the APIs
//----------------------------------------------------------------------


#define	_ERROR_CODE_ORIGIN				(0x0000)

/*
	Return codes of the following API(s):
	- RS232C_8_ConnectDevice
	- RS232C_8_DisconnectDevice
	- RS232C_8_ExecuteCommand
	- RS232C_8_CancelCommand
	- RS232C_8_GetLibraryRevision
*/
#define	_RS232C_8_NO_ERROR				( _ERROR_CODE_ORIGIN + 0x0 )
#define	_RS232C_8_DEVICE_NOT_CONNECTED_ERROR		( _ERROR_CODE_ORIGIN + 0x1 )
#define	_RS232C_8_CANCEL_COMMAND_SESSION_ERROR		( _ERROR_CODE_ORIGIN + 0x2 )
#define	_RS232C_8_FAILED_TO_SEND_COMMAND_ERROR		( _ERROR_CODE_ORIGIN + 0x3 )
#define	_RS232C_8_FAILED_TO_RECEIVE_REPLY_ERROR		( _ERROR_CODE_ORIGIN + 0x4 )
#define	_RS232C_8_COMMAND_CANCELED			( _ERROR_CODE_ORIGIN + 0x5 )
#define	_RS232C_8_REPLY_TIMEOUT				( _ERROR_CODE_ORIGIN + 0x6 )
#define	_RS232C_8_ACK_TIMEOUT				( _ERROR_CODE_ORIGIN + 0x7 )
#define	_RS232C_8_FAILED_TO_RECEIVE_ACK_ERROR		( _ERROR_CODE_ORIGIN + 0x8 )
#define	_RS232C_8_NAK_RECEIVE_OVERTIMES			( _ERROR_CODE_ORIGIN + 0x9 )
#define	_RS232C_8_CHARACTER_TIMEOUT			( _ERROR_CODE_ORIGIN + 0xA )
#define	_RS232C_8_CRITICAL_SECTION_ERROR		( _ERROR_CODE_ORIGIN + 0xB )
#define	_RS232C_8_PARAMETER_ERROR			( _ERROR_CODE_ORIGIN + 0xC )


/*
	Return codes of the following API(s):
	- RS232C_8_ConnectDevice
*/
#define	_RS232C_8_CANNOT_OPEN_PORT_ERROR		( _ERROR_CODE_ORIGIN + 0x0301 )
#define	_RS232C_8_DEVICE_ALREADY_CONNECTED_ERROR	( _ERROR_CODE_ORIGIN + 0x0302 )

/*
	Return codes of the following API(s):
	- RS232C_8_DisconnectDevice

	nothing unique ErrorCode
*/

/*
	Return codes of the following API(s):
	- RS232C_8_CancelCommand

	nothing unique ErrorCode
*/


/*
	Return codes of the following API(s):
	- RS232C_8_ICCardTransmit
	- RS232C_8_SAMTransmit
*/
#define	_RS232C_8_TRANSMIT_COMMAND_EXECUTION_FAILED_ERROR			( _ERROR_CODE_ORIGIN + 0x1001 )
#define	_RS232C_8_TRANSMIT_NEGATIVE_REPLY_RECEIVED_ERROR			( _ERROR_CODE_ORIGIN + 0x1002 )
#define	_RS232C_8_TRANSMIT_FAILED_TO_ALLOCATE_MEMORY_REGION_ERROR		( _ERROR_CODE_ORIGIN + 0x1003 )
#define	_RS232C_8_TRANSMIT_ABORT_REQUEST_RECEIVED_ERROR				( _ERROR_CODE_ORIGIN + 0x1004 )
#define	_RS232C_8_TRANSMIT_UNEXPECTED_ERROR					( _ERROR_CODE_ORIGIN + 0x1005 )


/*
	Return codes of the following API(s):
	- RS232C_8_UpdateFirmware
*/
#define	_RS232C_8_UPDATE_FIRMWARE_CONNECT_DEVICE_FAILED_ERROR			( _ERROR_CODE_ORIGIN + 0x2001 )
#define	_RS232C_8_UPDATE_FIRMWARE_DISCONNECT_DEVICE_FAILED_ERROR		( _ERROR_CODE_ORIGIN + 0x2002 )
#define	_RS232C_8_UPDATE_FIRMWARE_UNKNOWN_FILE_TYPE_ERROR			( _ERROR_CODE_ORIGIN + 0x2003 )
#define	_RS232C_8_UPDATE_FIRMWARE_CANNOT_OPEN_FILE_ERROR			( _ERROR_CODE_ORIGIN + 0x2004 )
#define	_RS232C_8_UPDATE_FIRMWARE_FAILED_TO_ALLOCATE_MEMORY_REGION_ERROR	( _ERROR_CODE_ORIGIN + 0x2005 )
#define _RS232C_8_UPDATE_FIRMWARE_CANNOT_READ_FILE_ERROR			( _ERROR_CODE_ORIGIN + 0x2006 )
#define	_RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_FILE_CONTENTS_ERROR		( _ERROR_CODE_ORIGIN + 0x2007 )
#define	_RS232C_8_UPDATE_FIRMWARE_DEVICE_ALREADY_CONNECTED_ERROR		( _ERROR_CODE_ORIGIN + 0x2008 )
#define	_RS232C_8_UPDATE_FIRMWARE_COMMAND_EXECUTION_FAILED_ERROR		( _ERROR_CODE_ORIGIN + 0x2009 )
#define	_RS232C_8_UPDATE_FIRMWARE_NEGATIVE_REPLY_RECEIVED_ERROR			( _ERROR_CODE_ORIGIN + 0x200A )
#define	_RS232C_8_UPDATE_FIRMWARE_IDENTICAL_REVISION_ERROR			( _ERROR_CODE_ORIGIN + 0x200B )
#define	_RS232C_8_UPDATE_FIRMWARE_UNEXPECTED_ERROR				( _ERROR_CODE_ORIGIN + 0x200C )

/*
	Return codes of the following API(s):
	- RS232C_8_GetLibraryRevision

	nothing unique ErrorCode
*/



//----------------------------------------------------------------------
//	Library Function
//----------------------------------------------------------------------

long	RS232C_8_ConnectDevice(
			unsigned long		Port,
			unsigned long 		Baudrate
			);

long	RS232C_8_DisconnectDevice(
			unsigned long		Port
			);

long	RS232C_8_CancelCommand(
			unsigned long		Port
			);

long	RS232C_8_ExecuteCommand(
			unsigned long		Port,
			PCOMMAND		pCommand,
			unsigned long		Timeout,
			PREPLY	 		pReply
			);

long	RS232C_8_ICCardTransmit(
			unsigned long		Port,
			unsigned long	 	DataSizeToSend,
			unsigned char		*pDataToSend,
			unsigned long	 	SizeOfDataToReceive,
			unsigned long		*pSizeOfDataReceived,
			unsigned char		*pDataReceived,
			unsigned long 		*pAdditionalErrorCode,
			PREPLY			pReply
			);

long	RS232C_8_SAMTransmit(
			unsigned long		Port,
			unsigned long		DataSizeToSend,
			unsigned char 		*pDataToSend,
			unsigned long 		SizeOfDataToReceive,
			unsigned long 		*pSizeOfDataReceived,
			unsigned char		*pDataReceived,
			unsigned long 		*pAdditionalErrorCode,
			PREPLY			pReply
			);

long	RS232C_8_UpdateFirmware(
			unsigned long		Port,
			unsigned long		Baudrate,
			char 			*pFileName,
			PUPDATE_INIT_COMM	pUpdate_Init_Comm,
			int			CheckRevision,
			unsigned long 		*pAdditionalErrorCode,
			int			DisplayOn
			);

void	RS232C_8_GetLibraryRevision(
			PLIB_INFORMATION	pLibInfo
			);

int RS232C_8_IsConnected(unsigned long Port);

#ifdef __cplusplus
}
#endif


#endif	// _PROTOCOL_RS232C_8_H_

