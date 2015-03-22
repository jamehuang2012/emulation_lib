/*
 * Copyright (C) 2009 MapleLeaf Software, Inc
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
*   xmlparser.c - XML Parser functions for swupgrade
*/

/**
 * xml parsing library
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "xmlparser.h"
#include "log.h"


/**********************************************************************
 * NAME:	void logMessage(char *name, uint8_t level, char *message)
 * DESCRIPTION: This function will send a log message to the server to
 *              process.
 *
 * Inputs:    uint8_t level - log level of message being sent
 *            char *message - message from application
 *            ... - x number of additional arguments
 * Outputs: None
 *********************************************************************/
void logMessage(uint8_t level, char *format, ...)
{
    void *buffer;

    char *message = NULL;
    char *argBuffer = NULL;
    unsigned int i;
    unsigned int j;
    unsigned int position;
    uint32_t bufferSize;
    uint32_t payloadSize;
    va_list argptr;

    /* allocate memory to store log message in */
    message = (char *)calloc(1, MAXLOGLEN);
    if (message == NULL)
    {
        /* Can't process message...no memory */
        return;
    }

    /* allocate memory for argument buffer */
    argBuffer = (char *)calloc(1, MAXLOGLEN);
    if (argBuffer == NULL)
    {
        /* Can't process message...no memory */
        free(message);
        return;
    }
    va_start(argptr, format);

    /* position in message array */
    position = 0;

    /* parse the incoming message arguments */
    for (i = 0; format[i] != '\0'; i++)
    {
        /* % is a token */
        if (format[i] == '%')
        {
            i++;
            switch(format[i])
            {
                /* Argument is a char */
                case 'c':
                    sprintf(argBuffer, "%c", va_arg(argptr, int));
                    break;

                /* Argument is type char* */
                case 's':
                    strcpy(argBuffer, va_arg(argptr, char *));
                    break;

                /* Argument is type int */
                case 'd':
                    sprintf(argBuffer, "%d", va_arg(argptr, int));
                    break;

                /* Argument is type short and unsigned short */
                case 'h':
                    if (format[i+1] == 'u')
                    {
                        sprintf(argBuffer, "%hu", va_arg(argptr, unsigned int));
                    }
                    else
                    {
                        sprintf(argBuffer, "%hd", va_arg(argptr, int));
                    }
                    break;

                /* Argument is type long and unsigned long */
                case 'l':
                    if (format[i+1] == 'u')
                    {
                        sprintf(argBuffer, "%lu", va_arg(argptr, unsigned long));
                    }
                    else
                    {
                        sprintf(argBuffer, "%ld", va_arg(argptr, long));
                    }
                    break;

                /* Argument is type float|double */
                case 'f':
                    sprintf(argBuffer, "%f", va_arg(argptr, double));
                    break;

                case 'p':
                    sprintf(argBuffer, "%p", va_arg(argptr, void *));
                    break;

                /* Argument is type unknown...
                 * no error processing, just skip it
                 */
                default:
                    break;
            }

            /* Copy the contents of the argument buffer into the message */
            for (j = 0; j < strlen(argBuffer); j++)
            {
                message[position] = argBuffer[j];
                position++;
            }
        }
        else
        {
            message[position] = format[i];
            position++;
        }
    }


    message[position] = '\0';

    /* adding one more to position so buffer size is calculated properly */
    position++;

    va_end(argptr);

    /* free the memory used to retrieve the arguments */
    free(argBuffer);

    payloadSize = sizeof(struct LogPayload) + position;
    bufferSize = sizeof(struct Header) + payloadSize;

    buffer = (void *)calloc(1, bufferSize);
    if (buffer == NULL)
    {
        /* Can't process message...no memory */
        free(message);
        return;
    }

printf("Log: Level: %d %s", level, message);

    /* free the memory */
    free(buffer);
    free(message);
}







/***********************************************************
 * NAME: getNodeByName
 * DESCRIPTION: This function loops through a tree
 *              and looks for a given node matching the
 *              specified name. It assumes that the parent
 *              passed in is the start of a tree.
 *
 * IN:  xmlNodePtr,
 *      char *
 *
 * OUT: none
 ***********************************************************/
xmlNodePtr getNodeByName(xmlNodePtr parent, char * name)
{
    xmlNodePtr nodePtr;
    if (parent == NULL || name == NULL)
        return NULL;

    for (nodePtr = parent->children; nodePtr != NULL; nodePtr = nodePtr->next)
    {
        if (nodePtr->name && (strcmp((char*)nodePtr->name,name) == 0))
        {
            return nodePtr;
        }
    }
    return NULL;
}

/***********************************************************
 * NAME: getNodeContentByName
 * DESCRIPTION: This function loops through a tree
 *              and looks for a given node matching the
 *              specified name.  It allocates a buffer
 *              for the return value of the nodes
 *              content.
 *
 *              CALLER MUST FREE MEMORY
 *
 * IN:  xmlNodePtr,
 *      char *
 *
 * OUT: char *
 ***********************************************************/
char *getNodeContentByName(xmlNodePtr parent, char* name)
{
    char *retVal = NULL;
    xmlNodePtr node;

    if (!parent || !name)
        return retVal;

    node = getNodeByName(parent, name);
    if (node)
    {
        retVal = (char*)xmlNodeGetContent(node);
    }

    return retVal;
}


