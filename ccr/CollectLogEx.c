/*************************************

	CollectLogEx.c

	Collect Log Library

**************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "CollectLogEx.h"
#include "CollectLogExDef.h"

#include "FileProc.h"

void LogFileClose(int iLogFileHandle);

// LogCollect
int			g_iLogCollect = TRUE;

// Log File Name
char			g_cLogFileName[BUFFER_SIZE_FOR_FILE_PROC];

// Log File Folder Name
char			g_cLogFileFolder[BUFFER_SIZE_FOR_FILE_PROC];

// Number of Characters per Line
int			g_iNumberOfCharactersPerLine = DEFAULT_NUMBER_OF_CHAR_PER_LINE;

// Number of Lines
int			g_iNumberOfLines = DEFAULT_NUMBER_OF_LINES;

// Log File Header Information
char			g_cHeaderString[HEADER_MAX_SIZE];

// Log File Header Information Length
unsigned long		g_ulHeaderStringLength;

// Log File Handle
unsigned long		g_iLogFileHandle;

// Detail Error Code
long			g_ulErrorDetailCode;

// Open Flag
int			g_iOpen;

// Semaphore ID
int			g_iSemID = (-1);



long CollectLogExOpen(
	unsigned long	ulInformation,
	char 		*lpcInformationData,
	long		*lDetailError
)
{
	long	lRet = LOG_SUCCESS;
	int	iRet;


	g_iLogCollect = TRUE;
	*lDetailError = 0;

	// Get Configrations
	lRet = GetConfigration( lDetailError );

	// Init Proc
	lRet = LogInit( lDetailError );
	if( LOG_SUCCESS != lRet ) {
		goto _OPEN_EXIT;	}

	// Check Logging or Not
	if( !g_iLogCollect ) {
		lRet = LOG_NO_COLLECT;
		goto _OPEN_EXIT;	}

	// Create Header Information of Log File
	bzero( g_cHeaderString, sizeof( g_cHeaderString ) );
	MakeLogHeader(
		ulInformation,
		lpcInformationData,
		g_cHeaderString,
		&g_ulHeaderStringLength
		);

	// Write Header Information of Log File
	iRet = LogHeaderWrite(
			g_iLogFileHandle,
			g_cHeaderString,
			g_ulHeaderStringLength,
			lDetailError
			);

	if( !iRet ) {
		lRet = LOG_ERR_WRITE_FILE;
		goto _OPEN_EXIT;
	}

	// Open Flag ON
	g_iOpen = TRUE;

_OPEN_EXIT:

	return lRet;
}


long CollectLogExClose(
	long	*lDetailError
)
{
	long		lRet = LOG_SUCCESS;
//	int		iRet;
	union semun	semu;


	lDetailError = 0;

	// Check Open Flag
	if( !g_iOpen ) {
		lRet = LOG_ERR_NOT_CALL_OPEN;
		return lRet;
	}

	// Open Flag OFF
	g_iOpen = FALSE;

//_CLOSE_ERR:

	// Using File Close
	LogFileClose( g_iLogFileHandle );
	// Set File Attribute
	SetLogFileAttribute( g_cLogFileName, S_IRUSR | S_IRGRP | S_IROTH );

	// End Semaphore	if( -1 != g_iSemID ) {
		semctl( g_iSemID, 0, IPC_RMID, semu );
	}

	return lRet;
}


long CollectLogExWrite(
	unsigned long	ulLogMessageLength,
	char 		*lpLogMessage,
	long		*lDetailError
)
{
	long		lRet = LOG_SUCCESS;
	int		iRet;
	struct sembuf	Sembuf;


	// Check NOT Collect Log
	if( !g_iLogCollect ) {
		*lDetailError = 0;
		return LOG_NO_COLLECT;
	}

	lDetailError = 0;

	Sembuf.sem_num = 0;
	Sembuf.sem_flg = SEM_UNDO;

	// In Semaphore
	Sembuf.sem_op = -1;
	if( ( -1 ) == semop( g_iSemID, &Sembuf, 1 ) ) {
		*lDetailError = errno;
		return LOG_ERR_INTERNAL_FUNCTION;
	}

	// Check Open Flag
	if( !g_iOpen ) {
		lRet = LOG_ERR_NOT_CALL_OPEN;
		goto _WRITE_END;
	}

	// Write Data
	iRet = LogWriteFile(
			g_iLogFileHandle,
			ulLogMessageLength,
			lpLogMessage,
			lDetailError
			);

	if( !iRet ) {
		lRet = LOG_ERR_WRITE_FILE;
		goto _WRITE_END;
	}

_WRITE_END:

	// Out Semaphore
	Sembuf.sem_op = 1;
	if( ( -1 ) == semop( g_iSemID, &Sembuf, 1 ) ) {
		*lDetailError = errno;
		lRet = LOG_ERR_INTERNAL_FUNCTION;
	}

	return lRet;}


void CollectLogExGetRevision(
	LPLIB_INFORMATION_COLLECT_LOG_EX	lpLibInfo
)
{
	// Clear Buffer
	bzero( lpLibInfo, sizeof( LPLIB_INFORMATION_COLLECT_LOG_EX ) );

	// Set Filename
	memcpy(
		lpLibInfo->cFilename,
		COLLECTLOGEX_LIBRARY_FILENAME,
		strlen( COLLECTLOGEX_LIBRARY_FILENAME )
		);

	// Set Revison
	memcpy(
		lpLibInfo->cRevision,
		COLLECTLOGEX_LIBRARY_REVISION,
		strlen(COLLECTLOGEX_LIBRARY_REVISION )
		);

	return;
}


long GetConfigration(
	long	*lErrorDetailCode
)
{
	long		lRet = LOG_SUCCESS;
	int		iFileHandle;
//	int		iTempChar;
	off_t		FileSize;
	size_t		ReadSize;
	char		cTempBuffer[256];
////	int		iLine = 0, iCounter;
//	char		*lpIndex;
	char		*lpTemp;
	char		cNumberOfCharactersPerLine;
	char		cNumberOfLines;
	unsigned char	ucLength;


	// Open Configration File
	{
//	    printf("Open log config: %s\n", CONFIG_FILE);
		iFileHandle = open( CONFIG_FILE, O_RDONLY );

		if( ( -1 ) == iFileHandle ) {
			*lErrorDetailCode = errno;
			g_iLogCollect = FALSE;
			return LOG_ERR_FILE_OPEN;
		}
	}

	// Get File Size
	{
		FileSize = lseek( iFileHandle, 0, SEEK_END );

		if( !FileSize ) {
			*lErrorDetailCode = errno;
			g_iLogCollect = FALSE;
			lRet = LOG_ERR_FILE_OPEN;
			goto _EXIT;
		}
	}
	// Get File Data
	{
		// Buffer Clear
		bzero( cTempBuffer, sizeof( cTempBuffer ) );

		// Read File
		lseek( iFileHandle, 0, SEEK_SET );
		ReadSize = read( iFileHandle, cTempBuffer, FileSize );

		if( ( (size_t)-1 ) == ReadSize ) {
			*lErrorDetailCode = errno;
			g_iLogCollect = FALSE;
			lRet = LOG_ERR_FILE_OPEN;
			goto _EXIT;
		}
	}

	// Get Each Item
	{
		// Set Log File Name
		{
			lpTemp = strstr( cTempBuffer, PARAM_LOG_FILE_NAME );

			if( NULL != lpTemp ) {

				// Buffer Clear
				bzero( g_cLogFileName, sizeof( g_cLogFileName ) );

				// Get Filename Length
				ucLength = 0;
				do {
					ucLength++;
				} while( *( lpTemp + ucLength ) != RETURN_CODE );

				strncpy(
					g_cLogFileName,
					lpTemp + strlen( PARAM_LOG_FILE_NAME ),
					ucLength - strlen( PARAM_LOG_FILE_NAME )
					);
			}
			else {
				// Set Default
				strcpy( g_cLogFileName, DEFAULT_LOG_FILE_NAME );
			}
		}

		// Set Log File Folder
		{
			lpTemp = strstr( cTempBuffer, PARAM_LOG_FILE_FOLDER );

			if( NULL != lpTemp ) {

				// Buffer Clear
				bzero( g_cLogFileFolder, sizeof( g_cLogFileFolder ) );

				// Get Filefolder Length
				ucLength = 0;
				do {
					ucLength++;
				} while( *( lpTemp + ucLength ) != RETURN_CODE );

				strncpy(
					g_cLogFileFolder,
					lpTemp + strlen( PARAM_LOG_FILE_FOLDER ),
					ucLength - strlen( PARAM_LOG_FILE_FOLDER )
					);
			}
			else {
				// Set Default
				strcpy( g_cLogFileFolder, DEFAULT_LOG_FILE_FOLDER );
			}
		}

		// Set Number of Characters per Line
		{
			lpTemp = strstr( cTempBuffer, PARAM_MAX_NUMBER_OF_CHAR_PER_LINE );

			if( NULL != lpTemp ) {

				strcpy(
					&cNumberOfCharactersPerLine,
					lpTemp + strlen( PARAM_MAX_NUMBER_OF_CHAR_PER_LINE )
					);
				g_iNumberOfCharactersPerLine = atoi( &cNumberOfCharactersPerLine );

				// Check Value
				if( ( g_iNumberOfCharactersPerLine < MIN_NUMBER_OF_CHAR_PER_LINE ) ||
				    ( MAX_NUMBER_OF_CHAR_PER_LINE < g_iNumberOfCharactersPerLine ) ) {

					// Set Default
					g_iNumberOfCharactersPerLine = DEFAULT_NUMBER_OF_CHAR_PER_LINE;
					*lErrorDetailCode = 0;
					lRet = LOG_ERR_INVALID_PARAMETER;
				}
			}
			else {
				// Set Default
				g_iNumberOfCharactersPerLine = DEFAULT_NUMBER_OF_CHAR_PER_LINE;
			}
		}

		// Set Number of Lines
		{
			lpTemp = strstr( cTempBuffer, PARAM_MAX_NUMBER_OF_LINES );

			if( NULL != lpTemp ) {

				strcpy( &cNumberOfLines, lpTemp + strlen( PARAM_MAX_NUMBER_OF_LINES ) );
				g_iNumberOfLines = atoi( &cNumberOfLines );

				// Check Value

				// Zero = Infinite ( 0xFFFFFFFF )
				if( 0 == g_iNumberOfLines ) {
					g_iNumberOfLines = 0xFFFFFFFF;
				}

				else if( ( g_iNumberOfLines < MIN_NUMBER_OF_LINES ) ||
					 ( MAX_NUMBER_OF_LINES < g_iNumberOfLines ) ) {

					// Set Default
					g_iNumberOfLines = DEFAULT_NUMBER_OF_LINES;
					*lErrorDetailCode = 0;
					lRet = LOG_ERR_INVALID_PARAMETER;
				}
			}
			else {
				// Set Default
				g_iNumberOfLines = DEFAULT_NUMBER_OF_LINES;
			}
		}
	}

_EXIT:

	// Close File
	close( iFileHandle );

	return lRet;
}


long LogInit(
	long	*lErrorDetailCode
)
{
	long		lRet = LOG_SUCCESS;
	union semun	semu;
	char		cFullFileName[BUFFER_SIZE_FOR_FILE_PROC];
	char		cCurrentFolder[BUFFER_SIZE_FOR_FILE_PROC];


	g_iLogCollect = FALSE;

	// Init Semaphore
	{
		g_iSemID = semget( ( key_t )2709, 2, 0666 | IPC_CREAT );

		if( ( -1 ) == g_iSemID ) {
			*lErrorDetailCode = errno;
			lRet = LOG_ERR_INTERNAL_FUNCTION;
			goto _EXIT;
		}

		semu.val = 1;

		if( ( -1 ) == semctl( g_iSemID, 0, SETVAL, semu ) ) {
			*lErrorDetailCode = errno;
			lRet = LOG_ERR_INTERNAL_FUNCTION;
			goto _EXIT;
		}
	}

_CURRENT_RETRY:

	// Set Log File Name with Path
	{
		if( !strcmp( g_cLogFileFolder, DEFAULT_LOG_FILE_FOLDER ) ) {

			// Buffer Clear
			bzero( cFullFileName, sizeof( cFullFileName ) );

			// Get Current Folder
			if( !getcwd( cCurrentFolder, sizeof( cCurrentFolder ) ) ) {
				*lErrorDetailCode = errno;
				lRet = LOG_ERR_INTERNAL_FUNCTION;
				goto _EXIT;
			}

			// Set Current Folder Name
			strcpy( cFullFileName, cCurrentFolder );
		}

		else {

			// Set assigned Folder Name
			strcpy( cFullFileName, g_cLogFileFolder );
		}

		// Append Log File Name
		strcat( cFullFileName, "/" );
		strcat( cFullFileName, g_cLogFileName );
	}

	// Set File Attribute
	SetLogFileAttribute( g_cLogFileName, S_IWUSR | S_IWGRP | S_IWOTH );

	// Open Log File
	g_iLogFileHandle = LogFileOpen( cFullFileName );

	// Open Check
	if( ( (size_t)-1 ) == g_iLogFileHandle ) {

		// NOT Default Current
		if( strcmp( g_cLogFileFolder, DEFAULT_LOG_FILE_FOLDER ) ) {

			// Reset Current Folder and Retry
			bzero( g_cLogFileFolder, sizeof( g_cLogFileFolder ) );
			strcpy( g_cLogFileFolder, DEFAULT_LOG_FILE_FOLDER );
			goto	_CURRENT_RETRY;
		}

		*lErrorDetailCode = errno;
		lRet = LOG_ERR_FILE_OPEN;
		goto _EXIT;
	}

	g_iLogCollect = TRUE;

_EXIT:

	return lRet;
}

