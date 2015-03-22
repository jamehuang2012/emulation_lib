/**************************************************************************

	CRC_Proc.h

	Processing for CRC
	Header File

***************************************************************************/

#ifndef	_CRC_PROC_H_
#define	_CRC_PROC_H_


#ifdef __cplusplus
extern "C"
{
#endif



//----------------------------------------------------------------------
//	Define Data type
//----------------------------------------------------------------------

typedef	struct
{
	char	Filename[32];
	char	Revision[32];
}
LIB_INFORMATION_CHECK, *PLIB_INFORMATION_CHECK;



//----------------------------------------------------------------------
//	Library Function
//----------------------------------------------------------------------

unsigned short CrcCcitt(
			unsigned char		*pAddress,
			const unsigned short	Number
			);

unsigned short CrcCcitt2(
			const unsigned short	InitialCrcValue,
			const unsigned char	*pAddress,
			const unsigned short	Number
			);
					
unsigned short CrcCcittOne(
			const unsigned short	Crc,
			const unsigned char	Data
			);

void GetLibraryCheckRevision(
			PLIB_INFORMATION_CHECK	pLibInfo
			);



#ifdef __cplusplus
}
#endif


#endif	// _CRC_PROC_H_


