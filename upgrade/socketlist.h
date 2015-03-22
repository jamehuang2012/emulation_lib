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
 * socketlist.h
 *
 * Author: Burt Bicksler bbicksler@mlsw.biz
 *
 * Description: Header file for socket connection list processing.
 */

#ifndef _SOCKET_LIST_H_
#define _SOCKET_LIST_H_

#include <sys/queue.h>
#include <cardrdrdatadefs.h>

struct socketEntry
{
    ccr_connection_type_t connType;
    int socketHandle;

   TAILQ_ENTRY( socketEntry ) next_entry;  /* Used by linked list processing */
};

struct socketlist
{
    int size;

    TAILQ_HEAD(, socketEntry) head;
};

extern void init_socketlist( struct socketlist *list );
extern struct socketEntry *add_socketlist_entry( struct socketlist *list, int socketHandle );
extern void del_socketlist_entry( struct socketlist *list, struct socketEntry *pSocketEntry );
extern struct socketEntry *find_socketlist_entry_by_handle( struct socketlist *list, int socketHandle );
extern struct socketEntry *find_socketlist_entry_by_connType( struct socketlist *list, ccr_connection_type_t connType );

extern int getSocketListSize( struct socketlist *list );

#endif /* _LIST_H_ */
