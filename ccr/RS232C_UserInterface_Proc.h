/**************************************************************************

	RS232C_UserInterface_Proc.h

	RS232C User Interface Processing Library
	Header File

***************************************************************************/

#ifndef	_RS232C_USER_INTERFACE_PROC_H_
#define	_RS232C_USER_INTERFACE_PROC_H_


#include "RS232C_Def.h"


// Define Data Type

#define	_HEX		0x00
#define	_ASCII		0x01


// Define Function

unsigned long	Select_COM_Port( void );
			
unsigned long	Select_Baudrate( void );

void		DisplayData(
			char		Type,
			unsigned char	*Data,
			unsigned long	DataSize
			);

int		Select_Yes_or_No( void );
				
void		ShowReplyType( REPLY_TYPE );

void		MakeBinaryData(
			unsigned char	*OriginalData,
			unsigned long	OriginalDataSize,
			unsigned char	*ConvertedData,
			unsigned long	*ConertedDataSize
			);

#endif	// _RS232C_USER_INTERFACE_PROC_H_

