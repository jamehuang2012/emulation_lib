/**************************************************************************

	ProperDataDef.c

	Data Definition Library Proper to 8bit Protocol
	Dip Type Card Reader

***************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ProperDataDef.h"



// Dip Type Card Reader Revision
#define	RS232C_8_LIBRARY_REVISION			"3907-01A"


// Dip Type Card Reader IC & SAM Transmit Data Max Length
#define	MAX_IC_AND_SAM_TRANSMIT_SIZE_DIP		(300)


// First Parameter of Command Format Data 
#define	FIRST_PARAM_OF_COMMAND_FORMAT_DATA		'C'



unsigned long GetMax_IC_SAM_DataLength( void )
{
	return	MAX_IC_AND_SAM_TRANSMIT_SIZE_DIP;
}


void GetLibraryRevision( char *Revision )
{
	memcpy(
		Revision,
		RS232C_8_LIBRARY_REVISION,
		sizeof( RS232C_8_LIBRARY_REVISION )
		);

	return;
}


char GetFirstParameterCommandFormatData( void )
{
	return	FIRST_PARAM_OF_COMMAND_FORMAT_DATA;
}


