#ifndef __LOGGER_H_
#define __LOGGER_H_

#define LEVEL_CRITICAL  (int)1
#define LEVEL_ERROR     (int)2
#define LEVEL_WARNING   (int)3
#define LEVEL_INFO      (int)4
#define LEVEL_DEBUG     (int)5

#define LOG_FILENAME "peripheral.log"

#define DEFAULT_LOG_LEVEL   LEVEL_DEBUG

#define LEVEL_NO_PERMIT  -1
#define LEVEL_PERMIT 	1
#define DEFAULT_LOG_LIMIT   (4 * 1024 * 1024)
#define MAX_LOG_LEN      256

#define LOGGER_DAEMON
/* loggger file format */
/*
 * 	[CCR][DBG 09/24/14 13:13:00:365 ccr.c 1315] scheduler: add exit
 *   Module Name, DEBUG LEVEL timestamp File name, line 
 */
typedef struct  {
	char title[12];
	char file_name[32];
	int line;
	char log_buf[MAX_LOG_LEN];

}LogMessage;
void log_append_to_file(char *file_name, char *str);

void hex_log_append_to_file(char *filename, char *str, int length,
			    char *sourceFile, int fileLine);
void log_func(const char *file_name,int debug_level, char *format, ...);


void log_output(const char *log_file_name,const char *module_name,int debug_level,
		const char *source_file_name,const char *fctn,int line, char *format, ...);
		

void log_send_queue(const char *module_name,int debug_level,
		const char *source_file_name,const char *fctn,int line, char *format, ...);

int logger_init(void);

int logger_close(void);

		
		
/* macro for different Debug LEVEL */

#ifdef LOGGER_DAEMON

#ifdef DEBUG
	#define ALOGD(module_name,fmt,args...) log_send_queue(module_name, LEVEL_DEBUG, __FILE__,__func__,__LINE__,fmt,##args);
	#define ALOGHEX(module_name,str,length)  log_send_hexmessage(module_name,LEVEL_DEBUG,__FILE__,__func__,__LINE__, str,length);
#else
	#define ALOGD(module_name,fmt,args...) 
	#define ALOGHEX(module_name,str,length) 
#endif
#define ALOGI(module_name,fmt,args...) log_send_queue(module_name, LEVEL_INFO, __FILE__,__func__,__LINE__,fmt,##args);
#define ALOGW(module_name,fmt,args...) log_send_queue(module_name, LEVEL_WARNING, __FILE__,__func__,__LINE__,fmt,##args);
#define ALOGE(module_name,fmt,args...) log_send_queue(module_name, LEVEL_ERROR, __FILE__,__func__,__LINE__,fmt,##args);


#else /* write log file directly */
#ifdef DEBUG
	#define ALOGD(module_name,fmt,args...) log_output(LOG_FILENAME,module_name, LEVEL_DEBUG, __FILE__,__func__,__LINE__,fmt,##args);
#else
	#define ALOGD(module_name,fmt,args...) 
#endif
#define ALOGI(module_name,fmt,args...) log_output(LOG_FILENAME,module_name, LEVEL_INFO, __FILE__,__func__,__LINE__,fmt,##args);
#define ALOGW(module_name,fmt,args...) log_output(LOG_FILENAME,module_name, LEVEL_WARNING, __FILE__,__func__,__LINE__,fmt,##args);
#define ALOGE(module_name,fmt,args...) log_output(LOG_FILENAME,module_name, LEVEL_ERROR, __FILE__,__func__,__LINE__,fmt,##args);
#endif //LOGGER_DAEMON end

#endif
