/**************************************************************************

	DLE_EOT.c

	RS-232C 8bit Protocol Card Reader
	DLE + EOT

***************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "DLE_EOT.h"



int DLE_EOT(
	unsigned long	Port,
	unsigned long	Baudrate)
{
	long	Ret;
Baudrate = 1;
	Ret = RS232C_8_CancelCommand( Port );

	return (int)Ret;
}


