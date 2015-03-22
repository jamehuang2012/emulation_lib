#ifndef SVS_LOG_H
#define SVS_LOG_H

#include <stdio.h>
#include <pthread.h>

#define LOG_DEFAULT_FILE_NAME   "/usr/scu/logs/svsd"
#define LOG_BUF_SIZE            512
#define LOG_MSG_PAYLOAD_MAX     512

typedef enum
{
    LOG_VERBOSITY_NONE,
    LOG_VERBOSITY_ERROR,
    LOG_VERBOSITY_WARNING,
    LOG_VERBOSITY_INFO,
    LOG_VERBOSITY_DEBUG,
    LOG_VERBOSITY_MAX  // Keep last
} log_verbosity_t;

typedef struct
{
    pthread_mutex_t mutex;
    log_verbosity_t verbosity;
} logInfo_t;

typedef struct
{
    int             isInit;
    const char      *appName;
    log_verbosity_t *verbosity;
    pthread_mutex_t *mutex;

    char buf[LOG_BUF_SIZE];
    FILE *fp;

} log_state_t;

typedef struct
{
    log_verbosity_t verbosity;
} svsMsgLogHeader_t;

//extern log_state_t log_state;

int svsLogServerInit(const char *fileName);
int svsLogServerUninit(void);

void logInit(char *fileName, int isInit, const char *appName, logInfo_t *logInfo);
void logUninit(void);
int logVerbositySet(log_verbosity_t verbosity);
void logOutput(log_verbosity_t verb, char *file, const char *fctn, int line, const char *format, ...);

//#define log(verb, fmt, args...)     logOutput(verb,fmt,##args) //   ((verb<=*log_state.verbosity) ? (logOutput(fmt,##args)) : ((void *)0))

//#define logError(fmt,args...)       (log(LOG_VERBOSITY_ERROR,"ERROR: [%s %s() %d] ", __FILE__, __FUNCTION__, __LINE__), log(LOG_VERBOSITY_ERROR,fmt,##args))
//#define logWarning(fmt,args...)     (log(LOG_VERBOSITY_WARNING,"WARNING: [%s %s() %d] ", __FILE__, __FUNCTION__, __LINE__), log(LOG_VERBOSITY_WARNING,fmt,##args))
//#define logInfo(fmt,args...)        (log(LOG_VERBOSITY_INFO,"INFO: "), log(LOG_VERBOSITY_INFO,fmt,##args))
//#define logDebug(fmt,args...)       (log(LOG_VERBOSITY_DEBUG,"DEBUG: [%s %s() %d] ", __FILE__, __FUNCTION__, __LINE__),log(LOG_VERBOSITY_DEBUG,fmt,##args))

#define logError(fmt,args...)       (logOutput(LOG_VERBOSITY_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logWarning(fmt,args...)     (logOutput(LOG_VERBOSITY_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logInfo(fmt,args...)        (logOutput(LOG_VERBOSITY_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))
#define logDebug(fmt,args...)       (logOutput(LOG_VERBOSITY_DEBUG, __FILE__, __FUNCTION__, __LINE__, fmt, ##args))


#endif // SVS_LOG_H
