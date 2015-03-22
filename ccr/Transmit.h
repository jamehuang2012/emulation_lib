/**************************************************************************

	Transmit.h

	RS-232C 8bit Protocol Card Reader
	Transmit Header File

***************************************************************************/

#ifndef	_TRANSMIT_H_
#define	_TRANSMIT_H_


#define	ICCARD		(1)
#define	SAM		(2)

// Define Function

void Transmit(
	unsigned long	Port,
	unsigned long	Baudrate,
	unsigned char	Flag
	);


#endif	// _TRANSMIT_H_

