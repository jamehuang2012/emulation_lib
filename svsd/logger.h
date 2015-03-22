/*
 * Copyright (C) 2011-2012 MapleLeaf Software, Inc
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
 * logger.h
 *
 * Dan Jozwiak: djozwiak@mlsw.biz
 *
 * Description: contains logging-related definitions and function prototypes
 * for applications that use the logger.
 */

#include <sys/queue.h>

#ifndef __LOGGER_H__
#define __LOGGER_H__

#define LEVEL_CRITICAL  (uint32_t)1
#define LEVEL_ERROR     (uint32_t)2
#define LEVEL_WARNING   (uint32_t)3
#define LEVEL_INFO      (uint32_t)4
#define LEVEL_DEBUG     (uint32_t)5

#define DEFAULT_LOG_LEVEL   LEVEL_INFO
#define DEFAULT_LOG_LIMIT   (4 * 1024 * 1024)

#define logHandle    void**

struct logfileEntry
{
    FILE *fp;
    char *filename;
    uint32_t log_level;
    uint32_t log_limit;

    TAILQ_ENTRY( logfileEntry ) next_entry;
};

struct logfileList
{
    int size;

    TAILQ_HEAD(, logfileEntry) head;
};

extern logHandle log_init(char *filename, uint32_t limit, uint32_t level);
extern void log_exit(logHandle handle);
extern void log_level(logHandle handle, uint32_t new_level);
extern void log_limit(logHandle handle, uint32_t new_limit);
extern void logme(logHandle handle, uint32_t debug_level, char *format, ...);

#endif /* __LOGGER_H__ */
