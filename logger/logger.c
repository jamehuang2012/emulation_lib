/*
 *	logger.c
 *  	PBSC James Huang
 *	June 12 2013
 *
 * 	mofification:
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

#include "logger.h"
#include "queue.h"


pthread_once_t logger_init_block = PTHREAD_ONCE_INIT;

static pthread_t logger_thread_id = 0;



static void *logger_thread(void *arg);
static uint8_t logger_active = 1;



static pthread_mutex_t logger_mutex;	/* Protects access to value */
static pthread_cond_t logger_cond;
struct LogList logger_list;




static unsigned long current_log_limit = (4 * 1024 * 1024);



/*
 * Check the file size , if file size more than current_log_limit
 * rename to file_name.0
 */

void check_file_size(const char *filename)
{
	char backup_filename[100];
	long file_size;
	FILE *fp;

	fp = fopen(filename, "a");
	if (fp != NULL) {
		fseek(fp,0, SEEK_END);
		file_size = ftell(fp);
		fclose(fp);
		if (file_size > current_log_limit ) {

					sprintf(backup_filename,"%s.0",filename);
					rename(filename,backup_filename);

		}

	}

}

/***********************************************************
 * NAME: level_to_str
 * DESCRIPTION: utility to convert an enumeration to a string.
 *
 * IN:  debug level
 * OUT: string representation of the debug level
 ***********************************************************/
static char *level_to_str(int debug_level)
{
	char *level = NULL;

	switch (debug_level) {
	case LEVEL_CRITICAL:
		level = "CRITICAL";
		break;
	case LEVEL_ERROR:
		level = "ERR ";
		break;
	case LEVEL_WARNING:
		level = "WAR ";
		break;
	case LEVEL_INFO:
		level = "INFO";
		break;
	case LEVEL_DEBUG:
		level = "DGB ";
		break;
	default:
		level = "UNKNOWN";
		break;
	}
	return level;
}

int check_log_level(int level)
{
	if (level < LEVEL_CRITICAL) {
		return LEVEL_NO_PERMIT;
	}

	if (DEFAULT_LOG_LEVEL >= level) {
		return LEVEL_PERMIT;
	}

	return LEVEL_NO_PERMIT;

}

void log_append_to_file(char *file_name, char *str)
{
	int ret;
	

	check_file_size(file_name);
	FILE *fo;
	fo = fopen(file_name, "a");
	if (fo == NULL) {
		fprintf(stderr,"fail to open %s file\n",file_name);
		return;
	}
	/* 	[CCR][DBG 09/24/14 13:13:00:365 ccr.c 1315] scheduler: add exit */

	fprintf(fo, "%s\n", str);
	
	fclose(fo);
}

void hex_log_append_to_file(char *file_name, char *str, int length,
			    char *sourceFile, int fileLine)
{
	int ret;
	time_t t;
	int i;
	time(&t);
	struct tm *tp = localtime(&t);
	char now_str[100];
	char backup[100];
	strftime(now_str, 100, "%Y-%m-%d %H:%M:%S", tp);

	check_file_size(file_name);
	
	FILE *fo;
	fo = fopen(file_name, "a");
	if (fo == 0) {
		return;
	}

	fprintf(fo, "%s %s(:%d):\n", now_str, sourceFile, fileLine);
	for (i = 0; i < length; i++) {
		fprintf(fo, "%.2x ", (unsigned char)str[i]);
		if (((i + 1) % 16) == 0) {
			fprintf(fo, "\n");
		}
	}
	fprintf(fo, "\n");
	fclose(fo);
}

void log_func(const char *file_name,int debug_level, char *format, ...)
{
	int ret;
	struct stat info;
	va_list args;
	struct timeval tv;
	char *time;
	char backup[100];


	if (check_log_level(debug_level) == LEVEL_NO_PERMIT)
		return;
	check_file_size(file_name);

	FILE *fo;
	fo = fopen(file_name, "a");
	if (fo != NULL) {


		gettimeofday(&tv, NULL);
		time = ctime(&tv.tv_sec);
		time[strlen(time) - 1] = 0;

		fprintf(fo, "[%s: %s]:", time, level_to_str(debug_level));
		va_start(args, format);
		vfprintf(fo, format, args);
		va_end(args);
		fprintf(fo, "\n");
		fflush(fo);
		fclose(fo);
	}

}

void log_output(const char *log_file_name,const char *module_name,int debug_level,
		const char *source_file_name,const char *fctn,int line, char *format, ...)
{
	va_list args;
	struct timeval tv;
	char *time;
	int milli;
	

	if (check_log_level(debug_level) == LEVEL_NO_PERMIT)
		return;
	check_file_size(log_file_name);

	FILE *fo;
	fo = fopen(log_file_name, "a");
	if (fo != NULL) {


		gettimeofday(&tv, NULL);
		milli = tv.tv_usec / 100;
	
		/* 	[CCR][DBG 09/24/14 13:13:00:365 ccr.c 1315] scheduler: add exit */
		char buffer [80];
		strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
	
		fprintf(fo, "[%s][%s %s:%04d %s %s %d]:",module_name,  level_to_str(debug_level),buffer,milli,source_file_name,fctn,line);
		va_start(args, format);
		vfprintf(fo, format, args);
		va_end(args);
		fprintf(fo, "\n");
		fflush(fo);
		fclose(fo);
	}

}


void log_send_queue(const char *module_name,int debug_level,
		const char *source_file_name,const char *fctn,int line, char *format, ...)
{		
		int milli;
		int result;
		va_list args;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		milli = tv.tv_usec / 10;
		char title[128];
		char body[MAX_LOG_LEN*2];
		char message[MAX_LOG_LEN*2];
		struct LogEntry *entry;
	
		/* 	[CCR][DBG 09/24/14 13:13:00:365 ccr.c 1315] scheduler: add exit */
		char buffer [80];
		strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
#ifdef DEBUG	
		sprintf(title, "[%s][%s %s:%05d %s %s %d]:",module_name,  level_to_str(debug_level),buffer,milli,source_file_name,fctn,line);
#else 

		sprintf(title, "[%s][%s %s:%05d %s %d]:",module_name,  level_to_str(debug_level),buffer,milli,source_file_name,line);
#endif 
		va_start(args, format);
		//result = vsprintf(body, format, args);
		result = vsnprintf(body,MAX_LOG_LEN,format,args);	
		va_end(args);
		if ( result > 0) 
			sprintf(message, "%s%s",title,body);
		else
			sprintf(message, "%s",title);
		
	
		
		result = pthread_mutex_lock(&logger_mutex);
		if ( result != 0 ) {
			fprintf(stderr,"pthread mutex lock error = %d\n", result);
			return;
		}
		
		/* put the message into queue */ 
	
			
		
		entry = log_add_list_entry(&logger_list,message,strlen(message));
		
		result = pthread_cond_signal(&logger_cond);
		
		result = pthread_mutex_unlock(&logger_mutex);
		if ( result != 0 ) {
			fprintf(stderr,"pthread mutex lock error = %d\n", result);
			return ;
		}
		
		
		
		
}


void log_send_hexmessage(const char *module_name,int debug_level,
		const char *source_file_name,const char *fctn,int line, char *str, int length)
{		
		int milli;
		int result,i;
		va_list args;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		milli = tv.tv_usec / 10;
		char title[128];
		char body[MAX_LOG_LEN*4];
		char message[MAX_LOG_LEN*4];
		struct LogEntry *entry;
		char *msg=body;
	
		/* 	[CCR][DBG 09/24/14 13:13:00:365 ccr.c 1315] scheduler: add exit */
		char buffer [80];
		strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
	
		sprintf(title, "[%s][%s %s:%05d %s %s %d]:",module_name,  level_to_str(debug_level),buffer,milli,source_file_name,fctn,line);
		
		
		for (i = 0; i < length; i++) {
			sprintf(msg, "%.2X ", (unsigned char)str[i]);
			msg +=3;
			if (((i + 1) % 16) == 0) {
				sprintf(msg, "\n");
				msg++;
			}
		}
	
		
		sprintf(message, "%s\n%s\n",title,body);
 	
		
		
		result = pthread_mutex_lock(&logger_mutex);
		if ( result != 0 ) {
			fprintf(stderr,"pthread mutex lock error = %d\n", result);
			return;
		}
		
		/* put the message into queue */ 
		
		
		
		
		entry = log_add_list_entry(&logger_list,message,strlen(message));
		
		result = pthread_cond_signal(&logger_cond);
		
		result = pthread_mutex_unlock(&logger_mutex);
		if ( result != 0 ) {
			fprintf(stderr,"pthread mutex lock error = %d\n", result);
			return ;
		}
		
		
		
		
}


/* 
	Logfile thread 
*/




/* 
 * thread function for logger service 
 * 
 * 
 */

 
static void *logger_thread(void *arg)
{
    int result;
	char log_buffer[MAX_LOG_LEN+1];
    struct LogEntry *entry;
    
    
    pthread_detach(pthread_self());


	
    while ( logger_active == 1) {
    
		result = pthread_mutex_lock(&logger_mutex);
		if (result != 0) {
		
			fprintf(stderr,"Pthread mutex lock error\n");
		}
		

		if (log_entry_empty(&logger_list) == 0 && logger_active != 0){
			
			result = pthread_cond_wait(&logger_cond, &logger_mutex);
		}
		
		
		if (logger_active == 0) {
			result = pthread_mutex_unlock(&logger_mutex);
			if (result != 0) {
				/* critical issue if system has this error */
				fprintf(stderr,"unlock thread error =%d",result);
				
			}
			break;
		}
			/* get first logger item from queue */
			entry = log_get_first_entry(&logger_list);
			
			if (entry != NULL ) {
			//	memcpy(log_buffer,entry->message,sizeof(char)*MAX_LOG_LEN);
				log_append_to_file(LOG_FILENAME,entry->message);
				log_del_first_entry(&logger_list);
				entry = NULL;
			} 
			
			
			result = pthread_mutex_unlock(&logger_mutex);
			if (result != 0) 
				fprintf(stderr,"mutex unlock error = %d\n", result);
				
			
			/* write the log buffer to files */
			
			
//			log_append_to_file(LOG_FILENAME,log_buffer);

			
			
	}
		
}

/*
 * Initialize routine
 */
void logger_init_routinue(void)
{
	
        int rc;
	/* Initialize logfile , create /open log file */

    	log_init_list(&logger_list);
	/* create the call back thread */
	fprintf(stderr,"start logging ...\n");
        if ((rc = pthread_create(&logger_thread_id, NULL, logger_thread,
	                        (void *) NULL))) {
	    fprintf(stderr,"Create Thread error %d\n",rc);
	 }
	
}


/*
 * Initialize the logger file service , create the logger thread
 *
 */
int logger_init(void)
{
	 int status;
	 status = pthread_once(&logger_init_block, logger_init_routinue);
	 if (status != 0) {
	        fprintf(stderr, "init thread fail\n");
	        return -1;
	 }


	return 0;
}

/* 
 * 
 * stop the service thread 
 * 
 */

int logger_close(void)
{
	int result;
    
     	result = pthread_mutex_lock(&logger_mutex);
	if ( result != 0 ) {
			fprintf(stderr,"pthread mutex lock error = %d\n", result);
			return -1;
	}
		
		/* put the message into queue */ 
		
    	logger_active = 0;
		
	result = pthread_cond_signal(&logger_cond);
		
		 
	result = pthread_mutex_unlock(&logger_mutex);
	if ( result != 0 ) {
		fprintf(stderr,"pthread mutex lock error = %d\n", result);
		return -1;
	}
	
	
		
}
