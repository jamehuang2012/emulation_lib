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

 /**************************************************************************

	Command.c

	RS-232C 8bit Protocol Card Reader
	Command

***************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <logger.h>


#include "Command.h"
#include "DLE_EOT.h"

#include "crypto.h"
#include "config.h"
#include "Prtcl_RS232C_8.h"
#include "RS232C_Def.h"
#include "RS232C_UserInterface_Proc.h"
// How long to wait for Intake and Withdraw commands
#define CCR_INTAKE_WITHDRAW_TIMEOUT_MSEC 5000

// Commands
#define CMD_INIT                        1
#define CMD_INTAKE                      2
#define CMD_WITHDRAW                    3
#define CMD_READTRACK                   4
#define CMD_LED                         5
#define CMD_IMPORT_CUSTOMER_MASTER_KEY  6

#define CMD_SEND_KeyKe                  7
#define CMD_SEND_KeyKwa                 8
#define CMD_GET_CHALLENGE               9
#define CMD_DEV_AUTH                    10
#define CMD_MAG_DATA_KE                 11
#define CMD_MAG_DATA_KEY                12
#define CMD_MAG_TRACK1_READ             13
#define CMD_MAG_TRACK2_READ             14
#define CMD_MAG_TRACK3_READ             15

#define CMD_INFO_SN                     16
#define CMD_ZAP_READER_LOCK             17
#define CMD_INFO_MODEL                  18
#define CMD_RESET                       19

#define CMD_LED_YEL_BLINK               20

#define CMD_GET_SUPER_FW_VERS           21
#define CMD_GET_USER_FW_VERS            22
#define CMD_GET_EMV2000_FW_VERS         23
#define CMD_CANCEL_COMMAND              24

#define CMD_GET_SECCPU_SUPER_FW_VERS    25
#define CMD_GET_SECCPU_USER_FW_VERS     26

#define CANCEL_COMMAND                  "10"
#define CANCEL_COMMAND_PARAM            "04"

#define GET_FW_VERSION_CMD              "41"
#define GET_SECCPU_FW_VERSION_CMD       "61"

// Parameter bytes for both FW_VERSION and SECCPU_
#define GET_SUPER_FW_VERS               "30"
#define GET_USER_FW_VERS                "31"
#define GET_EMV2000_FW_VERS             "32"


#define RESET_READER_CMD                "7A"
#define RESET_READER_PARAM              "30"

#define INFO_MODEL_CMD                  "36"
#define INFO_MODEL_PARAM                "45"

#define INFO_SN_CMD                     "36"
#define INFO_SN_CMD_PARAM               "41"

#define MAG_TRACK_READ_CMD              "32"
#define MAG_TRACK1_READ_CMD_PARAM       "41"
#define MAG_TRACK2_READ_CMD_PARAM       "42"
#define MAG_TRACK3_READ_CMD_PARAM       "43"

#define KEY_KE_CMD                      "47"
#define KEY_KE_PARAM                    "31"

#define KEY_KWA_CMD                     "47"
#define KEY_KWA_PARAM                   "32"

#define GET_CHALLENGE_CMD               "47"
#define GET_CHALLENGE_CMD_PARAM         "37"

#define DEV_AUTH_CMD                    "47"
#define DEV_AUTH_CMD_PARAM              "38"

#define KEY_MAG_DATA_KE_CMD             "47"
#define KEY_MAG_DATA_KE_PARAM           "35"

#define MAG_DATA_KEY_CMD                "47"
#define MAG_DATA_KEY_CMD_PARAM          "36"

#define TAMPER_DETECT_RECOVER_CMD "3D"
#define TAMPER_DETECT_RECOVER_PARAM "3F"

#define IMPORT_CUSTOMER_MASTER_KEY_CMD_STR "47"
#define IMPORT_CUSTOMER_MASTER_KEY_PARAM "40"

#define INIT_CMD "30"
#define INIT_PARAM "40"
#define INIT_DATA "2"

#define LED_CMD "33"
// pm value for LED off
#define LED_OFF "30"
// No Flash is default if Ft is not sent.
#define LED_FT_NO_FLASH "30"
// pm value for Green
#define LED_GREEN_ON "32"
// pm value for Yellow (cheat for now go Green)
#define LED_YELLOW_ON  "32"

// Blink at .5 seconds
#define LED_FT_YEL_BLINK_FAST  "34"


#define INTAKE_CMD "31"
#define INTAKE_PARAM "31"

#define WITHDRAW_CMD "31"
#define WITHDRAW_PARAM "32"

#define READTRK_CMD "32"
#ifdef USE_3DES
    #define READTRK_1_PARAM "41"
    #define READTRK_2_PARAM "42"
    #define READTRK_3_PARAM "43"
#else
    #define READTRK_1_PARAM "31"
    #define READTRK_2_PARAM "32"
    #define READTRK_3_PARAM "33"
#endif




/********************************************************
*   New command function to work with more automated operations
*   such as authentication, track read, etc.
*
*********************************************************/
int CommandAuto(
	unsigned long	Port,
	unsigned long	Baudrate,
	int nCmdCode,
	unsigned char *Param,
	int *nDataSize)
{
	long		Ret ;
	int         nRetVal = 0;
	COMMAND		Command_Val;
	REPLY		Reply_Val;
Baudrate = 1;
	unsigned char	TempCommandCode[3];
	unsigned long	TempCommandCodeSize;

	unsigned char	TempParameterCode[3];
	unsigned long	TempParameterCodeSize;

	unsigned char	TempSendData[MAX_DATA_ARRAY_SIZE];
	unsigned char	SendData[MAX_DATA_ARRAY_SIZE];

    int nTimeout = 10000; // Default timeout of 10 seconds.


	*nDataSize = 0;

	// Buffer Clear
	bzero( TempSendData, sizeof( TempSendData ) );
	bzero( SendData, sizeof( SendData ) );
	bzero( &Reply_Val, sizeof( REPLY ) );

	// Command Code
    switch(nCmdCode)
    {
        case CMD_INIT:
                    strcpy( (char *)TempCommandCode, INIT_CMD);
                    break;

        case CMD_INTAKE:
                    strcpy( (char *)TempCommandCode, INTAKE_CMD);
                    break;

        case CMD_READTRACK:
                    strcpy( (char *)TempCommandCode, READTRK_CMD);
                    break;

        case CMD_WITHDRAW:
                    strcpy( (char *)TempCommandCode, WITHDRAW_CMD);
                    break;

        case CMD_LED:
                    strcpy( (char *)TempCommandCode, LED_CMD);
                    break;

        case CMD_IMPORT_CUSTOMER_MASTER_KEY:
                    strcpy( (char *)TempCommandCode, IMPORT_CUSTOMER_MASTER_KEY_CMD_STR);
                    break;

        case CMD_SEND_KeyKe:
                    strcpy( (char *)TempCommandCode, KEY_KE_CMD);
                    break;

        case CMD_SEND_KeyKwa:
                    strcpy( (char *)TempCommandCode, KEY_KWA_CMD);
                    break;

        case CMD_GET_CHALLENGE:
                     strcpy( (char *)TempCommandCode, GET_CHALLENGE_CMD);
                   break;

        case CMD_DEV_AUTH:
                     strcpy( (char *)TempCommandCode, DEV_AUTH_CMD);
                   break;

        case CMD_MAG_DATA_KE:
                    strcpy( (char *)TempCommandCode, KEY_MAG_DATA_KE_CMD);
                    break;

        case CMD_MAG_DATA_KEY:
                    strcpy( (char *)TempCommandCode, MAG_DATA_KEY_CMD);
                    break;

        case CMD_MAG_TRACK1_READ:
                    strcpy( (char *)TempCommandCode, MAG_TRACK_READ_CMD);
                    break;

        case CMD_MAG_TRACK2_READ:
                    strcpy( (char *)TempCommandCode, MAG_TRACK_READ_CMD);
                    break;

        case CMD_MAG_TRACK3_READ:
                    strcpy( (char *)TempCommandCode, MAG_TRACK_READ_CMD);
                    break;

        case CMD_INFO_SN:
                    strcpy( (char *)TempCommandCode, INFO_SN_CMD);
                    break;

        case CMD_ZAP_READER_LOCK:
                    strcpy( (char *)TempCommandCode, TAMPER_DETECT_RECOVER_CMD);
                    break;

        case CMD_INFO_MODEL:
                    strcpy( (char *)TempCommandCode, INFO_MODEL_CMD);
                    break;

        case CMD_RESET:
                    strcpy((char *)TempCommandCode, RESET_READER_CMD);
                    break;

        case CMD_GET_SUPER_FW_VERS:
        case CMD_GET_USER_FW_VERS:
        case CMD_GET_EMV2000_FW_VERS:
                    strcpy((char *)TempCommandCode, GET_FW_VERSION_CMD);
                    break;

        case CMD_GET_SECCPU_SUPER_FW_VERS:
        case CMD_GET_SECCPU_USER_FW_VERS:
                    strcpy((char *)TempCommandCode, GET_SECCPU_FW_VERSION_CMD);
                    break;



        case CMD_LED_YEL_BLINK:
                    strcpy((char *)TempCommandCode, LED_CMD);
                    break;

        case CMD_CANCEL_COMMAND:
                    strcpy((char *)TempCommandCode, CANCEL_COMMAND);
                    break;

    }
    // Convert the command to binary
	MakeBinaryData( TempCommandCode, 2, &Command_Val.CommandCode, &TempCommandCodeSize );

	// Parameter Code
    switch(nCmdCode)
    {
        case CMD_INIT:
                    strcpy( (char *)TempParameterCode, INIT_PARAM);
                    break;

        case CMD_INTAKE:
                    strcpy( (char *)TempParameterCode, INTAKE_PARAM);
                    break;

        case CMD_READTRACK:
                    strcpy( (char *)TempParameterCode, (char *)Param);
                    break;

        case CMD_WITHDRAW:
                    strcpy( (char *)TempParameterCode, WITHDRAW_PARAM);
                    break;

        case CMD_LED:
                    strcpy( (char *)TempParameterCode, LED_GREEN_ON);
                    break;

        case CMD_IMPORT_CUSTOMER_MASTER_KEY:
                    strcpy( (char *)TempParameterCode, IMPORT_CUSTOMER_MASTER_KEY_PARAM);
                    break;

        case CMD_SEND_KeyKe:
                    strcpy( (char *)TempParameterCode, KEY_KE_PARAM);
                    break;

        case CMD_SEND_KeyKwa:
                    strcpy( (char *)TempParameterCode, KEY_KWA_PARAM);
                    break;

        case CMD_GET_CHALLENGE:
                    strcpy( (char *)TempParameterCode, GET_CHALLENGE_CMD_PARAM);
                    break;

        case CMD_DEV_AUTH:
                    strcpy( (char *)TempParameterCode, DEV_AUTH_CMD_PARAM);
                    break;

        case CMD_MAG_DATA_KE:
                    strcpy( (char *)TempParameterCode, KEY_MAG_DATA_KE_PARAM);
                    break;

        case CMD_MAG_DATA_KEY:
                    strcpy( (char *)TempParameterCode, MAG_DATA_KEY_CMD_PARAM);
                    break;

        case CMD_MAG_TRACK1_READ:
                    strcpy( (char *)TempParameterCode, MAG_TRACK1_READ_CMD_PARAM);
                    break;

        case CMD_MAG_TRACK2_READ:
                    strcpy( (char *)TempParameterCode, MAG_TRACK2_READ_CMD_PARAM);
                    break;

        case CMD_MAG_TRACK3_READ:
                    strcpy( (char *)TempParameterCode, MAG_TRACK3_READ_CMD_PARAM);
                    break;

        case CMD_INFO_SN:
                    strcpy( (char *)TempParameterCode, INFO_SN_CMD_PARAM);
                    break;

        case CMD_ZAP_READER_LOCK:
                    strcpy( (char *)TempParameterCode, TAMPER_DETECT_RECOVER_PARAM);
                    break;

        case CMD_INFO_MODEL:
                    strcpy((char *)TempParameterCode, INFO_MODEL_PARAM);
                    break;

        case CMD_RESET:
                    strcpy((char *)TempParameterCode, RESET_READER_PARAM);
                    break;

        case CMD_GET_SUPER_FW_VERS:
                    strcpy((char *)TempParameterCode, GET_SUPER_FW_VERS);
                    break;

        case CMD_GET_USER_FW_VERS:
                    strcpy((char *)TempParameterCode, GET_USER_FW_VERS);
                    break;

        case CMD_GET_SECCPU_SUPER_FW_VERS:
                    strcpy((char *)TempParameterCode, GET_SUPER_FW_VERS);
                    break;

        case CMD_GET_SECCPU_USER_FW_VERS:
                    strcpy((char *)TempParameterCode, GET_USER_FW_VERS);
                    break;

        case CMD_GET_EMV2000_FW_VERS:
                    strcpy((char *)TempParameterCode, GET_EMV2000_FW_VERS);
                    break;

        case CMD_LED_YEL_BLINK:
                    strcpy((char *)TempParameterCode, LED_YELLOW_ON);
                    break;

        case CMD_CANCEL_COMMAND:
                    strcpy((char *)TempParameterCode, CANCEL_COMMAND_PARAM);
                    break;

    }
    // Convert the parameter code to binary
	MakeBinaryData( TempParameterCode, 2, &Command_Val.ParameterCode, &TempParameterCodeSize );
	// Data

	Command_Val.Data.Size = 0;

    switch(nCmdCode)
    {
        case CMD_INIT:
                    // One data byte '2' associated
                    sprintf((char *)TempSendData, INIT_DATA);
                    Command_Val.Data.Size = strlen((char *)TempSendData);
                    Command_Val.Data.pBody = TempSendData;
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Initialize command: %c %c %s\n", Command_Val.CommandCode, Command_Val.ParameterCode,
                               Command_Val.Data.pBody);
#endif
                    }
                    break;

        case CMD_INTAKE:
                    {
                        // We want to have 8 bytes of '0'
                        // Note: Now we will take the timeout from the *Param passed in
                        unsigned int nTimeout = *(int *)Param;
                        sprintf((char *)TempSendData, "%08u", nTimeout);

                        Command_Val.Data.Size = strlen((char *)TempSendData);
                        Command_Val.Data.pBody = TempSendData;
                        nTimeout = 0;
                        if(theConfiguration.bDisplayDebugMsgs)
                        {
#ifdef PRINT_MSGS
                            printf( "Sending Intake command: %c %c %s\n", Command_Val.CommandCode, Command_Val.ParameterCode,
                                   Command_Val.Data.pBody);
#endif
                        }
                    }
                    break;

        case CMD_READTRACK:
                    // No data associated with read track request
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending ReadTrack command: %c %c\n", Command_Val.CommandCode, Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_WITHDRAW:
                    {
                        // We want to have 8 bytes of '0'
                        unsigned int nTimeout = *(int *)Param;
                        sprintf((char *)TempSendData, "%08u", nTimeout);
                        Command_Val.Data.Size = strlen((char *)TempSendData);
                        Command_Val.Data.pBody = TempSendData;
                        nTimeout = 0;
                        if(theConfiguration.bDisplayDebugMsgs)
                        {
#ifdef PRINT_MSGS
                            printf( "Sending Withdraw command: %c %c %s\n", Command_Val.CommandCode, Command_Val.ParameterCode,
                                Command_Val.Data.pBody);
#endif
                        }
                    }
                    break;

        case CMD_LED:
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending LED Green On command: %c %c\n", Command_Val.CommandCode, Command_Val.ParameterCode);
#endif
                    }
                    // No data needs to be setup No Ft for Green LED solid on
                    break;

        case CMD_LED_YEL_BLINK:
                    // Need to set Ft data value
                        Command_Val.Data.Size = sizeof(char);
                        Command_Val.Data.pBody = Param;  // '4'
                    break;

        case CMD_IMPORT_CUSTOMER_MASTER_KEY:
                    // 16 bytes of binary data associated with this command
				// Set Data
                    Command_Val.Data.Size = 16; // Key size
                    Command_Val.Data.pBody = Param; // Encrypted Master Key data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Import Customer Master Key command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_SEND_KeyKe:
                    Command_Val.Data.Size = 16; // Key size
                    Command_Val.Data.pBody = Param; // Encrypted Master Key data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Set Ke Key command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_SEND_KeyKwa:
                    Command_Val.Data.Size = 16; // Key size
                    Command_Val.Data.pBody = Param; // Encrypted Master Key data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Set Kwa Key command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_GET_CHALLENGE:
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Get Challenge command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_DEV_AUTH:
                    Command_Val.Data.Size = 32; // Dev Auth buffer
                    Command_Val.Data.pBody = Param; // Encrypted Dev Auth data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Device Authentication command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_MAG_DATA_KE:
                    Command_Val.Data.Size = 16; // Key size
                    Command_Val.Data.pBody = Param; // Encrypted Mag Data Ke Key data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Set Mag Data Ke Key command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_MAG_DATA_KEY:
                    Command_Val.Data.Size = 24; // Key size
                    Command_Val.Data.pBody = Param; // Encrypted Mag Data Key + IV data
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf( "Sending Mag Data Key command: %c %c\n", Command_Val.CommandCode,
                                                                                        Command_Val.ParameterCode);
#endif
                    }
                    break;

        case CMD_MAG_TRACK1_READ:
        case CMD_MAG_TRACK2_READ:
        case CMD_MAG_TRACK3_READ:
                    // No data to send with read command will return data (hopefully)
                    break;

        case CMD_INFO_SN:
        case CMD_INFO_MODEL:
        case CMD_RESET:
        case CMD_GET_SUPER_FW_VERS:
        case CMD_GET_USER_FW_VERS:
        case CMD_GET_SECCPU_SUPER_FW_VERS:
        case CMD_GET_SECCPU_USER_FW_VERS:
        case CMD_GET_EMV2000_FW_VERS:
        case CMD_CANCEL_COMMAND:
                    // No data to send with Serial Number, Model number and Reset commands
                    break;

        case CMD_ZAP_READER_LOCK:
                    if(theConfiguration.bDisplayDebugMsgs)
                    {
#ifdef PRINT_MSGS
                        printf("Sending Tamper Detect Recover Command\n");
#endif
                    }
                    break;
    }


	Ret = RS232C_8_ExecuteCommand(
				Port,
				&Command_Val,
//				0,
//				5000,
				nTimeout, // Timeout 10 seconds, may need to set to other for Intake and Withdraw
				&Reply_Val
				);

 	/* if ACK time out , we just return error , instead of return 0 */
    if (_RS232C_8_ACK_TIMEOUT == Ret || _RS232C_8_NAK_RECEIVE_OVERTIMES == Ret) {
		return Ret;
	} 
    if( _RS232C_8_NO_ERROR != Ret)
    {
#ifdef PRINT_MSGS
        printf( "\nRS8_ExecuteCommand Returned %X\n", (unsigned int)Ret );
#endif
        Ret = (Ret + 10000) * -1; // Make a negative error code
    }

	if( _RS232C_8_NO_ERROR == Ret )
	{
#ifdef PRINT_MSGS
        if(theConfiguration.bDisplayDebugMsgs)
        {
            ShowReplyType( Reply_Val.replyType );
            printf("\n" );
        }
#endif
	}

	if( _RS232C_8_NO_ERROR == Ret)
	{
		if( PositiveReply == Reply_Val.replyType )
		{
#ifdef PRINT_MSGS
            if(theConfiguration.bDisplayDebugMsgs)
            {
                printf( "CommandCode   : %X\n", Reply_Val.message.positiveReply.CommandCode );
                printf( "ParameterCode : %X\n", Reply_Val.message.positiveReply.ParameterCode );
                printf( "StatusCode1   : %X\n", Reply_Val.message.positiveReply.StatusCode.St1 );
                printf( "StatusCode0   : %X\n", Reply_Val.message.positiveReply.StatusCode.St0 );
            }
#endif
            /*
			* Convert from ASCII number
			* to decimal status value
			*/
			nRetVal = Reply_Val.message.positiveReply.StatusCode.St1 - '0';
			nRetVal *= 10;
			nRetVal += Reply_Val.message.positiveReply.StatusCode.St0 - '0';

			if( 0 != Reply_Val.message.positiveReply.Data.Size )
			{
			    *nDataSize = Reply_Val.message.positiveReply.Data.Size;
			    switch(nCmdCode)
			    {
                    case CMD_GET_CHALLENGE:
                            memcpy(Param, Reply_Val.message.positiveReply.Data.Body,
                                   Reply_Val.message.positiveReply.Data.Size);
                            break;

                    case CMD_DEV_AUTH:
                            memcpy(Param, Reply_Val.message.positiveReply.Data.Body,
                                   Reply_Val.message.positiveReply.Data.Size);
                            break;

                    case CMD_MAG_TRACK1_READ:
                    case CMD_MAG_TRACK2_READ:
                    case CMD_MAG_TRACK3_READ:
                            memcpy(Param, Reply_Val.message.positiveReply.Data.Body,
                                   Reply_Val.message.positiveReply.Data.Size);
                            break;

                    case CMD_INFO_SN:
                    case CMD_INFO_MODEL:
                    case CMD_GET_SUPER_FW_VERS:
                    case CMD_GET_USER_FW_VERS:
                    case CMD_GET_SECCPU_SUPER_FW_VERS:
                    case CMD_GET_SECCPU_USER_FW_VERS:
                    case CMD_GET_EMV2000_FW_VERS:
                            memcpy(Param, Reply_Val.message.positiveReply.Data.Body,
                                   Reply_Val.message.positiveReply.Data.Size);
                            break;
			    }

#ifdef PRINT_MSGS
                if(theConfiguration.bDisplayDebugMsgs)
                {
                    printf("\nHEX :\n" );
                    DisplayData(
                        _HEX,
                        Reply_Val.message.positiveReply.Data.Body,
                        Reply_Val.message.positiveReply.Data.Size
                        );

                    printf("\n\nASCII :\n" );
                    DisplayData(
                        _ASCII,
                        Reply_Val.message.positiveReply.Data.Body,
                        Reply_Val.message.positiveReply.Data.Size
                        );
                }
#endif
			}
			else
			{
#ifdef PRINT_MSGS
                if(theConfiguration.bDisplayDebugMsgs)
                {
                    printf("\nNo Data\n" );
                }
#endif
			}
		}
		else if( NegativeReply == Reply_Val.replyType )
		{
#ifdef PRINT_MSGS
            if(theConfiguration.bDisplayDebugMsgs)
            {
                printf( "Command Code   : %X\n", Reply_Val.message.negativeReply.CommandCode );
                printf( "Parameter Code : %X\n", Reply_Val.message.negativeReply.ParameterCode );
                printf( "ErrorCode1     : %X\n", Reply_Val.message.negativeReply.ErrorCode.E1 );
                printf( "ErrorCode0     : %X\n", Reply_Val.message.negativeReply.ErrorCode.E0 );
            }
#endif

			/*
			* Convert from ASCII number
			* to decimal error value
			*/
			nRetVal = Reply_Val.message.negativeReply.ErrorCode.E1 - '0';
			nRetVal *= 10;
			nRetVal += Reply_Val.message.negativeReply.ErrorCode.E0 - '0';
            nRetVal *= -1;

#ifdef PRINT_MSGS
			if( 0 != Reply_Val.message.negativeReply.Data.Size )
			{
                printf("\nHEX :\n" );
                DisplayData(
                    _HEX,
                    Reply_Val.message.negativeReply.Data.Body,
                    Reply_Val.message.negativeReply.Data.Size
                    );

                printf("\n\nASCII :\n" );
                DisplayData(
                    _ASCII,
                    Reply_Val.message.negativeReply.Data.Body,
                    Reply_Val.message.negativeReply.Data.Size
                    );
			}
			else
			{
                if(theConfiguration.bDisplayDebugMsgs)
                {
                    printf("\nNo Data\n" );
                }
			}
#endif
		}
	}

#ifdef PRINT_MSGS
    if(theConfiguration.bDisplayDebugMsgs)
    {
        printf("\n" );
    }
#endif
	return nRetVal;
}

void resetReader(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;
    int nDataSize = 0;

    nRetVal = CommandAuto( Port, Baudrate, CMD_RESET, (unsigned char *)"0", &nDataSize);
#ifdef PRINT_MSGS
    if(nRetVal < 0)
    {
        printf("Reset Reader Cmd Returned Error: %d\n", abs(nRetVal));
		return;
    }
    else
    {
        printf("Card Reader Reset.  When LED blinks Green\n"
               "Issue the Authenticate, or other appropriate\n"
               "command again.\n");
        printf("Press Enter Key to continue\n");
        getchar();
    }
#endif
}

int initCardReader(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal = 0;
    int nDataSize = 0;
    unsigned char SerialNumber[32];
    unsigned char ModelNumber[128];
    memset(SerialNumber, 0, sizeof(SerialNumber));
    memset(ModelNumber, 0, sizeof(ModelNumber));


    nRetVal = CommandAuto( Port, Baudrate, CMD_INIT, (unsigned char *)"0", &nDataSize);
    if(nRetVal < 0)
    {
        ALOGE("CCR","Initialize Cmd Returned Error: %d\n", abs(nRetVal));
		return nRetVal;
    }
    else
    {
	ALOGD("CCR","Initialize Cmd Returned Status: %d\n", abs(nRetVal));
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Initialize Cmd Returned Status: %d\n", abs(nRetVal));
        }
    }

    nRetVal = CommandAuto( Port, Baudrate, CMD_LED, (unsigned char *)LED_GREEN_ON, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Set LED Cmd Returned Error: %d\n", abs(nRetVal));
#endif
        return nRetVal;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("Set LED Cmd Returned Status: %d\n", abs(nRetVal));
#endif
        }
    }


	return nRetVal;
}

int blinkGreenLED(unsigned long Port, unsigned long Baudrate)
{
    // LED_FT_YEL_BLINK_FAST
    char BlinkFast = '3'; // 1 per second
    int nRetVal;
    int nDataSize = sizeof(BlinkFast);

    nRetVal = CommandAuto( Port, Baudrate, CMD_LED_YEL_BLINK, (unsigned char *)&BlinkFast, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Set LED Cmd Returned Error: %d\n", abs(nRetVal));
#endif
        return nRetVal;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("Set LED Cmd Returned Status: %d\n", abs(nRetVal));
#endif
        }
    }
    return SUCCESS;
}


int getModelSerialNumber(unsigned long Port, unsigned long Baudrate, ccr_modelSerialNumber_t *ModelSerialNum)
{
    int nRetVal = 0;
    int nDataSize = 0;

    memset(ModelSerialNum->serialNumber, 0, sizeof(ModelSerialNum->serialNumber));
    memset(ModelSerialNum->modelNumber, 0, sizeof(ModelSerialNum->modelNumber));

    nRetVal = CommandAuto( Port, Baudrate, CMD_INFO_MODEL, ModelSerialNum->modelNumber, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Send Get Model Number Cmd Returned Error: %02d\n", abs(nRetVal));
#endif
        return nRetVal;
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get Model Number Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf(" Model Number: %s\n", ModelSerialNum->modelNumber);
    }
#endif
    nRetVal = CommandAuto( Port, Baudrate, CMD_INFO_SN, ModelSerialNum->serialNumber, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Send Get Serial Number Cmd Returned Error: %02d\n", abs(nRetVal));
#endif
        return nRetVal;
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get Serial Number Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf("Serial Number: %s\n\n", ModelSerialNum->serialNumber);
    }
#endif
    return nRetVal;
}

int getFirmwareVersions(unsigned long Port, unsigned long Baudrate, ccr_VersionNumbers_t *firmwareVersions)
{
    int nRetVal = SUCCESS;
    int nDataSize = 0;

    memset(firmwareVersions->emv2000Version, 0, sizeof(firmwareVersions->emv2000Version));
    memset(firmwareVersions->supervisorVersion, 0, sizeof(firmwareVersions->supervisorVersion));
    memset(firmwareVersions->userVersion, 0, sizeof(firmwareVersions->userVersion));

    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_USER_FW_VERS, (unsigned char *)firmwareVersions->userVersion, &nDataSize);
    if(nRetVal < 0)
    {
        return nRetVal;
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get FW Versions User Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf(" User FW Vers: %s\n", firmwareVersions->userVersion);
    }
#endif

    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_SUPER_FW_VERS, (unsigned char *)firmwareVersions->supervisorVersion, &nDataSize);
    if(nRetVal < 0)
    {
        return nRetVal;
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get Super FW Version Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf("Super FW Vers Number: %s\n\n", firmwareVersions->supervisorVersion);
    }
#endif

    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_EMV2000_FW_VERS, (unsigned char *)firmwareVersions->emv2000Version, &nDataSize);
    if(nRetVal < 0)
    {
        return nRetVal;
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get EMV2000 FW Version Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf("EMV2000 FW Vers Number: %s\n\n", firmwareVersions->emv2000Version);
        nRetVal = SUCCESS;
    }
#endif

// Security CPU FW versions
    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_SECCPU_SUPER_FW_VERS,
                          (unsigned char *)firmwareVersions->secCPUSuperVersion, &nDataSize);
    if(nRetVal < 0)
    {
        if(abs(nRetVal) == 1)
        {
            // Just means we are running in User mode code
            firmwareVersions->secCPUSuperVersion[0] = 0; // Mark as empty string
            nRetVal = SUCCESS;
        }
        else
        {
            return nRetVal;
        }
    }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get Supervisor Security FW Version Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf("Supervisor Security FW Vers Number: %s\n\n", firmwareVersions->secCPUSuperVersion);
        nRetVal = SUCCESS;
    }
#endif

    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_SECCPU_USER_FW_VERS,
                          (unsigned char *)firmwareVersions->secCPUUserVersion, &nDataSize);
        if(abs(nRetVal) == 1)
        {
            // Just means we are running in Supervisor mode code
            firmwareVersions->secCPUSuperVersion[0] = 0; // Mark as empty string
            nRetVal = SUCCESS;
        }
        else
        {
            return nRetVal;
        }
#ifdef PRINT_MSGS
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Send Get EMV2000 FW Version Cmd Returned Status: %02d\n", abs(nRetVal));
        }
        printf("EMV2000 FW Vers Number: %s\n\n", firmwareVersions->secCPUSuperVersion);
        nRetVal = SUCCESS;
    }
#endif

    return nRetVal;
}


unsigned char TempKey16[16] = {0xCB, 0x9B, 0x6B, 0xCE, 0x86, 0xEA, 0x8F, 0xD3,
                           0xA2, 0xD0, 0xD0, 0x15, 0x85, 0x91, 0xF1, 0x2C};

// Temporary Master Key for Development Reader
unsigned char DevKeyTemp[16] = {0xCB, 0x9B, 0x6B, 0xCE, 0x86, 0xEA, 0x8F, 0xD3,
                           0xA2, 0xD0, 0xD0, 0x15, 0x85, 0x91, 0xF1, 0x2C};

// Temporary Master Key for Production Reader
unsigned char ProdKeyTemp[16] = {0x68, 0xB0, 0x20, 0x0D, 0x04, 0xCE, 0x45, 0x37,
                            0x68, 0xB6, 0x5D, 0x01, 0x85, 0x79, 0x26, 0x92};

// This is our Master Key that has been sent to the Reader.
//unsigned char KeyKm[16] = {0xD0, 0x6B, 0x5E, 0x85, 0xAE, 0x02, 0x83, 0xB0,
//                            0xD3, 0x9D, 0x62, 0x13, 0xB5, 0x7C, 0xFB, 0x49};

// NOTE: BBB: DEBUG ****************************************
// The KeyKm value must always be available to the program when running
// so it either has to be compiled in, which implies one Km for everyone
// Or it needs to be stored encrypted on the file system so that the test
// code can access it.  For now it is hard coded in the application, but
// if a new Master key is generated it must replace the data for KeyKm
// below.
// **********************************************
// This is the Master Key that Sankyo provided with their Windows authentication and encrypt read test app.
//unsigned char KeyKm[16] =  {0x3E,0x3D,0x3B,0x38,0x37,0x34,0x32,0x31,0x0E,0x0D,0x0B,0x08,0x07,0x04,0x02,0x01};
// My key that I had generated in initial testins
unsigned char KeyKm[16] = {0xD0, 0x6B, 0x5E, 0x85, 0xAE, 0x02, 0x83, 0xB0,
                            0xD3, 0x9D, 0x62, 0x13, 0xB5, 0x7C, 0xFB, 0x49};
//unsigned char KeyKe[16] = {0x01,0x02,0x04,0x07,0x08,0x0B,0x0D,0x0E,0x31,0x32,0x34,0x37,0x38,0x3B,0x3D,0x3E};
//unsigned char KeyKwa[16] = {0x31,0x32,0x34,0x37,0x38,0x3B,0x3D,0x3E,0x01,0x02,0x04,0x07,0x08,0x0B,0x0D,0x0E};
/*
unsigned char EncTheirKm[16] = {0x6a, 0x73, 0x78, 0x00, 0x0e, 0x53, 0x31, 0x7a,
                                0xe9, 0x20, 0x10, 0x1e, 0xf1, 0xae, 0xcf, 0xbd};
unsigned char TheirKmDecrypted[16];
*/

// Dynamically generated Keys and data for authentication and Track Data decryption.
unsigned char KeyKe[16]; // Key Enciphering Key for Dev Auth and Magnetic Data
unsigned char EncryptedKeyKe[16];

unsigned char KeyKwa[16]; // Authentication Key
unsigned char EncryptedKeyKwa[16];

unsigned char KeyKwm[16]; // Magnetic data Key
unsigned char MagDataIV[8]; // IV for CBC
unsigned char EncryptedMagDataKwmIV[24];

unsigned char EncryptedRndDt_A[16];
unsigned char RndDt_A[16];
unsigned char RndDt_B[16];
unsigned char RndDt_BDecrypted[16];

unsigned char DevAuthSig[32];
unsigned char EncryptedDevAuthSig[32];

void DataMessageToLog(
	unsigned long		Port,
	char			*pCommand_Data,
	unsigned long		Command_Data_Length,
	unsigned long		HeaderLength
);

void MessageToLog(
	unsigned long		Port,
	char			*pMessage,
	unsigned long		Data,
	long			Ret
);

#define R_HEADER_LEN			(5)		// Pxxxx
#define PROC_NO_DATA			(0)
#define PROC_EXIST_DATA			(1)

void logHexData(unsigned long Port, char *pName, unsigned char *pData, int length)
{
    char LogBuffer[2048];
    char *pWork = LogBuffer;
    int i;

    memset(LogBuffer, 0, sizeof(LogBuffer));
    sprintf(LogBuffer, "%s: ", pName);
    pWork += strlen(pName);
    pWork += 2; // ": "

    for(i = 0; i < length; i++)
    {
        sprintf(pWork, "%02X ", pData[i]);
        pWork += 3;
    }
    MessageToLog(Port, LogBuffer, PROC_EXIST_DATA, 0);
}

int ZapReaderLock(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;
    unsigned char TempBuffer[32];
    int nDataSize = sizeof(TempBuffer);
    char YesNo = 'N';

    /*
    *   Give them a chance to bail if the wrong reader type has been set.
    */
    printf("\n\n******** WARNING *********\n");
    printf("You are about to Unlock a locked Development Reader\n");
    printf( "This will reset the Card Reader Master Key to default.\n\n");
    printf("After you execute this command you will need to upload\n");
    printf("A new Master Key before you will be able to fully use the Reader.\n\n");
    printf("Do you want to clear the Master Key Y for yes\n");
    printf("or any other key to return without clearing the Master Key: \n");
    YesNo = getchar();
    if(YesNo != 'Y')
    {
        printf("\nReader Unlock not done, Master Key not cleared.\n");
        return -1;
    }

    nRetVal = CommandAuto( Port, Baudrate, CMD_ZAP_READER_LOCK, TempBuffer, &nDataSize);
    if(nRetVal < 0)
    {
        printf("Send Clear Reader Security Lock Cmd Returned Error: %02d\n", abs(nRetVal));
        return -1;
    }

    return 0;
}

int authenticateReader(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;
    int i;
    int nDataSize = 0;


	char LogBuffer[2048];
/*
unsigned char KeyKe[16] = {0x01,0x02,0x04,0x07,0x08,0x0B,0x0D,0x0E,0x31,0x32,0x34,0x37,0x38,0x3B,0x3D,0x3E};
unsigned char KeyKwa[16] = {0x31,0x32,0x34,0x37,0x38,0x3B,0x3D,0x3E,0x01,0x02,0x04,0x07,0x08,0x0B,0x0D,0x0E};
*/

    sprintf(LogBuffer, "authenticateReader(): Entered\n");
    // Write Log
//    DataMessageToLog( Port, LogBuffer, strlen(LogBuffer), HeaderLength );
    MessageToLog(Port, LogBuffer, PROC_EXIST_DATA, 0);


//    nRetVal = decryptDESKey(EncTheirKm, TheirKmDecrypted, TempKey16);



    logHexData(Port, "Key Km", KeyKm, 16);

    // Generate KeyKe, encrypt with KeyKm
    nRetVal = generateDESKey(KeyKe);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error generating New DES Ke Key\n", __func__);
#endif
        return -1;
    }

    // Test the key
    nRetVal = validate2DESKey(KeyKe);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error validating DES Ke key\n", __func__);
#endif
        return -2;
    }

    logHexData(Port, "Key Ke", KeyKe, 16);
    memset(EncryptedKeyKe, 0, sizeof(EncryptedKeyKe));
    // Send with 47,31
    // On Error bail
    // Encrypt the newly generated Ke Key using the Master Key for the Card Reader
    nRetVal = encryptDESKey(KeyKe, EncryptedKeyKe, KeyKm);
    logHexData(Port, "Key Ke Encrypted with KeyKm", EncryptedKeyKe, 16);

    nRetVal = CommandAuto( Port, Baudrate, CMD_SEND_KeyKe, EncryptedKeyKe, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Send Ke Key Cmd Returned Error: %02d\n", _-funct__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Send Ke Key Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }

    // Load the authentication key
    // Generate KeyKwa, encrypt with KeyKe
    nRetVal = generateDESKey(KeyKwa);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error generating New DES Kwa Key\n", __func__);
#endif
        return -1;
    }

    // Test the key
    nRetVal = validate2DESKey(KeyKwa);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error validating DES Kwa key\n", __func__);
#endif
        return -2;
    }

    logHexData(Port, "Key Kwa", KeyKwa, 16);

    memset(EncryptedKeyKwa, 0, sizeof(EncryptedKeyKwa));

    // Encrypt the newly generated Kwa Key using the Ke Key for the Card Reader
    nRetVal = encryptDESKey(KeyKwa, EncryptedKeyKwa, KeyKe);
    logHexData(Port, "Key Kwa Encrypted with KeyKe", EncryptedKeyKwa, 16);

    // Send with 47,32
    // On Error bail

    nRetVal = CommandAuto( Port, Baudrate, CMD_SEND_KeyKwa, EncryptedKeyKwa, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Send Kwa Key Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Send Kwa Key Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }


    // Get Challenge Command
    // Send 47,37 no data
    nRetVal = CommandAuto( Port, Baudrate, CMD_GET_CHALLENGE, EncryptedRndDt_A, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Send Get Challenge Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Send Get Challenge Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }

    logHexData(Port, "Challenge (Encrypted From ICRW)", EncryptedRndDt_A, 16);

    // Success should have 16 bytes encrypted in EncryptedRndDt_A with KeyKwa
    // Decrypt the data to RndDt_A
    nRetVal = decryptDESKey(EncryptedRndDt_A, RndDt_A, KeyKwa);
    MessageToLog(Port, "RndDt_A Challenge From ICRW Decrypted with KeyKwa\n", PROC_EXIST_DATA, 0);
    logHexData(Port, "", RndDt_A, 16);

    // Generate 16 byte random data to RndDt_B
    generateRandomData(RndDt_B, 16);
    logHexData(Port, "Generated RndDt_B", RndDt_B, 16);

    // Interleave RndDt_B and RndDt_A to DevAuthSig one byte at a time starting with RndDt_B
    int j, k;
    j = 0; k = 0;
    for(i = 0; i < 16; i++)
    {
        DevAuthSig[j] = RndDt_B[i];
        DevAuthSig[j+1] = RndDt_A[i];
        j += 2;
    }
    MessageToLog(Port, "Interleaved RndDt_A and RndDt_B\n", PROC_EXIST_DATA, 0);
    logHexData(Port, " ", DevAuthSig, 32);

    // Encrypt DevAuthSig 32 bytes with the KeyKwa
    nRetVal = encryptDESKey(DevAuthSig, EncryptedDevAuthSig, KeyKwa);
    nRetVal = encryptDESKey(DevAuthSig+16, EncryptedDevAuthSig+16, KeyKwa);

    MessageToLog(Port, "Interleaved RndDt_A and RndDt_B Encrypted with KeyKwa\n", PROC_EXIST_DATA, 0);
    logHexData(Port, " ", EncryptedDevAuthSig, 32);

    // Send 47, 38
    // On Error bail
    nRetVal = CommandAuto( Port, Baudrate, CMD_DEV_AUTH, EncryptedDevAuthSig, &nDataSize);
    if(nRetVal < 0)
    {
        MessageToLog(Port, "Dev Auth Failed:\n", PROC_EXIST_DATA, 0);
#ifdef PRINT_MSGS
        printf("[%s] Send Device Authentication Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
#ifdef PRINT_MSGS
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("[%s] Send Device Authentication Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
        }
#endif
    }

    // Success should have 16 bytes encrypted with KeyKwa in 1st 16 bytes of EncryptedDevAuthSig
    // Decrypt to RndDt_BDecrypted
    nRetVal = decryptDESKey(EncryptedDevAuthSig, RndDt_BDecrypted, KeyKwa);

    // Compare RndDt_BDecrypted with RndDt_B
    nRetVal = memcmp(RndDt_BDecrypted, RndDt_B, 16);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] RndDt_B does not match\n", __func__);
#endif
        return -3;
    }

    // We are now authenticated

    return 0;
}

/*********************************************************
 * NAME:   SetNewMasterKey()
 * DESCRIPTION: This is really only used when commissioning
 *              a new Card Reader.  A different function
 *              will be required to set a new Master Key
 *              for a reader that already has a custom
 *              Master Key installed.
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *
 * OUT: SUCCESS (0) or error code.
 *********************************************************/
int SetNewMasterKey(unsigned long	Port, unsigned long	Baudrate)
{
    unsigned char newMasterKey[16];

    // Test Km that Sankyo was setting in their demo application will use that for now
//unsigned char KeyKm[16] = {0xD0, 0x6B, 0x5E, 0x85, 0xAE, 0x02, 0x83, 0xB0,
//                            0xD3, 0x9D, 0x62, 0x13, 0xB5, 0x7C, 0xFB, 0x49};
// BBB: DEBUG *********** Remove this KeyKm when testing is completed.
    unsigned char EncryptedMasterKey[16];
    unsigned char DecryptedMasterKey[16];
    int nRetVal;
	FILE  *DataFilePtr;
	int nDataSize = 0;

    int i;
    char YesNo = 'N';

    printf("\n\n******** WARNING *********\n");
    printf("Setting new Master Key on a %s Reader\n",
           theConfiguration.bDevelopmentReader == 1 ? "Development" : "Production");
    printf("If the connected reader is not a %s type then you should\n",
           theConfiguration.bDevelopmentReader == 1 ? "Development" : "Production");
    printf("NOT perform this operation as it may render a production reader unusable.\n");
    printf("\n");
    printf("Do you want to Load a new Master Key to the %s Reader? Y for yes\n",
           theConfiguration.bDevelopmentReader == 1 ? "Development" : "Production");
    printf("Or any other key to return without clearing the Master Key: \n");
    YesNo = getchar();
    if(YesNo != 'Y')
    {
        printf("\nNew Master Key not generated.\n");
        return 0;
    }

    nRetVal = generateDESKey(newMasterKey);
    if(nRetVal)
    {
        printf("Error generating New DES Master Key\n");
        return -1;
    }

// BBB: DEBUG For testing DEBUG
    memcpy(newMasterKey, KeyKm, sizeof(KeyKm));
// Remove the above copy after we are past intial proof of concept

    // Test the key
    nRetVal = validate2DESKey(newMasterKey);
    if(nRetVal)
    {
        printf("Error validating DES keys\n");
        return -2;
    }

    DataFilePtr = fopen("MASTERKey.TXT", "w");
    if(!DataFilePtr)
    {
        // Bail can't save file
        printf("Error: Cannot create MASTERKey.TXT file\n");
        return -1;
    }

    /*
    *   Here we need to display the new MasterKey,
    *   and write it to a file so it can be preserved.  Note
    *   the file should NOT be stored on the device so it should
    *   be copied to a secure place and deleted from the file system.
    */
    // We should also check the key to make sure that it is not weak and that it has proper parity.
    printf( "New Master Key: ");
    fprintf(DataFilePtr, "New Master Key: ");
    for( i = 0; i < (int)sizeof(newMasterKey); i++)
    {
        printf("%02X ", newMasterKey[i]);
        fprintf(DataFilePtr, "%02X ", newMasterKey[i]);
    }
    printf("\n");
    fprintf(DataFilePtr, "\n");
    fclose(DataFilePtr);

    memset(EncryptedMasterKey, 0, sizeof(EncryptedMasterKey));
    memset(DecryptedMasterKey, 0, sizeof(EncryptedMasterKey));

    if(theConfiguration.bDevelopmentReader)
    {
        // Use the temporary key for a development reader
        memcpy(TempKey16, DevKeyTemp, sizeof(TempKey16));
    }
    else
    {
        // Use the temporary key for a production reader
        memcpy(TempKey16, ProdKeyTemp, sizeof(TempKey16));
    }

    // Encrypt the newly generated Master Key using the Temporary Key for the Card Reader
    nRetVal = encryptDESKey(newMasterKey,
                            EncryptedMasterKey,
                            TempKey16);

    // Only for testing not needed for production.
    nRetVal = decryptDESKey(EncryptedMasterKey, DecryptedMasterKey, TempKey16);

    // Here we will set the new Master Key in the Reader

    nRetVal = CommandAuto( Port, Baudrate, CMD_IMPORT_CUSTOMER_MASTER_KEY, EncryptedMasterKey, &nDataSize);
    if(nRetVal < 0)
    {
        printf("Import Customer Master Key Cmd Returned Error: %02d\n", abs(nRetVal));
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
            printf("Import Customer Master Key Cmd Returned Status: %02d\n", abs(nRetVal));
        }
    }

    // BBB: Delete the key(s) from memory before returning , except for new master.
    return nRetVal;
}

/*********************************************************
 * NAME:   cancelCommand()
 * DESCRIPTION: Sends the DLE/EOT sequence to the Card
 *                  Reader to cancel a command that is
 *                  in progress.  E.g. Intake and Withdraw
 *                  commands may be canceled.
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *
 * OUT: SUCCESS (0) or error code.  Generally errors can be
 *          ignored as it most likely is caused by no command
 *          running when the cancel request is sent.
 *********************************************************/
int cancelCommand(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;

    // Send DLE, EOT (0x10,0x04)
    // Reader should return DLE, EOT immediately and be waiting for new commands
    // In thread need to check the return from the Intake/Withdraw commands
    // and if DLE, EOT continue to see that should stop processing.
    // May need to add it to the read track stuff as well.
    nRetVal = DLE_EOT(Port, Baudrate);

    if(nRetVal < 0)
    {
        printf("[%s] Cancel Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
    }
    else
    {
        nRetVal = SUCCESS;
    }

    return nRetVal;
}


/*********************************************************
 * NAME:   intakeCommand()
 * DESCRIPTION: Sends the intake Card command to the Reader.
 *                  This will tell the reader to wait for a
 *                  card to be inserted.
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *
 * OUT: SUCCESS (0) or error code.  Note: Not all errors
 *              are real errors, this will timeout after
 *              5 seconds if a card insertion is not detected.
 *              Which is just to allow the software to see
 *              if card processing should stop, e.g. powering
 *              down, or need to access other card commands.
 *********************************************************/
 int intakeCommand(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;
    int nDataSize = 1;
    unsigned int intakeTimeout = CCR_INTAKE_WITHDRAW_TIMEOUT_MSEC; // Millseconds

    // Need to issue Initialize command
    nRetVal = CommandAuto( Port, Baudrate, CMD_INIT, (unsigned char *)"0", &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Initialize Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Initialize Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }

#ifdef PRINT_MSGS
    printf( "Intake Waiting for Card to be swiped\n");
#endif

    // Then Intake Command
    nRetVal = CommandAuto( Port, Baudrate, CMD_INTAKE, (unsigned char *)&intakeTimeout, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Intake Cmd Returned Error: %02d\n", abs(nRetVal));
#endif
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("Intake Cmd Returned Status: %02d\n", abs(nRetVal));
#endif
        }
    }

    return nRetVal;
}

/*********************************************************
 * NAME:   withdrawCommand()
 * DESCRIPTION: Sends the withdraw Card command to the Reader.
 *                  This will tell the reader to read the data
 *                  when the card is removed.
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *
 * OUT: SUCCESS (0) or error code.  Note: Not all errors
 *              are real errors, this will timeout after
 *              5 seconds if a card removal is not detected.
 *              Which is a user "error" but not a reader
 *              error.
 *********************************************************/
int withdrawCommand(unsigned long Port, unsigned long Baudrate)
{
    int nRetVal;
    int nDataSize = 1;
    unsigned int withdrawTimeout = CCR_INTAKE_WITHDRAW_TIMEOUT_MSEC; // Milliseconds

#ifdef PRINT_MSGS
    printf( "Withdraw Waiting for Card to be swiped\n");
#endif

    // Then Intake Command
    nRetVal = CommandAuto( Port, Baudrate, CMD_WITHDRAW, (unsigned char *)&withdrawTimeout, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Withdraw Cmd Returned Error: %02d\n", abs(nRetVal));
#endif
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("Withdraw Cmd Returned Status: %02d\n", abs(nRetVal));
#endif
        }
    }

    return nRetVal;
}

/*********************************************************
 * NAME:   Read3Tracks()
 * DESCRIPTION: Reads the Track data for all three tracks
 *              that the Card Reader can read.  It is possible
 *              that not all tracks will have data, or can be
 *              read.  The TrackData structure is filled in
 *              with individual track status, data length, and
 *              track data (decrypted).
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *      TrackData Pointer to ccr_trackData_t structure that will
 *                  receive the track status, data length and
 *                  track data for each track.
 *
 * OUT: SUCCESS (0) or error code.
 *********************************************************/
int Read3Tracks(unsigned long Port, unsigned long Baudrate, ccr_trackData_t *TrackData)
{
    char *pWork;

    int dataLen = 0;
    int nRetVal = 0;
    int i;

    unsigned char *pTrackData;

    TrackData->track1DataLen = 0;
    TrackData->track2DataLen = 0;
    TrackData->track3DataLen = 0;

    TrackData->track1Status = 0;
    TrackData->track2Status = 0;
    TrackData->track3Status = 0;

    memset(TrackData->track1Data, 0, sizeof(TrackData->track1Data));
    memset(TrackData->track2Data, 0, sizeof(TrackData->track2Data));
    memset(TrackData->track3Data, 0, sizeof(TrackData->track3Data));

    for( i = 0; i < 3; i++ )
    {
        if( i == 0 )
        {
            pTrackData = TrackData->track1Data;
        }
        else if( i == 1)
        {
            pTrackData = TrackData->track2Data;
        }
        else
        {
            pTrackData = TrackData->track3Data;
        }

        dataLen = 0;
        nRetVal = ReadEncryptedTrack(Port, Baudrate,
                               pTrackData, &dataLen,
                               i+1);
#ifdef PRINT_MSGS
        if(nRetVal)
        {
            printf( "Error Track %d ", i+1);
            switch(abs(nRetVal))
            {
                case ccr_data_not_read: // Not read, or buffer is clear
                    printf("Mag Data Not Read or Buffer is clear\n\n");
                    break;

                case ccr_no_start_sentinel: // No start sentinel
                    printf("No Start sentinel\n\n");
                    break;

                case ccr_parity_error: // Parity Error
                    printf("Parity Error\n\n");
                    break;

                case ccr_no_end_sentinel: // No end sentinel, data over spec
                    printf("No end sentinel, more data than spec calls for.\n\n");
                    break;

                case ccr_lrc_error: // Error found at LRC code
                    printf("LRC error\n\n");
                    break;

                case ccr_no_data_on_track: // No magentic data on track
                    printf("No Mag Data on Track\n\n");
                    break;

                case ccr_no_data_block: // No data block, only start and end sentinels, and LRC
                    printf("No Data Block, only start, end sentinels and LRC\n\n");
                    break;

                default:
                    printf("Error Code: %d\n\n", abs(nRetVal));
                    break;
            }
        } // End of decoding error status code.
#endif

        if( i == 0 )
        {
            TrackData->track1Status = abs(nRetVal);
            TrackData->track1DataLen = dataLen;
            nRetVal = SUCCESS;
        }
        else if( i == 1)
        {
            TrackData->track2Status = abs(nRetVal);
            TrackData->track2DataLen = dataLen;
            nRetVal = SUCCESS;
        }
        else
        {
            TrackData->track3Status = abs(nRetVal);
            TrackData->track3DataLen = dataLen;
            nRetVal = SUCCESS;
        }

    } // For loop for track reading

    if(TrackData->track1DataLen)
    {
        pWork = strchr((char *)TrackData->track1Data, 0x80);
        if(pWork)
        {
            *pWork = 0;
            TrackData->track1DataLen = strlen((char *)TrackData->track1Data);
        }
    }

    if(TrackData->track2DataLen)
    {
        pWork = strchr((char *)TrackData->track2Data, 0x80);
        if(pWork)
        {
            *pWork = 0;
            TrackData->track2DataLen = strlen((char *)TrackData->track2Data);
        }
    }

    if(TrackData->track3DataLen)
    {
        pWork = strchr((char *)TrackData->track3Data, 0x80);
        if(pWork)
        {
            *pWork = 0;
            TrackData->track3DataLen = strlen((char *)TrackData->track3Data);
        }
    }

    /*
    *   For testing we will display the Track 1 and Track 2 data.
    *   Also we will have to send a test CC # to Auth.net as real ones don't work in test mode.
    *   For real processing we send either Track 2 (first choice) or Track 1 entire data to Auth.net
    *   AIM API.
    */
#ifdef PRINT_MSGS
    if( TrackData->track1DataLen )
    {
        printf( "Track 1 Data: \n%s\n\n", TrackData->track1Data);
    }

    if( TrackData->track2DataLen )
    {
        printf( "Track 2 Data: \n%s\n\n", TrackData->track2Data);
    }

    if( TrackData->track2DataLen == 0 )
    {
        printf( "Track 2 could not be read, checking track 1\n");
        if( TrackData->track1DataLen == 0 )
        {
            printf( "Track 1 and Track 2 could not be read.  Cannot process CC\n");
        }
    }
#endif

    return nRetVal;
}

/*********************************************************
 * NAME:   ReadEncryptedTrack()
 * DESCRIPTION: Reads an individual Track's Encrypted
 *                  data from the Card Reader buffer.
 *
 * IN:  Port - Port number for Card Reader
 *      Baudrate - Have to pass this unti Sankyo code is fixed.
 *      trackData - Pointer to buffer that will
 *                  receive the track data
 *      dataLen - Pointer to int that receives the number of
 *                  track bytes read.
 *      trackNumber - Specifies the track data to read, 1 - 3
 *
 * OUT: SUCCESS (0) or error code.
 *********************************************************/
int ReadEncryptedTrack(unsigned long Port, unsigned long Baudrate,
                       unsigned char *trackData, int *dataLen,
                       int trackNumber)
{
    int nRetVal = 0;
    int nCommand = -1;
    unsigned char trackDataEncrypted[1024];
    int nDataSize = sizeof(trackDataEncrypted);

    unsigned char KeyKe[16];
    unsigned char EncryptedKeyKe[16];

    unsigned char KeyKwm[16];
    unsigned char MagDataKeyIV[24]; // Holds Kwm + IV
    unsigned char EncryptedKeyKwm[24]; // Key + IV
    unsigned char MagDataIV[8] = {0,0,0,0,0,0,0,0};


    // Then generate Ke

    // Generate KeyKe, encrypt with KeyKm
    nRetVal = generateDESKey(KeyKe);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error generating New DES Ke Key\n", __func__);
#endif
        return -1;
    }

    // Test the key
    nRetVal = validate2DESKey(KeyKe);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error validating DES Ke key\n", __func__);
#endif
        return -2;
    }

    logHexData(Port, "Track Data Key Ke", KeyKe, 16);
    memset(EncryptedKeyKe, 0, sizeof(EncryptedKeyKe));
    // Send with 47,31
    // On Error bail
    // Encrypt the newly generated Ke Key using the Master Key for the Card Reader
    nRetVal = encryptDESKey(KeyKe, EncryptedKeyKe, KeyKm);
    logHexData(Port, "Key Ke Encrypted with KeyKm", EncryptedKeyKe, 16);

    nRetVal = CommandAuto( Port, Baudrate, CMD_MAG_DATA_KE, EncryptedKeyKe, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Send Ke Key Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Send Ke Key Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }

    // Generate KeyKwm, encrypt with KeyKe
    nRetVal = generateDESKey(KeyKwm);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error generating New DES Kwa Key\n", __func__);
#endif
        return -1;
    }

    // Test the key
    nRetVal = validate2DESKey(KeyKwm);
    if(nRetVal)
    {
#ifdef PRINT_MSGS
        printf("[%s] Error validating DES Kwa key\n", __func__);
#endif
        return -2;
    }

    logHexData(Port, "Track Data Key Kwm", KeyKwm, 16);

    memset(EncryptedKeyKwm, 0, sizeof(EncryptedKeyKwm));

    memcpy(MagDataKeyIV, KeyKwm, 16);
    memcpy(MagDataKeyIV+16, MagDataIV, 8);

    // Encrypt the newly generated Kwa Key using the Ke Key for the Card Reader
    nRetVal = encryptData(MagDataKeyIV, EncryptedKeyKwm, KeyKe, 24);
    logHexData(Port, "Key Kwa Encrypted with KeyKe", EncryptedKeyKwm, 24);

    // Send with 47,32
    // On Error bail

    nRetVal = CommandAuto( Port, Baudrate, CMD_MAG_DATA_KEY, EncryptedKeyKwm, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("[%s] Send Kwa Key Cmd Returned Error: %02d\n", __func__, abs(nRetVal));
#endif
        return -1;
    }
    else
    {
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("[%s] Send Kwa Key Cmd Returned Status: %02d\n", __func__, abs(nRetVal));
#endif
        }
    }

    // Encrypt Ke with Km
    // Send with 47,35 command
    // Generate mag data key
    // Specify an IV
    // Encrypt with Ke
    // Send with 47, 36 command

    memset(trackDataEncrypted, 0, sizeof(trackDataEncrypted));

    if( trackNumber == 1)
    {
        nCommand = CMD_MAG_TRACK1_READ;
    }
    else if( trackNumber == 2)
    {
        nCommand = CMD_MAG_TRACK2_READ;
    }
    else if( trackNumber == 3)
    {
        nCommand = CMD_MAG_TRACK3_READ;
    }
    else
    {
#ifdef PRINT_MSGS
        printf( "Bad track number %d\n", trackNumber);
#endif
        return -1;
    }


    // Read Track n with 30,4n command
#ifdef PRINT_MSGS
    printf("\nReading Track %d\n", trackNumber);
#endif
    nRetVal = CommandAuto(Port, Baudrate, nCommand, trackDataEncrypted, &nDataSize);
    if(nRetVal < 0)
    {
#ifdef PRINT_MSGS
        printf("Read track %d Cmd Returned Error: %02d\n", trackNumber, abs(nRetVal));
#endif
        return nRetVal;
    }
    else
    {
        *dataLen = nDataSize;
#ifdef PRINT_MSGS
        printf("Track %d Read Successfully\n", trackNumber);
#endif
        if(theConfiguration.bDisplayDebugMsgs)
        {
#ifdef PRINT_MSGS
            printf("Read track %d Cmd Returned Status: %02d\n", trackNumber, abs(nRetVal));
#endif
        }
        nRetVal = decryptTrackData(KeyKwm,
                                     MagDataIV,
                                     trackDataEncrypted,
                                     trackData,
                                     nDataSize);
    }


    // on  success return success code

    return nRetVal;
}

