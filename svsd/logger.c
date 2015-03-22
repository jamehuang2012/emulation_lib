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
 * logger.c
 *
 * Dan Jozwiak: djozwiak@mlsw.biz
 *
 * Description: This file contains functions that manage a log file for the
 * software component that uses it. Note that this is not a centralized logging
 * mechanism. Each component that uses this code will have its own log file.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "logger.h"

struct logfileList filelist;
static uint8_t list_init = 0;

static void init_list(struct logfileList *list);
static void del_list_entry(struct logfileList *list, struct logfileEntry *pLogfileEntry);
static struct logfileEntry *add_list_entry(struct logfileList *list, FILE *fp, char *filename, uint32_t log_level, uint32_t log_limit);

/***********************************************************
 * NAME: level_to_str
 * DESCRIPTION: utility to convert an enumeration to a string.
 *
 * IN:  debug level
 * OUT: string representation of the debug level
 ***********************************************************/
static char *level_to_str(uint8_t debug_level)
{
    char *level = NULL;

    switch (debug_level)
    {
    case LEVEL_CRITICAL:
        level = "CRITICAL";
        break;
    case LEVEL_ERROR:
        level = "ERROR";
        break;
    case LEVEL_WARNING:
        level = "WARNING";
        break;
    case LEVEL_INFO:
        level = "INFO";
        break;
    case LEVEL_DEBUG:
        level = "DEBUG";
        break;
    default:
        level = "UNKNOWN";
        break;
    }
    return level;
}

/***********************************************************
 * NAME: log_init
 * DESCRIPTION: this function opens up the file associated
 * with the specified name and sets the limit (maximum size
 * allowed for the log file) and the log level (lowest level
 * allowed for log messages to be written). If no file is
 * specified or if the open fails then all messages will go
 * to stdout. Note that all data (file descriptor, file name,
 * default limit and level are maintained locally so that
 * applications don't have to manage the data.
 *
 * IN:  log file name, log limit and lowest log level
 * OUT: file descriptor to the log file
 ***********************************************************/
logHandle log_init(char *filename, uint32_t limit, uint32_t level)
{
    struct logfileEntry *pLogfileEntry = NULL;

    if (list_init == 0)
    {
        init_list(&filelist);
        list_init = 1;
    }

    if (filename != NULL)
    {
        FILE *file = fopen(filename, "a");
        FILE *logfp = stdout;

        if (file)
        {
            logfp = file;
        }

        pLogfileEntry = add_list_entry(&filelist, logfp, filename, level, limit);
    }

    if (!pLogfileEntry)
        return NULL;

    return (logHandle)pLogfileEntry;
}

/***********************************************************
 * NAME: log_exit
 * DESCRIPTION: close out the log file descriptor.
 *
 * IN:  none
 * OUT: none
 ***********************************************************/
void log_exit(logHandle handle)
{
    struct logfileEntry *pLogfileEntry = (struct logfileEntry *)handle;

    if (pLogfileEntry != NULL)
    {
        if (pLogfileEntry->fp != stdout)
        {
            fclose(pLogfileEntry->fp);
        }

        del_list_entry(&filelist, pLogfileEntry);
    }
}

/***********************************************************
 * NAME: log_level
 * DESCRIPTION: function that allows an application to change
 * the log level after log initialization (during program
 * operation).
 *
 * IN:  new log level
 * OUT: none
 ***********************************************************/
void log_level(logHandle handle, uint32_t new_level)
{
    struct logfileEntry *pLogfileEntry = (struct logfileEntry *)handle;

    if (pLogfileEntry != NULL)
    {
        pLogfileEntry->log_level = new_level;
    }
}

/***********************************************************
 * NAME: log_limit
 * DESCRIPTION: function that allows an application to change
 * the log limit after log initialization (during program
 * operation).
 *
 * IN:  new log limit
 * OUT: none
 ***********************************************************/
void log_limit(logHandle handle, uint32_t new_limit)
{
    struct logfileEntry *pLogfileEntry = (struct logfileEntry *)handle;

    if (pLogfileEntry != NULL)
    {
        pLogfileEntry->log_limit = new_limit;
    }
}

/***********************************************************
 * NAME: manage_log_file
 * DESCRIPTION: this function checks the current size of the
 * log file and compares it agains the limit. If the limit
 * has been reached the file is closed, saved to a backup
 * file and a new file opened. Note that only one backup is
 * allowed.
 *
 * IN:  none
 * OUT: none
 ***********************************************************/
static void manage_log_file(struct logfileEntry *pLogfileEntry)
{
    struct stat info;
    int status;
    char backup[100];

    if (pLogfileEntry->fp == stdout)
        return;

    status = stat(pLogfileEntry->filename, &info);
    if (!status)
    {
        if (info.st_size < pLogfileEntry->log_limit)
            return;
    }

    fclose(pLogfileEntry->fp);
    pLogfileEntry->fp = stdout;

    sprintf(backup, "%s.0", pLogfileEntry->filename);
    status = rename(pLogfileEntry->filename, backup);
    if (status < 0)
    {
        fprintf(stdout, "Error: rename() returned %d\n", status);
    }

    if (pLogfileEntry->filename != NULL)
    {
        FILE *file = fopen(pLogfileEntry->filename, "a");

        if (file)
        {
            pLogfileEntry->fp = file;
        }
    }

    return;
}

/***********************************************************
 * NAME: logme
 * DESCRIPTION: this function logs the specified message
 * based on the current logging configuration. If the
 * specified level is greater then the log level as set by
 * the application the message is just dropped. Otherwise
 * a timestamp and the log level are prepended to the
 * message and the message is written to the log file.
 *
 * IN:  id of logfile to store message in
 *      debug level for this message
 *      message format and subsequent parameters
 * OUT: none
 ***********************************************************/
void logme(logHandle handle, uint32_t debug_level, char *format, ...)
{
    va_list args;
    struct timeval tv;
    char *time;
    struct logfileEntry *pLogfileEntry = (struct logfileEntry *)handle;

    if (debug_level > pLogfileEntry->log_level)
        return;

    manage_log_file(pLogfileEntry);

    gettimeofday(&tv, NULL);
    time = ctime(&tv.tv_sec);
    time[strlen(time) - 1] = 0;

    fprintf(pLogfileEntry->fp, "%s: %s:", time, level_to_str(debug_level));
    va_start(args, format);
    vfprintf(pLogfileEntry->fp, format, args);
    va_end(args);
    fprintf(pLogfileEntry->fp, "\n");

    fflush(pLogfileEntry->fp);
}

/*********************************************************
 * NAME: add_list_entry
 * DESCRIPTION: Add a Logfile Entry to the list using the
 *              file pointer.
 *
 * IN: list pointer to Logfile List structure.
 *     fp to be added.
 *
 * OUT: None
 *
 **********************************************************/
static struct logfileEntry *add_list_entry(
                struct logfileList *list,
                FILE *fp, char *filename, uint32_t log_level, uint32_t log_limit)
{
    struct logfileEntry *pLogfileEntry;
    pLogfileEntry = (struct logfileEntry *)calloc(1, sizeof(struct logfileEntry));
    if (!pLogfileEntry)
    {
        return NULL;
    }

    // Init to unopened value
    pLogfileEntry->fp = fp;
    pLogfileEntry->log_level = log_level;
    pLogfileEntry->log_limit = log_limit;

    pLogfileEntry->filename = (char *)malloc(strlen(filename) + 1);
    strcpy(pLogfileEntry->filename, filename);

    TAILQ_INSERT_TAIL(&list->head, pLogfileEntry, next_entry);
    list->size++;

    return pLogfileEntry;
}

/*********************************************************
 * NAME: del_list_entry
 * DESCRIPTION: Removes Socket Entry from list.
 *
 * IN: list pointer to file list.
 *     pLogfileEntry pointer to Socket Entry to remove.
 * OUT: Nothing.
 *
 **********************************************************/
static void del_list_entry(struct logfileList *list, struct logfileEntry *pLogfileEntry)
{
    TAILQ_REMOVE(&list->head, pLogfileEntry, next_entry);
    list->size--;
    free(pLogfileEntry);
}

/*********************************************************
 * NAME: init_list
 * DESCRIPTION: Initializes the Logfile List
 *
 * IN: list pointer to Logfile List.
 * OUT: Nothing.
 **********************************************************/
static void init_list(struct logfileList *list)
{
    memset(list, 0, sizeof(struct logfileList));
    TAILQ_INIT(&list->head);
}
