/*************************************

	FileProc.h

	CollectLogEx Library Inside Header File

**************************************/

#ifndef	_FILE_PROC_H_
#define	_FILE_PROC_H_

#ifdef __cplusplus
extern "C"
{
#endif


// Data Definition

#define	BUFFER_SIZE_FOR_FILE_PROC	(256)

#define	COLLECTLOGEX_NAME		"CollectLogEx.a"
#define	REVISION_NAME			"Rev"

#define	HEADER_MAX_SIZE			(2048)

#define BLANK_CHAR			' '
#define RETURN_STRING			"\n"
#define SEPARATOR_CHAR			'*'
#define START_CHAR			'~'

#define	TEMP_LOGFILE_NAME		"LogFileTemp.txt"

#define	LINUX_YEAR_ORIGIN		(1900)
#define	LINUX_MONTH_ORIGIN		(1)


typedef	struct
{
	// Year
	unsigned long			ulYear;

	// Month
	unsigned long			ulMonth;

	// Date
	unsigned long			ulDate;
}
LOG_DATE, *LPLOG_DATE;



// Define Inside Function

int LogFileOpen(
		char		*lpcLogFileName
		);

void MakeLogHeader(
		unsigned long	ulDataLength,
		char		*lpcData,
		char		*lpcHeaderString,
		unsigned long	*ulHeaderStringLength
		);

int LogHeaderWrite(
		int		iLogFileHandle,
		char		*lpcHeaderString,
		unsigned long	ulHeaderStringLength,
		long		*lDetailError
		);

int WriteDataExe(
		int		iLogFileHandle,
		char		*lpcData,
		unsigned long	ulDataLength,
		int		iStartChar,
		long		*lDetailError
		);

int LogWriteFile(
		int		iLogFileHandle,
		unsigned long	ulDataLength,
		char		*lpcData,
		long		*lDetailError
		);

int AjustAndWriteData(
		int		iLogFileHandle,
		char		*cData,
		unsigned long	ulDataLength,
		long		*lDetailError
		);

int Write_START_CHAR(
		int		iLogFileHandle,
		long		*lDetailError
		);

void SetLogFileAttribute(
		char		*cLogFileName,
		long		lAttribute
		);


#ifdef __cplusplus
}
#endif

#endif	// _FILE_PROC_H_

