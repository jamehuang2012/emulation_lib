/*************************************

	CollectLogEx.h

	Logging Library Inside Header File

**************************************/

#ifndef	_COLLECT_LOG_H_
#define	_COLLECT_LOG_H_

#ifdef __cplusplus
extern "C"
{
#endif


// Data Definition

#define	COLLECTLOGEX_LIBRARY_FILENAME		"CollectLogEx"
#define	COLLECTLOGEX_LIBRARY_REVISION		"2709-02A"

#define	TRUE					(1)
#define	FALSE					(0)

#define	CONFIG_FILE				"LogConfig.ini"
#define	RETURN_CODE				0x0A

#define	PARAM_LOG_FILE_NAME			"LogFileName:"
#define	PARAM_LOG_FILE_FOLDER			"LogFileFolder:"
#define	PARAM_MAX_NUMBER_OF_CHAR_PER_LINE	"MaxNumberOfCharactersPerLine:"
#define	PARAM_MAX_NUMBER_OF_LINES		"MaxNumberOfLines:"

#define	DEFAULT_LOG_FILE_NAME			"$LogEx.txt"
#define	DEFAULT_LOG_FILE_FOLDER			"."
#define	DEFAULT_NUMBER_OF_CHAR_PER_LINE		(256)
#define	MAX_NUMBER_OF_CHAR_PER_LINE		(256)
#define	MIN_NUMBER_OF_CHAR_PER_LINE		(100)
#define	DEFAULT_NUMBER_OF_LINES			(10000)
#define	MAX_NUMBER_OF_LINES			(100000)
#define	MIN_NUMBER_OF_LINES			(1)

#define	SUFFIX_OF_FILE_REVISION			"Rev"
#define	LENGTH_OF_REVISION			(8)


#if !( defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED) )
union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
		struct seminfo *__buf;
		};
#endif


// Define Inside Function

long	GetConfigration( long *lErrorDetailCode );
long	LogInit( long *lErrorDetailCode );


#ifdef __cplusplus
}
#endif

#endif	// _COLLECT_LOG_H_
