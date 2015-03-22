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
 /*************************************

	Command.h

	RS-232C 8bit Protocol Card Reader
	Command Header File

**************************************/

#ifndef	_COMMAND_H_
#define	_COMMAND_H_

#include "cardrdrdatadefs.h"

// Define Functions

int CommandAuto(
	unsigned long	Port,
	unsigned long	Baudrate,
	int nCmdCode,
	unsigned char *Param,
	int *nDataSize);


void ReadTracks(
	unsigned long	Port,
	unsigned long	Baudrate
);

int initCardReader(unsigned long	Port, unsigned long	Baudrate);
int SetNewMasterKey(unsigned long	Port, unsigned long	Baudrate);
int authenticateReader(unsigned long	Port, unsigned long	Baudrate);
int ReadEncryptedTrack(unsigned long Port, unsigned long Baudrate,
                       unsigned char *trackData, int *dataLen,
                       int trackNumber);

int Read3Tracks(unsigned long Port, unsigned long Baudrate, ccr_trackData_t *ptrackData);


int ZapReaderLock(unsigned long Port, unsigned long Baudrate);
void resetReader(unsigned long Port, unsigned long Baudrate);
int getModelSerialNumber(unsigned long Port, unsigned long Baudrate, ccr_modelSerialNumber_t *ModelSerialNum);
int getFirmwareVersions(unsigned long Port, unsigned long Baudrate, ccr_VersionNumbers_t *firmwareVersions);

int intakeCommand(unsigned long Port, unsigned long Baudrate);
int withdrawCommand(unsigned long Port, unsigned long Baudrate);
int blinkGreenLED(unsigned long Port, unsigned long Baudrate);

int cancelCommand(unsigned long Port, unsigned long Baudrate);

#endif	// _COMMAND_H_

