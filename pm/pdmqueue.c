
/*
 * list.c
 *
 * Author: Nate Jozwiak njozwiak@mlsw.biz
 *
 * Description: linked-list processing functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>

#include "pdm.h"
#include "logger.h"
#include "pdmqueue.h"


pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;

/*********************************************************
 * NAME: add_list_entry
 * DESCRIPTION: Add an entry to the list
 *
 * INPUT:	list pointer to list structure
 * 			int dest - destination thread
 * 			int socket - originating socket
 * 			char *message - message string
 * OUTPUT: Pointer to newly added list entry, or NULL on error.
 *
 **********************************************************/
struct MessageEntry *pdm_add_list_entry(struct MessageList *list,
					power_message_t * message)
{
    int rc;
	struct MessageEntry *entry;

	entry = (struct MessageEntry *)calloc(1, sizeof(struct MessageEntry));
	if (!entry) {
		return NULL;
	}
	entry->message = (power_message_t *)calloc(1,sizeof(power_message_t));
	if (entry->message == NULL) {
		free(entry);
		return NULL;
	}

	memcpy(entry->message , message,sizeof(power_message_t));
	rc = pthread_mutex_lock(&mutex_queue);
	if (rc != 0) {
        return NULL;
    }

	TAILQ_INSERT_TAIL(&list->head, entry, entries);
	list->size++;
	rc = pthread_mutex_unlock(&mutex_queue);
	if (rc != 0) {
        return NULL;
    }


	ALOGD("PM","list->size %d",list->size);
	return entry;
}

/*********************************************************
 * NAME: init_list
 * DESCRIPTION: Initializes the message list
 *
 * INPUT:	list pointer to list structure
 * OUTPUT:	None
 *
 **********************************************************/
void pdm_init_list(struct MessageList *list)
{
    int rc;
  	rc = pthread_mutex_lock(&mutex_queue);
	if (rc != 0) {
        return ;
    }

	memset(list, 0, sizeof(struct MessageList));
	TAILQ_INIT(&list->head);

	rc = pthread_mutex_unlock(&mutex_queue);
	if (rc != 0) {
        return ;
    }

}

/*********************************************************
 * NAME: del_list_entry
 * DESCRIPTION: Removes an entry from list
 *
 * INPUT:	list pointer to list structure
 * 			entry pointer to entry to remove
 * OUTPUT:
 *
 **********************************************************/
void pdm_del_first_entry(struct MessageList *list)
{
    int rc;
	struct MessageEntry *entry;

	rc = pthread_mutex_lock(&mutex_queue);
	if (rc != 0) {
        return ;
    }


	if (!TAILQ_EMPTY(&list->head)) {
		entry = TAILQ_FIRST(&list->head);

		TAILQ_REMOVE(&list->head, entry, entries);
		list->size--;
		free(entry->message);
		free(entry);

	}

	rc = pthread_mutex_unlock(&mutex_queue);
	if (rc != 0) {
        return;
    }


}

/*********************************************************
 * NAME: Get first entry
 * DESCRIPTION: Get first entry
 *
 * INPUT:	list pointer to list structure
 * 			entry pointer to entry to remove
 * OUTPUT:
 *
 **********************************************************/

struct MessageEntry *pdm_get_first_entry(struct MessageList *list)
{
    int rc;
    struct MessageEntry *entry = NULL;
	rc = pthread_mutex_lock(&mutex_queue);
	if (rc != 0) {
        return NULL;
    }


	if (!TAILQ_EMPTY(&list->head)) {

		entry = TAILQ_FIRST(&list->head);
	}
    rc = pthread_mutex_unlock(&mutex_queue);
	if (rc != 0) {
        return NULL;
    }


	return entry;
}

/* empty check */

int pdm_entry_empty(struct MessageList *list)
{
    int rc;
    rc = pthread_mutex_lock(&mutex_queue);
	if (rc != 0) {
        return 0;
    }
	if (!TAILQ_EMPTY(&list->head)) {

        rc = pthread_mutex_unlock(&mutex_queue);
        if (rc != 0) {
            return 0;
        }
        return 1;
	}

    rc = pthread_mutex_unlock(&mutex_queue);
    if (rc != 0) {
        return 0;
    }
	return 0;
}
