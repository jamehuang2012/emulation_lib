
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "logger.h"

#include "list.h"





static pthread_once_t hash_init_block = PTHREAD_ONCE_INIT;
static pthread_mutex_t hash_map_mutex = PTHREAD_MUTEX_INITIALIZER;
static list_t *cache_list = NULL;

/* proviate interface for scu */

#define MAX_ID_LEN 32
typedef struct {

	char user_id[MAX_ID_LEN];
	char bike_id[MAX_ID_LEN];
	struct timeval timestamp;
	int  status;    /* 0 waiting for uploading, 1 uploaded*/
	int  update_status;
	/* if status = 2 , use this flag to indicat if sbe allows user to rent bike*/
}_user_status;

static
int key_msne_equals(_user_status *src,_user_status *dest )
{

    return 0 == strncmp(src->user_id,dest->user_id,MAX_ID_LEN);
}

static
int bike_msne_equals(_user_status *src,_user_status *dest )
{

    return 0 == strncmp(src->bike_id,dest->bike_id,MAX_ID_LEN);
}

void hash_init_routine(void)
{

    if (NULL == cache_list) {
        cache_list = list_new();
    }

    ALOGI("HASH", "start hash table for caching the user status");
}

int hash_init(void)
{
	int status;
	status = pthread_once(&hash_init_block,hash_init_routine);
	if ( 0 != status ) {
		ALOGE("HASH", "Initialize thread failed\n");
		return -1;
	}

	return 0;

}

int hash_close(void)
{

    if ( NULL != cache_list) {
        list_destroy(cache_list);
        ALOGI("HASH","free cache list .... ");
    }

    return 0;

}
/*
* Cache the Subscriber user id
* Cache the Bike rfid
*/

int hash_add_user_status(char *user_id,char *bike_id,int status)
{

    int error;
    _user_status *user_rfid;

/*  mutex */
    user_rfid = calloc(sizeof(char),sizeof(_user_status));

    if (NULL == user_rfid) {
        ALOGE("HASH","Failed to malloc memory");
        return -1;
    }
    strncpy(user_rfid->user_id,user_id,MAX_ID_LEN);
    strncpy(user_rfid->bike_id,bike_id,MAX_ID_LEN);
    user_rfid->status = status;

    ALOGI("HASH","add user user id = %s , bike id = %s",user_id,bike_id);

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }

    list_node_t * node = list_node_new(user_rfid);
    list_rpush(cache_list,node);
    fprintf(stderr,"listh length = %d\n",cache_list->len);

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }

    return 0;



}

/*
* Get info from hash map
* Iterator the hash table , find if there is user rfid
*/

int hash_has_element(char *user_id)
{

    int error;
    int result;
    _user_status rfid;

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    strncpy(rfid.user_id,user_id,MAX_ID_LEN);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    list_node_t *node = list_find(cache_list,&rfid);

    if (NULL == node)
        result = -1;
    else
        result = 0;

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }


    return result;
}


/*
* Remove user rfid hash item from user hash table ,
* also remove the bike rfid from bike hash table
*/

int hash_remove_user_status(char *user_id)
{
    int error;
    int result;

    _user_status rfid;
    strncpy(rfid.user_id,user_id,MAX_ID_LEN);
    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }

    ALOGI("HASH","Remove user id = %s ",user_id);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    list_node_t *node = list_find(cache_list,&rfid);

    if (NULL != node) {
        /* remove the list node from cache list */
        list_remove(cache_list,node);
        result = 0;
    }
    else {
	result = -1;
    }


    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }

    return result;

}



/*
* Remove bike rfid hash item from bike hash table ,
* also remove the bike rfid from bike hash table
*/

int hash_remove_bike_status(char *bike_id)
{
    int error;
    int result;


    _user_status rfid;
    strncpy(rfid.bike_id,bike_id,MAX_ID_LEN);

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }

    ALOGI("HASH","Remove bike id = %s ",bike_id);
    fprintf(stderr,"Remove bike id  = %s\n",bike_id);
    cache_list->match = bike_msne_equals;  /* find key rfid from list */
    list_node_t *node = list_find(cache_list,&rfid);

    if (NULL != node) {
        /* remove the list node from cache list */
        list_remove(cache_list,node);
        result = 0;
    }
    else {

	fprintf(stderr,"Doesn't find the bike id \n");
	result = -1;
    }


    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }
    return result;
}


long hash_cal_tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec-older.tv_sec)*1000+
    (newer.tv_usec-older.tv_usec)/1000;
}


static int expired_count = 120000; /* 2mins dealy */
#define INIT_STATUS 1
#define FIN_STATUS 2
/*
*   check if user id in the list, if not add user status into cache
*/
int hash_check_and_add_user_stauts(char *user_id,char *bike_id,int status,int update_status)
{

    int error;
    int result = 0;
    struct timeval ts;
    _user_status *user_rfid;
    _user_status u_rfid;
    list_node_t *node;
    long time_diff;


    ALOGI("HASH", "hash check and add user stauts user_id = %s,bike id = %s",user_id,bike_id);

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    strncpy(u_rfid.user_id,user_id,MAX_ID_LEN);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    node = list_find(cache_list,&u_rfid);

    if (NULL != node) {
    /* if cache has this user id and expired , just replace it */
    /* check if there is item in cache */
        ALOGI("HASH", "Found user id is in cache list");
        user_rfid = (_user_status *) node->val;

        if (user_rfid->status == FIN_STATUS && user_rfid->update_status == 0) {
            gettimeofday(&ts,0);
            time_diff = hash_cal_tvdiff(ts,user_rfid->timestamp);
            if (time_diff <= expired_count) {
            ALOGI("HASH", "Not expired, can't replace %d",time_diff);
            user_rfid->update_status = update_status;
            result = 2;

            }else  {

                ALOGI("HASH","time 1 %d %d",ts.tv_sec,ts.tv_usec);
                ALOGI("HASH","time 2 %d %d",user_rfid->timestamp.tv_sec,user_rfid->timestamp.tv_usec);

            ALOGI("HASH", "Expired, replace %d",time_diff);
            strncpy(user_rfid->user_id,user_id,MAX_ID_LEN);
            strncpy(user_rfid->bike_id,bike_id,MAX_ID_LEN);
            user_rfid->status = status;
            result = 0;

            }
        } else {
            ALOGI("HASH","Status = %d , process status = %d ", user_rfid->status,user_rfid->update_status);
            result = -1;
        }

    } else {
	    ALOGI("HASH", "Not Found user id is in cache list");
            user_rfid = calloc(sizeof(char),sizeof(_user_status));

	    if (NULL == user_rfid) {
		    ALOGE("HASH","Failed to malloc memory");
		    result = -1;
	    } else {
		    strncpy(user_rfid->user_id,user_id,MAX_ID_LEN);
		    strncpy(user_rfid->bike_id,bike_id,MAX_ID_LEN);
		    user_rfid->status = status;

		    ALOGI("HASH","add user user id = %s , bike id = %s",user_id,bike_id);

		    node = list_node_new(user_rfid);
		    list_rpush(cache_list,node);
		    fprintf(stderr,"listh length = %d\n",cache_list->len);
		    result = 0;
        }


    }


    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }


    return result;
}

/*
*   update status ,and start timing
*/

int hash_update_user_status(char *user_id,int status)
{
    int error;
    int result;
    _user_status rfid;
    _user_status *user_rfid;
    struct timeval timestamp;


    ALOGI("HASH", "hash_update_user_status user id = %s status = %d",user_id,status);
    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    strncpy(rfid.user_id,user_id,MAX_ID_LEN);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    list_node_t *node = list_find(cache_list,&rfid);

    if (NULL != node) {
        /* update status to 2 */
        user_rfid = (_user_status *)node->val;
        user_rfid->status = status;
        gettimeofday(&timestamp,0);
        user_rfid->timestamp = timestamp;
	ALOGI("HASH","time %d %d",timestamp.tv_sec,timestamp.tv_usec);
        result = 0;

    }else {
        result = -1;

    }

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }

    return result;

}
/* remove expired status from cache */
int hash_remove_expired_status(void)
{


    int error;

    _user_status rfid;
    _user_status *user_rfid;
    struct timeval ts;
    list_node_t *node;
    long time_diff;


    ALOGI("HASH", "Clean expired records");
    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }




    list_iterator_t *it = list_iterator_new(cache_list, LIST_HEAD);
    do {
      node = list_iterator_next(it);

      if (NULL != node ) {
	 user_rfid = (_user_status *) node->val;
	  if (user_rfid->status == FIN_STATUS ) {
	      gettimeofday(&ts,0);
	      time_diff = hash_cal_tvdiff(ts,user_rfid->timestamp);
	      if (time_diff >= expired_count) {

		 ALOGI("HASH","Remove rfid = %s from cache",user_rfid->user_id);
		 list_remove(cache_list,node);

	      }
	  }
      }

      } while (node != NULL);

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }




    return 0;

}

/*
*  setup the timer of expired
*/

int hash_setup_expired_time(int expired_time)
{
    int error;
    ALOGI("HASH", "Set expired time = %d seconds",expired_count);
    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    /* at least 2 mins */
    if ( (expired_time * 1000) > 120000) {
        expired_count = expired_time * 1000;
        ALOGI("HASH","Setting expired count to %d seconds",expired_time);
    } else {
        ALOGE("HASH","Setting expired count is invalid , time =  %d seconds",expired_time);

    }

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }

    return 0;
}



/*
*   Since there is user_id in the cache , and status = 2
*   then,SBE allows this user to rent the bike due to no  opened trip in the backend
*
*/
int hash_update_process_stauts(char *user_id,int status)
{
 int error;
    int result = 0;
    _user_status *user_rfid;
    _user_status u_rfid;
    list_node_t *node;


    ALOGI("HASH", "hash check and add user stauts user_id = %s",user_id);

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    strncpy(u_rfid.user_id,user_id,MAX_ID_LEN);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    node = list_find(cache_list,&u_rfid);

    if (NULL != node) {
    /* if cache has this user id and expired , just replace it */
    /* check if there is item in cache */
        ALOGI("HASH", "update process user id  = %s  status  to %d is in cache list",user_id,status);
        user_rfid = (_user_status *) node->val;

        if (user_rfid->status == FIN_STATUS ) {

            user_rfid->update_status = status;
            result = 0;
        } else {
            ALOGI("HASH","Can update process status due to user status =1 ");
            result = -1;
        }

    } else {
	    ALOGI("HASH", "Not Found user id is in cache list,Can't update");
        result = -1;
 	}

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }

    return result;

}



/*
*   Since there is user_id in the cache , and status = 2
*   then,SBE allows this user to rent the bike due to no  opened trip in the backend
*
*/
int hash_force_update_user_stauts(char *user_id,char *bike_id,int status)
{

    int error;
    int result = 0;
    _user_status *user_rfid;
    _user_status u_rfid;
    list_node_t *node;


    ALOGI("HASH", "force update  user_id = %s,bike id = %s",user_id,bike_id);

    error = pthread_mutex_lock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex lock fail error= %d",error);
        return -1;
    }
    strncpy(u_rfid.user_id,user_id,MAX_ID_LEN);
    cache_list->match = key_msne_equals;  /* find key rfid from list */
    node = list_find(cache_list,&u_rfid);

    if (NULL != node) {
    /* if cache has this user id and expired , just replace it */
    /* check if there is item in cache */
        ALOGI("HASH", "Found user id is in cache list");
        user_rfid = (_user_status *) node->val;

        if (user_rfid->status == FIN_STATUS ) {

            strncpy(user_rfid->user_id,user_id,MAX_ID_LEN);
            strncpy(user_rfid->bike_id,bike_id,MAX_ID_LEN);
            user_rfid->status = status;
            user_rfid->update_status = 0;

            result = 0;

            ALOGI("HASH", "update rfid %s info",user_id);

        } else {
            ALOGI("HASH","Can't force update , due to user status =1");
            result = -1;
        }

    } else {
	    ALOGE("HASH", "Not Found user id is in cache list,Can't update");
        result = -1;

 	}

    error = pthread_mutex_unlock(&hash_map_mutex);
    if ( 0 != error) {
        ALOGE("HASH","mutex unlock fail error= %d",error);

    }


    return result;
}





