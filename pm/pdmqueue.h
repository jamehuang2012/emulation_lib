#ifndef _PDM_LIST_H_
#define _PDM_LIST_H_

#include <sys/queue.h>

#define PM_THREAD	0
#define PDM_THREAD	1

struct MessageEntry {
	power_message_t *message;
	 TAILQ_ENTRY(MessageEntry) entries;
};

struct MessageList {
	int size;

	 TAILQ_HEAD(, MessageEntry) head;
};

struct MessageEntry *pdm_add_list_entry(struct MessageList *list,
					power_message_t * message);

void pdm_init_list(struct MessageList *list);

void pdm_del_first_entry(struct MessageList *list);

struct MessageEntry *pdm_get_first_entry(struct MessageList *list);
int pdm_entry_empty(struct MessageList *list);
#endif
