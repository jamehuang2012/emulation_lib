/*************************************

	FileProc.c

	Logging Library
	Function about File Procedure

**************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <semaphore.h>
#include <time.h>
#include <dirent.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include "FileProc.h"
#include "CollectLogEx.h"


// File Handle
int			g_iHandle;

// Number of Characters per Line
extern int		g_iNumberOfCharactersPerLine;

// Number of Line
extern int		g_iNumberOfLines;

// File Pointer
off_t			g_FilePointer;

// File Pointer
unsigned long		g_ulLine;

// Log File Information
char			g_cInformationData[HEADER_MAX_SIZE];

// Log File Information Length
unsigned long		g_ulInformation;

// Writing Log Date
LOG_DATE		g_LogDate;



int LogFileOpen( char *lpcLogFileName )
{
	int		iFileHandle;
	off_t		offPointer = 0;
	char		cTempChar;
	off_t		FileSize;
	int nRead = 0 ;


	{
		off_t	NumberOfLines = 0;
		off_t	OneLineCharCount;

		// Open Log File
		iFileHandle = open( lpcLogFileName, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO );

		if( ( -1 ) != iFileHandle )
		{
			// Get File Size
			FileSize = lseek( iFileHandle, 0, SEEK_END );

			if( FileSize )
			{
				// Get Number Of Lines
				for( offPointer = 0, OneLineCharCount = 0;
				     offPointer < FileSize;
				     offPointer++, OneLineCharCount++ )
                {

					lseek( iFileHandle, offPointer, SEEK_SET );
					nRead = read( iFileHandle, &cTempChar, 1 );

					if( RETURN_CODE == cTempChar )
					{
						NumberOfLines = FileSize / OneLineCharCount;
						break;
					}
				}

				// Check Number Of Lines
				if( NumberOfLines > g_iNumberOfLines )
				{
					int	iTempFileHandle;
					off_t	WriteSize = OneLineCharCount * g_iNumberOfLines;

					// Temp File Open
					iTempFileHandle = open( TEMP_LOGFILE_NAME, O_CREAT | O_RDWR,
											S_IRWXU | S_IRWXG | S_IRWXO  );

					if( ( -1 ) == iTempFileHandle )
					{
						close( iFileHandle );
						goto _OPEN_FILE;
					}

					// Write Log File Data To Temp File Until g_iNumberOfLines
					for( offPointer = 0; offPointer < WriteSize; offPointer++ )
					{
						lseek( iFileHandle, offPointer, SEEK_SET );
						nRead = read( iFileHandle, &cTempChar, 1 );
						lseek( iTempFileHandle, offPointer, SEEK_SET );
						nRead = write( iTempFileHandle, &cTempChar, 1 );
					}

					close( iFileHandle );
					remove( lpcLogFileName );

					// Open Log File
					iFileHandle = open( lpcLogFileName, O_CREAT | O_RDWR,
									S_IRWXU | S_IRWXG | S_IRWXO  );

					if( ( -1 ) == iFileHandle )
					{
						close( iTempFileHandle );
						goto _OPEN_FILE;
					}

					// Write Temp File Data To Log File
					for( offPointer = 0; offPointer < WriteSize; offPointer++ )
					{
						lseek( iTempFileHandle, offPointer, SEEK_SET );
						nRead = read( iTempFileHandle, &cTempChar, 1 );
						lseek( iFileHandle, offPointer, SEEK_SET );
						nRead = write( iFileHandle, &cTempChar, 1 );
					}

					close( iTempFileHandle );
					remove( TEMP_LOGFILE_NAME );
					close( iFileHandle );
				}
			}
		}
	}


_OPEN_FILE:

	// Open Log File
	iFileHandle = open( lpcLogFileName, O_CREAT | O_RDWR,
						S_IRWXU | S_IRWXG | S_IRWXO  );

	if( ( -1 ) != iFileHandle )
	{
		int	iThrough = 0;
		off_t	NumberOfOneLine = 0;

		// Clear
		g_FilePointer = 0;

		// Get File Size
		FileSize = lseek( iFileHandle, 0, SEEK_END );

		if( FileSize )
		{
			// Search Start Logging Position
			for( offPointer = 0; offPointer < FileSize; offPointer++ )
			{
				lseek( iFileHandle, offPointer, SEEK_SET );
				nRead = read( iFileHandle, &cTempChar, 1 );

				// Check '\n' to Get Number of One Line Character
				if( !iThrough ) {

					if( RETURN_CODE == cTempChar )
					{
						iThrough = 1;

						// Number Of One Line NOT EQUAL Number Of LogConfig.ini
						if( NumberOfOneLine != g_iNumberOfCharactersPerLine )
						{
							// Delete File ( Reset File ) and Remake
							close( iFileHandle );
							remove( lpcLogFileName );

							goto _OPEN_FILE;
						}
					}
					else
					{
						NumberOfOneLine++;
					}
				}

				// Check '~'
				if( START_CHAR == cTempChar )
				{
					// Set Position of '~'
					g_FilePointer = offPointer;
					break;
				}
			}

			// NOT found '~'
			if( ( 0 == g_FilePointer ) && ( iThrough ) )
			{
				// Delete File ( Reset File ) and Remake
				close( iFileHandle );
				remove( lpcLogFileName );

				goto _OPEN_FILE;
			}
		}
	}

	return iFileHandle;
}


void LogFileClose(
	int iLogFileHandle)
{
	// Close Log File
	close( iLogFileHandle );
}


int LogWriteFile(
	int		iLogFileHandle,
	unsigned long	ulDataLength,
	char		*lpcData,
	long		*lDetailError)
{
	int		iRet;
	char		cTimeStamp[64];
	char		*cTempDataBuffer;
	char		*cTempDataIndex;
	unsigned long	ulTempDataLength;


	// Get Time stamps
	{
		struct tm		*lpTimeValue;
		struct timeval		tv;
		struct timezone		tz;
		time_t			tt;
		int			iRet;
		unsigned long		ulYear, ulMonth, ulDate;

		bzero( cTimeStamp, sizeof( cTimeStamp ) );

		// Get Information of Time
		iRet = gettimeofday( &tv, &tz );
		if( ( -1 ) == iRet )
		{
			*lDetailError = errno;
			return FALSE;
		}

		// Get Local Time
		tt = tv.tv_sec;
		lpTimeValue = localtime( &tt );

		// Check Date
		ulYear = ( unsigned long )( lpTimeValue->tm_year + LINUX_YEAR_ORIGIN );
		ulMonth = ( unsigned long )( lpTimeValue->tm_mon + LINUX_MONTH_ORIGIN );
		ulDate = ( unsigned long )( lpTimeValue->tm_mday );

		if( ( ulYear == g_LogDate.ulYear ) &&
		    ( ulMonth == g_LogDate.ulMonth ) &&
		    ( ulDate == g_LogDate.ulDate ) )
        {
			sprintf(
				cTimeStamp,
				"[%02d:%02d:%02d:%03d:%03d] ",
				lpTimeValue->tm_hour,
				lpTimeValue->tm_min,
				lpTimeValue->tm_sec,
				(int)(tv.tv_usec / 1000),
				(int)(tv.tv_usec - ( ( tv.tv_usec / 1000 ) * 1000 ))
				);
		}
		else
		{
			sprintf(
				cTimeStamp,
				"[%.4d/%.2d/%.2d]\n[%02d:%02d:%02d:%03d:%03d] ",
				(int)ulYear,
				(int)ulMonth,
				(int)ulDate,
				lpTimeValue->tm_hour,
				lpTimeValue->tm_min,
				lpTimeValue->tm_sec,
				(int)(tv.tv_usec / 1000),
				(int)(tv.tv_usec - ( ( tv.tv_usec / 1000 ) * 1000 ))
				);

			// Record New Date
			g_LogDate.ulYear = ulYear;
			g_LogDate.ulMonth = ulMonth;
			g_LogDate.ulDate = ulDate;
		}
	}

	// Allocate Write Data Buffer
	{
		cTempDataBuffer = malloc( strlen( cTimeStamp ) + ulDataLength + strlen( RETURN_STRING ) );
		if( !cTempDataBuffer )
		{
			*lDetailError = errno;
			return FALSE;
		}
	}

	// Set Write Data & Length
	{
		// Time Stamps
		cTempDataIndex = cTempDataBuffer;
		strncpy( cTempDataIndex, cTimeStamp, strlen( cTimeStamp ) );
		cTempDataIndex += strlen( cTimeStamp );

		// Data
		strncpy( cTempDataIndex, lpcData, ulDataLength );
		cTempDataIndex += ulDataLength;

		// Return
		strncpy( cTempDataIndex, RETURN_STRING, strlen( RETURN_STRING ) );
		cTempDataIndex += strlen( RETURN_STRING );

		// Length
		ulTempDataLength = cTempDataIndex - cTempDataBuffer;
	}

	// Write Data
	{
		iRet = AjustAndWriteData(
					iLogFileHandle,
					cTempDataBuffer,
					ulTempDataLength,
					lDetailError
					);
	}

	free( cTempDataBuffer );

	return iRet;
}


void MakeLogHeader(
	unsigned long	ulDataLength,
	char		*lpcData,
	char		*lpcHeaderString,
	unsigned long	*ulHeaderStringLength)
{
	char	*lpcTemp = lpcHeaderString;

	// Copy Header Informations
	{
		strncpy( g_cInformationData, lpcData, ulDataLength );
		g_ulInformation = ulDataLength;
	}

	// Revision
	{
		*lpcHeaderString = SEPARATOR_CHAR;
		lpcHeaderString += 1;

		strncpy( lpcHeaderString, RETURN_STRING, strlen( RETURN_STRING ) );
		lpcHeaderString += strlen( RETURN_STRING );

		strncpy( lpcHeaderString, lpcData, ulDataLength );
		lpcHeaderString += ulDataLength;

		strncpy( lpcHeaderString, "(", strlen("(") );
		lpcHeaderString += strlen( "(" );

		strncpy( lpcHeaderString, COLLECTLOGEX_NAME, strlen( COLLECTLOGEX_NAME ) );
		lpcHeaderString += strlen( COLLECTLOGEX_NAME );

		strncpy( lpcHeaderString, "  ", strlen( "  " ) );
		lpcHeaderString += strlen( "  " );

		strncpy( lpcHeaderString, REVISION_NAME, strlen( REVISION_NAME ) );
		lpcHeaderString += strlen( REVISION_NAME );

		strncpy( lpcHeaderString, " ", strlen( " " ) );
		lpcHeaderString += strlen( " " );

		strncpy( lpcHeaderString, COLLECTLOGEX_LIBRARY_REVISION, strlen( COLLECTLOGEX_LIBRARY_REVISION ) );
		lpcHeaderString += strlen( COLLECTLOGEX_LIBRARY_REVISION );

		strncpy( lpcHeaderString, ")" , strlen( ")" ));
		lpcHeaderString += strlen( ")" );

		strncpy( lpcHeaderString, RETURN_STRING, strlen( RETURN_STRING ) );
		lpcHeaderString += strlen( RETURN_STRING );

		*lpcHeaderString = SEPARATOR_CHAR;
		lpcHeaderString += 1;

		strncpy( lpcHeaderString, RETURN_STRING, strlen( RETURN_STRING ) );
		lpcHeaderString += strlen( RETURN_STRING );
	}

	// Date
	{
		int			iRet;
		struct tm		*lpTimeValue;
		struct timeval		tv;
		struct timezone		tz;
		time_t			tt;
		char			cDate[16];
		unsigned long		ulYear, ulMonth, ulDate;


		// Get Information of Time
		iRet = gettimeofday( &tv, &tz );
		if( ( -1 ) == iRet )
		{
			return;
		}

		// Get Local Time
		tt = tv.tv_sec;
		lpTimeValue = localtime( &tt );

		bzero( cDate, sizeof( cDate ) );

		// Create Date String
		ulYear = ( unsigned long )( lpTimeValue->tm_year + LINUX_YEAR_ORIGIN );
		ulMonth = ( unsigned long )( lpTimeValue->tm_mon + LINUX_MONTH_ORIGIN );
		ulDate = ( unsigned long )( lpTimeValue->tm_mday );
		sprintf( cDate, "[%.4d/%.2d/%.2d]", (int)ulYear, (int)ulMonth, (int)ulDate );

		strncpy( lpcHeaderString, cDate, strlen( cDate ) );
		lpcHeaderString += strlen( cDate );

		// Record Date
		g_LogDate.ulYear = ulYear;
		g_LogDate.ulMonth = ulMonth;
		g_LogDate.ulDate = ulDate;
	}

	strncpy( lpcHeaderString, RETURN_STRING, strlen( RETURN_STRING ) );
	lpcHeaderString += strlen( RETURN_STRING );

	*ulHeaderStringLength = lpcHeaderString - lpcTemp;

	return;
}


int LogHeaderWrite(
	int		iLogFileHandle,
	char		*lpcHeaderString,
	unsigned long	ulHeaderStringLength,
	long		*lDetailError)
{
	int	iRet;


	iRet = AjustAndWriteData(
				iLogFileHandle,
				lpcHeaderString,
				ulHeaderStringLength,
				lDetailError
				);

	return iRet;
}


int AjustAndWriteData(
	int		iLogFileHandle,
	char		*cData,
	unsigned long	ulDataLength,
	long		*lDetailError)
{
	unsigned long	ulCounter = 0;
	char		cTempDataBuffer[ ( MAX_NUMBER_OF_CHAR_PER_LINE + 1 ) * 2 ]; // for '~'
	unsigned long	ulTempWriteDataLength = 0;
	unsigned long	ulTempOneLineDataLength;
	char		*lpcNextOneLineDataIndex = cData;
	char		*lpcWriteDataIndex;
	unsigned long	ulBlankLength;
	char		cHead;
	int		iRet = 0;


	while( ulCounter < ulDataLength )
	{
		// Clear Value
		bzero( cTempDataBuffer, sizeof( cTempDataBuffer ) );
		lpcWriteDataIndex = cTempDataBuffer;
		ulTempWriteDataLength = 0;

		// Copy One Line Data Length
		ulTempOneLineDataLength =
			strstr( lpcNextOneLineDataIndex, RETURN_STRING ) - lpcNextOneLineDataIndex;

		// Check One Line Length
		if( ulTempOneLineDataLength > (unsigned long)g_iNumberOfCharactersPerLine )
		{
			// Set Max One Line Data Length
			ulTempOneLineDataLength = g_iNumberOfCharactersPerLine;
		}

		// Copy One Line Data
		strncpy( lpcWriteDataIndex, lpcNextOneLineDataIndex, ulTempOneLineDataLength );

		// Copy Head Character
		cHead = *lpcWriteDataIndex;

		// Next Copy Data Index + RETURN
		lpcNextOneLineDataIndex += ( ( ulTempOneLineDataLength + strlen( RETURN_STRING ) ) );

		// Write Data Index
		lpcWriteDataIndex += ulTempOneLineDataLength;

		// Write Data Length
		ulTempWriteDataLength += ulTempOneLineDataLength;

		// Filled Blank Length
		ulBlankLength = g_iNumberOfCharactersPerLine - ulTempOneLineDataLength;

		// Padding Data
		{
			// Check Header
			if( SEPARATOR_CHAR != cHead )
			{
				cHead = BLANK_CHAR;
			}

			// Fill Blank
			memset( lpcWriteDataIndex, cHead, ulBlankLength );
			lpcWriteDataIndex += ulBlankLength;
			ulTempWriteDataLength += ulBlankLength;

			// Write Return
			strncpy( lpcWriteDataIndex, RETURN_STRING, strlen( RETURN_STRING ) );
			lpcWriteDataIndex += strlen( RETURN_STRING );

			// Write Data Return Length
			ulTempWriteDataLength += strlen( RETURN_STRING );
		}

		// Write to Log File
		iRet = WriteDataExe(
				iLogFileHandle,
				cTempDataBuffer,
				ulTempWriteDataLength,
				FALSE,
				lDetailError
				);

		// Write "~~~~"
		Write_START_CHAR( iLogFileHandle, lDetailError );

		ulCounter += ( ulTempOneLineDataLength + 1 );
	}

	return iRet;
}


int Write_START_CHAR(
	int	iLogFileHandle,
	long	*lDetailError)
{
	char		cTempDataBuffer[ ( MAX_NUMBER_OF_CHAR_PER_LINE + 1 ) * 2 ]; // for '~'
	unsigned long	ulTempWriteDataLength = 0;
	char		*lpcWriteDataIndex;
	int		iRet;


	// Clear Value
	bzero( cTempDataBuffer, sizeof( cTempDataBuffer ) );
	lpcWriteDataIndex = cTempDataBuffer;
	ulTempWriteDataLength = 0;

	// Set "~~~~"
	{
		// Fill '~'
		memset( lpcWriteDataIndex, START_CHAR, g_iNumberOfCharactersPerLine );
		lpcWriteDataIndex += g_iNumberOfCharactersPerLine;
		ulTempWriteDataLength += g_iNumberOfCharactersPerLine;

		// Write Return
		strncpy( lpcWriteDataIndex, RETURN_STRING, strlen( RETURN_STRING ) );

		// Write Data Return Length
		ulTempWriteDataLength += strlen( RETURN_STRING );
	}

	// Write to Log File
	iRet = WriteDataExe(
			iLogFileHandle,
			cTempDataBuffer,
			ulTempWriteDataLength,
			TRUE,
			lDetailError
			);

	return iRet;
}


int WriteDataExe(
	int		iLogFileHandle,
	char		*lpcData,
	unsigned long	ulDataLength,
	int		iStartChar,
	long		*lDetailError)
{
	int		iRet;
//	unsigned long	ulNumberOfBytesWritten;


	// Set File Pointer
	lseek( iLogFileHandle, g_FilePointer, SEEK_SET );

	if( write( iLogFileHandle, lpcData, ulDataLength ) < 0 )
	{
		*lDetailError = errno;
		return FALSE;
	}

	// NOT START_CHAR
	if( !iStartChar ) {

		// Reset File Pointer
		g_FilePointer += ulDataLength;
	}

	// Over Number of Line
	if( ( g_FilePointer / g_iNumberOfCharactersPerLine ) >= g_iNumberOfLines )
	{
		g_FilePointer = 0;

		// Write Header
		{
			char		cTempHeader[HEADER_MAX_SIZE];
			unsigned long	ulHeaderLength = 0;

			// Create Header Information of Log File
			bzero( cTempHeader, sizeof( cTempHeader ) );
			MakeLogHeader(
				g_ulInformation,
				g_cInformationData,
				cTempHeader,
				&ulHeaderLength
				);

			// Write Header Information of Log File
			iRet = LogHeaderWrite(
					iLogFileHandle,
					cTempHeader,
					ulHeaderLength,
					lDetailError
					);

			if( !iRet )
			{
				*lDetailError = errno;
				return FALSE;
			}
		}
	}

	return TRUE;
}


void SetLogFileAttribute(
	char	*cLogFileName,
	long	lAttribute)
{
	chmod( cLogFileName, lAttribute );
}

