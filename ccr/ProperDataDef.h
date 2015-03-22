/**************************************************************************

	ProperDataDef.h

	Data Definition Library Proper to 8bit Protocol Card Reader
	Header File

***************************************************************************/

#ifndef	_PROPER_DATA_DEF_H_
#define	_PROPER_DATA_DEF_H_


#ifdef __cplusplus
extern "C"
{
#endif


// Define Function

unsigned long	GetMax_IC_SAM_DataLength( void );

void		GetLibraryRevision( char *Revision );

char		GetFirstParameterCommandFormatData( void );



#ifdef __cplusplus
}
#endif


#endif	// _PROPER_DATA_DEF_H_

