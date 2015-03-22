
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

#include "logger.h"
#include "queue.h"



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
struct LogEntry *log_add_list_entry(struct LogList *list,
					char * message,int length)
{
    	int rc;
	struct LogEntry *entry;

	entry = (struct LogEntry *)calloc(1, sizeof(struct LogEntry));
	if (!entry) {
		return NULL;
	}
	entry->message = (char *)calloc(1,(sizeof(char)*length));
	if (entry->message == NULL) {
		free(entry);
		return NULL;
	}
#if 0	
	if (strlen(message) > MAX_LOG_LEN) 
		memcpy(entry->message , message,sizeof(char)*MAX_LOG_LEN);
	else
		memcpy(entry->message , message,strlen(message));
#endif 
	
	memcpy(entry->message , message,length);

	TAILQ_INSERT_TAIL(&list->head, entry, entries);
	list->size++;
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
void log_init_list(struct LogList *list)
{
	memset(list, 0, sizeof(struct LogList));
	TAILQ_INIT(&list->head);

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
void log_del_first_entry(struct LogList *list)
{
	struct LogEntry *entry;

	if (!TAILQ_EMPTY(&list->head)) {
		entry = TAILQ_FIRST(&list->head);

		TAILQ_REMOVE(&list->head, entry, entries);
		list->size--;
		free(entry->message);
		free(entry);
		
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

struct LogEntry *log_get_first_entry(struct LogList *list)
{
    int rc;
    struct LogEntry *entry = NULL;

	if (!TAILQ_EMPTY(&list->head)) {

		entry = TAILQ_FIRST(&list->head);
	}

	return entry;
}

/* empty check */

int log_entry_empty(struct LogList *list)
{
	if (!TAILQ_EMPTY(&list->head)) {
               return 1;
	}
   	return 0;
}
