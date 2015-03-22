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
#include "svsLog.h"

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



