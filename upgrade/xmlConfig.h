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

#ifndef XML_CONFIG_H
#define XML_CONFIG_H

#include <libxml/parser.h>
#include <libxml/tree.h>

#define CONFIG_MSG_PAYLOAD_MAX     1024

typedef struct
{
    uint8_t     flags;     // acknowledge request, ignore reply
} xmlMsgConfigHeader_t;

typedef struct
{
    xmlMsgConfigHeader_t  header;
    uint8_t               payload[CONFIG_MSG_PAYLOAD_MAX];
} xmlMsgConfig_t;

typedef struct
{
    xmlDoc  *doc;
    xmlNode *root_element;
} xmlConfigInfo_t;

int xmlConfigInit(char *ConfigFileName);
int xmlConfigClose(void);

int xmlConfigModuleParamStrGet(const char *module, const char *param, char *strDefaultParam, char *strParam, int strLen);
int xmlConfigParamIntGet(const char *module, const char *param, int intDefaultParam, int *intParam);

#endif // SVS_CONFIG_H


