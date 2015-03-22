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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xmlConfig.h"


static xmlConfigInfo_t xmlConfigInfo;

int xmlConfigInit(char *ConfigFileName)
{
    int     rc              = 0;

    memset(&xmlConfigInfo, 0, sizeof(xmlConfigInfo));

    xmlConfigInfo.doc = xmlReadFile(ConfigFileName, 0, 0);
    if(xmlConfigInfo.doc == 0)
    {
        return(-1);
    }

    // Get the root element node
    xmlConfigInfo.root_element = xmlDocGetRootElement(xmlConfigInfo.doc);
    if(xmlConfigInfo.root_element == 0)
    {
        return(-1);
    }
    // Validate entry
    if(xmlStrcmp(xmlConfigInfo.root_element->name, (const xmlChar *)"svsLib"))
    {
        return(-1);
    }

    return(rc);
}

int xmlConfigClose(void)
{
    int rc = 0;

    if(xmlConfigInfo.doc)
    {
        // free the document
        xmlFreeDoc(xmlConfigInfo.doc);
        // Free the global variables that may have been allocated by the parser.
        xmlCleanupParser();
    }

    return(rc);
}

xmlNodePtr xmlConfigNodeByNameFind(xmlNode *node, const char *nodeName)
{
    xmlNode *cur = 0;

    if(node == 0)
    {
        return(0);
    }

    if(nodeName == 0)
    {
        return(0);
    }

    cur = node->xmlChildrenNode;
    if(cur == 0)
    {
        return(0);
    }

    for(cur = node->xmlChildrenNode; cur; cur = cur->next)
    {
        if(cur->type == XML_ELEMENT_NODE)
        {
            if(xmlStrcmp(cur->name, (const xmlChar *)nodeName) == 0)
            {
                 return(cur);
            }
        }
    }
    return(0);
}

xmlNodePtr xmlConfigModuleNodeGet(const char *module)
{
    xmlNode *node;

    node = xmlConfigNodeByNameFind(xmlConfigInfo.root_element, "modules");
    node = xmlConfigNodeByNameFind(node, module);

    return(node);
}

int xmlConfigModuleParamStrGet(const char *module, const char *param, char *strDefaultParam, char *strParam, int strLen)
{
    int rc = 0;
    xmlNode *node = 0;
    xmlChar *str = 0;

    node = xmlConfigModuleNodeGet(module);
    node = xmlConfigNodeByNameFind(node, param);

    if(node)
    {
        str = xmlNodeListGetString(xmlConfigInfo.doc, node->xmlChildrenNode, 1);
        if(str)
        {
            strncpy(strParam, (const char *)str, strLen);
        }
        xmlFree(str);
    }

    if(node == 0 || str == 0)
    {   // param not found or not defined, use default if provided
        if(strDefaultParam)
        {
            strncpy(strParam, strDefaultParam, strLen);
        }
        else
        {
            rc = -1;
        }
    }

    return(rc);
}

int xmlConfigParamIntGet(const char *module, const char *param, int intDefaultParam, int *intParam)
{
    int rc;
    char str[80];

    // convert to string
    snprintf(str, sizeof(str), "%d", *intParam);

    rc = xmlConfigModuleParamStrGet(module, param, 0, str, sizeof(str));
    if(rc == -1)
    {
        *intParam = intDefaultParam;
    }
    else
    {
        *intParam = atoi(str);
    }

    return(0);
}

