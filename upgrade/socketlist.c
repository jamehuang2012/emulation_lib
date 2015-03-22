/*
 * Copyright (C) 2011, 2012 MapleLeaf Software, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * list.c
 *
 * Author: Burt Bicksler bbicksler@mlsw.biz
 *
 * Description: Linked list processing functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <errno.h>

#include "socketlist.h"


/*********************************************************
 * NAME: add_socketlist_entry
 * DESCRIPTION: Add a Socket Entry to the list using the
 *              socket Handle.
 *
 * IN: list pointer to Socket List structure.
 *     socketHandle to be added.
 *
 * OUT: Pointer to newly added list entry, or NULL on error.
 *
 **********************************************************/
struct socketEntry * add_socketlist_entry(struct socketlist *list, int socketHandle)
{
    struct socketEntry *pSocketEntry;
    pSocketEntry = (struct socketEntry *)calloc(1, sizeof(struct socketEntry));
    if (!pSocketEntry)
    {
        return NULL;
    }

    // Init to unopened value
    pSocketEntry->socketHandle = socketHandle;

    TAILQ_INSERT_TAIL(&list->head, pSocketEntry, next_entry);
    list->size++;
    return pSocketEntry;
}

/*********************************************************
 * NAME: del_socketlist_entry
 * DESCRIPTION: Removes Socket Entry from list.
 *
 * IN: list pointer to Socket list.
 *     pSocketEntry pointer to Socket Entry to remove.
 * OUT: Nothing.
 *
 **********************************************************/
void del_socketlist_entry(
    struct socketlist *list,
    struct socketEntry *pSocketEntry)
{
    TAILQ_REMOVE(&list->head, pSocketEntry, next_entry);
    list->size--;
    free(pSocketEntry);
}

/*********************************************************
 * NAME: find_socketlist_entry_by_handle
 * DESCRIPTION: Returns a Socket Entry using socket handle
 *              as the Key.
 *
 * IN: list pointer to Socket List.
 *     socketHandle Socket Handle to search for.
 * OUT: Pointer to Socket Entry found or NULL.
 **********************************************************/
struct socketEntry *find_socketlist_entry_by_handle(
    struct socketlist *list,
    const int socketHandle)
{
    struct socketEntry *pSocketEntry;

    TAILQ_FOREACH(pSocketEntry, &list->head, next_entry)
    {
        if (socketHandle == pSocketEntry->socketHandle)
         return pSocketEntry;
    }
    return NULL;
}

/*********************************************************
 * NAME: find_socketlist_entry_by_connType
 * DESCRIPTION: Returns a Socket Entry using the connection
 *              type as key.
 * IN: list pointer to Socket List.
 *     connType connection type to search for.
 * OUT: Socket Entry found, or NULL.
 *
 **********************************************************/
struct socketEntry *find_socketlist_entry_by_connType(
    struct socketlist *list,
    ccr_connection_type_t connType)
{
    struct socketEntry *pSocketEntry;

    TAILQ_FOREACH(pSocketEntry, &list->head, next_entry)
    {
        if (connType == pSocketEntry->connType)
         return pSocketEntry;
    }
    return NULL;
}

/*********************************************************
 * NAME: init_list
 * DESCRIPTION: Initializes the Socket List
 *
 * IN: list pointer to Socket List.
 * OUT: Nothing.
 *
 **********************************************************/
void init_socketlist(struct socketlist *list)
{
    memset(list, 0, sizeof(struct socketlist));
    TAILQ_INIT(&list->head);
}

/*********************************************************
 * NAME: getSocketListSize
 * DESCRIPTION: Returns the number of entries in the
 *              Socket List.
 *
 * IN: list pointer to Socket List.
 * OUT:  Number of entries in the Socket List.
 *
 **********************************************************/
int getSocketListSize(struct socketlist *list)
{
    return list->size;
}

