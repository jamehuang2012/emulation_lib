/**************************************************************************

	Prtcl_RS232C_FuncDef.h

	RS-232C 8bit Protocol Card Reader Library Inside Function
	Header File

***************************************************************************/

#ifndef	_PROTOCOL_RS232C_FUNC_DEF_H_
#define	_PROTOCOL_RS232C_FUNC_DEF_H_


#ifdef __cplusplus
extern "C"
{
#endif



// Define Inside Function

long RS232C_Init( void );

void RS232C_End(
	unsigned long		Port
);

long _TransmitControl(
	unsigned long		Port,
	unsigned long		DataSizeToSend,
	unsigned char		*pDataToSend,
	unsigned long		SizeOfDataToReceive,
	unsigned long 		*pSizeOfDataReceived,
	unsigned char		*pDataReceived,
	unsigned long 		*pAdditionalErrorCode,
	PREPLY			pReply,
	unsigned char		ICCard_SAM
);

long TransmitControl(
	unsigned long		Port,
	int			Init,
	unsigned long		DataSizeToSend,
	unsigned char		*pDataToSend,
	int			*pComplete,
	unsigned long		*pAdditionalErrorCode,
	PREPLY			pReply,
	unsigned char		ICCard_SAM
);

long DownloadGetFileInfo(
	char			*pFileName,
	int			*iFileType,
	unsigned long		*pFileSize	
);

long DownloadGetFileData(
	char			*pFileName,
	unsigned long		*pTotalNumberOfLines,
	char			*pDataBuffer
);

long DownloadExe(
	unsigned long		Port,
	unsigned long		Baudrate,
	PUPDATE_INIT_COMM	pUpdate_Init_Comm,
	int			CheckRevision,
	unsigned long		*pAdditionalErrorCode,
	int			DisplayOn,
	char			*pFileName
);

long AreaChange(
	unsigned long		Port,
	unsigned long		Baudrate,
	unsigned long		*pAdditionalErrorCode
);

long ErrorExchange(
	long			Error
);

void MessageToLog(
	unsigned long		Port,
	char			*pMessage,
	unsigned long		Data,
	long			Ret
);

void DataMessageToLog(
	unsigned long		Port,
	char			*pCommand_Data,
	unsigned long		Command_Data_Length,
	unsigned long		HeaderLength
);



#ifdef __cplusplus
}
#endif


#endif	// _PROTOCOL_RS232C_FUNC_DEF_H_

