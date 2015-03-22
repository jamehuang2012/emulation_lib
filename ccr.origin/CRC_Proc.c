/**************************************************************************

	CRC_Proc.c

	Processing for CRC Library

***************************************************************************/

#include <stdio.h>
#include <string.h>

#include "CRC_Proc.h"


#define	CRC_LIBRARY_FILENAME	"CRC_Proc"
#define	CRC_LIBRARY_REVISION	"3201-02A"

#define	POLYNOMIAL		0x1021		/* Polynomial X16+X12+X5+1 */



unsigned short _calc_crc(
	unsigned short	crc,
	unsigned short	ch
)
{
	unsigned short	i;

	ch <<= 8;
	for( i = 8; i > 0; i-- ) {
		if( ( ch ^ crc ) & 0x8000 ) {
			crc = ( crc << 1 )^POLYNOMIAL;
		}
		else {
			crc <<= 1;
		}
		ch <<= 1;
	}

	return	crc;
}



unsigned short _CrcCcitt(
	const unsigned short	InitialCrcValue,
	const unsigned char*	pAddress,
	const unsigned short	Number
)
{
	unsigned short	Crc = InitialCrcValue;
	unsigned short	i;
	unsigned char	ch;

	for( i = 0; i < Number; i++ ) {
		ch = *pAddress++;
		Crc = _calc_crc( Crc, ( unsigned short )ch );
	}

	return	Crc;
}



unsigned short CrcCcitt(
	unsigned char*		pAddress,		//start address
	const unsigned short	Number			//number of bytes
)
{
	return	_CrcCcitt( 0x0000, pAddress, Number );
}



void GetLibraryCheckRevision(
	PLIB_INFORMATION_CHECK	pLibInfo
)
{
	// Clear Buffer
	bzero( pLibInfo, sizeof( LIB_INFORMATION_CHECK ) );

	// Set Filename
	memcpy( pLibInfo->Filename, CRC_LIBRARY_FILENAME, sizeof( CRC_LIBRARY_FILENAME ) );

	// Set Revison
	memcpy( pLibInfo->Revision, CRC_LIBRARY_REVISION, sizeof( CRC_LIBRARY_REVISION ) );

	return;
}


