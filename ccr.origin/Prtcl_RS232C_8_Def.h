/**************************************************************************

	Prtcl_RS232C_8_Def.h

	RS-232C 8bit Protocol Card Reader Library
	Inside Header File

***************************************************************************/

#ifndef	_PROTOCOL_RS232C_8_DEF_H_
#define	_PROTOCOL_RS232C_8_DEF_H_


#ifdef __cplusplus
extern "C"
{
#endif



// Data Type Definition

#define	STX				0xF2
#define	ACK				0x06
#define	NAK				0x15
#define	EOT				0x04
#define	DLE				0x10
#define	TRUE				(1)
#define	FALSE				(0)

#define	MAX_PORT_NO			(256)

#define S_HEADER_LEN			(3)		// Cxx
#define R_HEADER_LEN			(5)		// Pxxxx
#define HEADER_SPACE			(25)

#define PROC_NO_DATA			(0)
#define PROC_EXIST_DATA			(1)


enum MESSAGE_TAG
{
	COMMAND_TAG_LARGE = 'C',
	POSITIVE_RESPONSE_TAG_LARGE = 'P',
	NEGATIVE_RESPONSE_TAG_LARGE = 'N',
	COMMAND_TAG_SMALL = 'c',
	POSITIVE_RESPONSE_TAG_SMALL = 'p',
	NEGATIVE_RESPONSE_TAG_SMALL = 'n',
};


#define	DWL_TYPE			(1)
#define	BIN_TYPE			(2)
#define	DAT_TYPE			(3)

#define	DEFAULT_ACK_TIMEOUT		(2000)
#define	DEFAULT_REPLY_TIMEOUT		(20000)
#define	DEFAULT_DLE_EOT_TIMEOUT		(5000)
#define	DEFAULT_DELAY_TIME		(5)

#define	DEFAULT_COMMAND_RETRY		(3)
#define	DEFAULT_NAK_RETRY		(3)
#define	DEFAULT_RECEIVE_RETRY		(3)

#define	ICCARD_TRANSMIT_FLAG		(0x30)
#define	SAM_TRANSMIT_FLAG		(0x40)

#define	SUFFIX_OF_FILE_REVISION		"Rev"
#define	LENGTH_OF_REVISION		(8)


typedef	struct
{
	// Port Handle
	int			Handle;

	// Cancel Received Flag
	int			CancelReceiveFlag;

	// CriticalSection for Connect
	sem_t			sem_Connect;

	// CriticalSection for CommadExecute
	sem_t			sem_CommandExecute;

	// CriticalSection for Cancel
	sem_t			sem_CommandCancel;

	// Connect Flag
	int			ConnectFlag;

	// Cnacel Processing Flag
	int			CancelFlag;

	// Send Data Size
	unsigned long		DataSizeToSend;

	// Send Data Area
	char			*pDataStrage;

	// Data Set Index
	char			*pIndex;

	// Previous ParamterCode
	unsigned char		LastTimeParameterCode;

	// Long Data or Not
	int			LongData;
}
DEVICE_ACCESS_INFORMATION, *PDEVICE_ACCESS_INFORMATION;



#ifdef __cplusplus
}
#endif


#endif	// _PROTOCOL_RS232C_8_DEF_H_

