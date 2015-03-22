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

#ifndef __CONFIG_H__
#define __CONFIG_H__


#define DFLT_CONFIG_FILE "/usr/scu/etc/cardreader.conf"
#define CONFIG_FILE "/usr/scu/etc/cardreader.conf"

#define DFLT_DATADIR "/home/bbick/pod"

#define DFLT_PODLOGLEVEL LOG_LEVEL_ERROR

#define CFG_SERIALPORT_BASE_NAME "SerialPortBaseName"
#define CFG_SERIALPORT_NUMBER "SerialPortNumber"

#define DFLT_SERIALPORT_BASE_NAME "/dev/ttyUSB0"
#define DFLT_SERIALPORT_BASE_NUMBER "1"

#define CFG_DEVELOPMENT_READER "DevelopmentReader"

#define CFG_LOGLEVEL_KEY "LogLevel"
#define DFLT_LOGLEVEL 0

#define CFG_DISP_DEBUG "DisplayDebug"
#define DFLT_DISP_DEBUG 0

#define CFG_SERVER_PORT "TCPServerPort"
#define DFLT_TCPSERVER_PORT 3357

#define SUCCESS 0


struct Configuration
{
    int nLogLevel;
    int bDevelopmentReader;
    int bDisplayDebugMsgs;
    int nTCPServerPort;

    char szSerialPortBaseName[32];
    char szSerialPortNumber[16];
};

extern struct Configuration theConfiguration;

void setDefaults(void);
int readConfig(void);
void updateConfiguration(void);

#endif

