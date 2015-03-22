/**************************************************************************

	RS232C_Interface.h

	RS-232C Card Reader Interface Library
	Header File

***************************************************************************/

#ifndef	_RS232C_INTERFACE_H_
#define	_RS232C_INTERFACE_H_


#ifdef __cplusplus
extern "C"
{
#endif



//----------------------------------------------------------------------
//	Define Data type
//----------------------------------------------------------------------

#define	_RS232C_MAX_TRANSFER_DATA_SIZE		(1024)

#define	_RS232C_DEFAULT_CHAR_TIMEOUT		(300)
#define	_RS232C_DEFAULT_DATA_RECEIVE_TIME	(2000000)

#define	PORT_NAME				"/dev/ttyS"
#define	_RS232C_NO_ERROR			(0)
#define	_RS232C_ERROR_DATA_SEND			(-1)
#define	_RS232C_ERROR_DATA_RECEIVE		(-2)
#define	_RS232C_ERROR_TIMEOUT			(-3)
#define	_RS232C_ERROR_CHARACTER_TIMEOUT		(-4)



// Data structure for Library Revisiontypedef	struct
{
	char	Filename[32];
	char	Revision[32];
}
LIB_INFORMATION_RS232C_INTERFACE, *PLIB_INFORMATION_RS232C_INTERFACE;



//----------------------------------------------------------------------
//	Library Function
//----------------------------------------------------------------------

int RS232C_SetBaseDevName( char *BaseDevName);

int RS232C_PortOpen(
	unsigned long		Port,
	unsigned long 		Baudrate,
	int			Bit
);

void RS232C_PortClose(
	int			Handle
);

void RS232C_PortFlush(
	int			Handle
);

long RS232C_SendData(
	int			Handle,
	char			*pSendData,
	unsigned long		SizeOfSendData
);

long RS232C_ReceiveData(
	int			Handle,
	char			*pReceiveData,
	unsigned long		SizeOfReceiveBuffer,
	unsigned long		*pSizeOfDataReceived,
	int			Timeout
);

void RS232C_Interface_GetLibraryRevision(
	PLIB_INFORMATION_RS232C_INTERFACE	pLibInfo
);



#ifdef __cplusplus
}
#endif


#endif	// _RS232C_INTERFACE_H_


