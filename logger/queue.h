#ifndef _PDM_LIST_H_
#define _PDM_LIST_H_

#include <sys/queue.h>
#include "logger.h"

#define PM_THREAD	0
#define PDM_THREAD	1


struct LogEntry{
	 char *message;
	 TAILQ_ENTRY(LogEntry) entries;
};

struct LogList {
	 int size;

	 TAILQ_HEAD(, LogEntry) head;
};

struct LogEntry *log_add_list_entry(struct LogList *list,
					char * message,int length);

void log_init_list(struct LogList *list);

void log_del_first_entry(struct LogList *list);

struct LogEntry *log_get_first_entry(struct LogList *list);
int log_entry_empty(struct LogList *list);
#endif
