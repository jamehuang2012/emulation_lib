#include <stdio.h>
#include "logger.h"

#include <pthread.h>
#include <stdio.h>
#define NUM_THREADS     5

void *msg_thread_func(void *threadid)
{
   int i;
   long tid;
   
   char buf[256];
   tid = (long)threadid;
   for (i = 0 ; i < 10 ; i ++) {
		ALOGI("TST","tid = %d thread id = %d %d",tid,pthread_self(),i);
		ALOGHEX("HXT",buf,256);
		usleep(50000);
   }
   
   pthread_exit(NULL);
}

int main(int argc, char **argv) 
{
   pthread_t threads[10];
   int rc;
   long t;
   
#ifdef LOGGER_DAEMON
	logger_init();
#endif
	
 
   for(t=0; t<10; t++){
      printf("In main: creating thread %ld\n", t);
      rc = pthread_create(&threads[t], NULL, msg_thread_func, (void *)t);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         break;
      }
   }
  
   
   
	
	sleep(2);
#ifdef LOGGER_DAEMON
	logger_close();
#endif
 
 pthread_exit(NULL);
	
}
