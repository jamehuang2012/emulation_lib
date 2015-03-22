/*************************************

	CollectLogExDef.h

	Logging Library Header File

**************************************/

#ifndef	_COLLECT_LOG_EX_DEF_H_
#define	_COLLECT_LOG_EX_DEF_H_

#ifdef __cplusplus
extern "C"
{
#endif


//----------------------------------------------------------------------
//	Define Data type
//----------------------------------------------------------------------

// Data structure for Library Revision typedef	struct
{
	char	cFilename[32];
	char	cRevision[32];
}
LIB_INFORMATION_COLLECT_LOG_EX, *LPLIB_INFORMATION_COLLECT_LOG_EX;


//----------------------------------------------------------------------
//	Return codes of Function
//----------------------------------------------------------------------

#define		LOG_SUCCESS			(0)
#define		LOG_NO_COLLECT			(1)
#define		LOG_ERR_CREATE_MUTEX		(-1)
#define		LOG_ERR_INVALID_PARAMETER	(-2)
#define		LOG_ERR_FILE_OPEN		(-3)
#define		LOG_ERR_NOT_CALL_OPEN		(-4)
#define		LOG_ERR_RELEASE_MUTEX		(-5)
#define		LOG_ERR_WRITE_FILE		(-6)
#define		LOG_ERR_INTERNAL_FUNCTION	(-7)


//----------------------------------------------------------------------
//	Library Function
//----------------------------------------------------------------------

long	CollectLogExOpen(
			unsigned long	ulStringLength,
			char 		*lpstring,
			long		*lDetailError
			);

long	CollectLogExClose(
			long		*lDetailError
			);

long	CollectLogExWrite(
			unsigned long	ulLogMessageLength,
			char 		*lpLogMessage,
			long		*lDetailError
			);

void	CollectLogExGetRevision(
			LPLIB_INFORMATION_COLLECT_LOG_EX	lpLibInfo
			);


#ifdef __cplusplus
}
#endif

#endif	// _COLLECT_LOG_EX_DEF_H_

