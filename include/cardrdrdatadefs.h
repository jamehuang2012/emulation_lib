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
 *   cardrdrdatadefs.h - Data structures for Credit Card related data structures
 *   used for sending and receiving data related to the Card Reader.
 */

#ifndef _CARD_RDR_DATA_DEFS_
#define _CARD_RDR_DATA_DEFS_

/* Card Manager Message Structure, Command and Error Code Definitions */
#define CCR_PROTOCOL_VERSION 1

typedef enum
{
    CARD_MANAGER_CANCEL_CMD = 0,
    CARD_MANAGER_REGISTER_CMD,
    CARD_MANAGER_INIT_CMD,
    CARD_MANAGER_ACQUIRE_CARD_CMD,
    CARD_MANAGER_STANDBY_CMD,
    CARD_MANAGER_RESUME_ACQUIRE_CARD,
    CARD_MANAGER_GET_MODEL_SN,

    CARD_MANAGER_GET_EMV_APPROVAL,
    CARD_MANAGER_GET_GIE_APPROVAL,

    CARD_MANAGER_GET_PASS_COUNTER,

    CARD_MANAGER_GET_USER_CRCC,
    CARD_MANAGER_GET_EMV2000_CRCC,

    CARD_MANAGER_GET_PERFORMANCE_LOG,
    CARD_MANAGER_GET_ERROR_LOG,
    CARD_MANAGER_EXIT,
    CARD_MANAGER_GET_FW_VERSIONS,
    CARD_MANAGER_CONN_TYPE
} ccr_card_manager_cmds;

/*
 * The following are internal Card Manager errors
 * They are translated to error codes for the
 * Application during processing by svsCCR.c code.
 */
typedef enum
{
    /* Unimplemented command */

    CARD_MANAGER_UNIMPLEMENTED_ERROR    = -10, // ERR_CCR_UNIMPLEMENTED_ERROR

    /* Data / Informational / Log related errors */
    CARD_MANAGER_MODELSN_ERROR          = -11,
    CARD_MANAGER_IN_ACQUIRE_CARD_STATE  = -12,

    /* Low level related errors */
    CARD_MANAGER_CONNECT_ERROR          = -100,
    CARD_MANAGER_INITIALIZE_ERROR       = -101,
    CARD_MANAGER_AUTHENTICATE_ERROR     = -102,
    CARD_MANAGER_SECURITY_LOCK_ERROR    = -103,
    CARD_MANAGER_DEVELOPMENT_READER     = -104,
    CARD_MANAGER_RELEASE_READER         = -105,


    /* Track and Card Reading related errors/status */
    CARD_MANAGER_CARD_READ_ERROR       = -1000,
    CARD_MANAGER_HAS_TRACK1            = -1001,
    CARD_MANAGER_HAS_TRACK2            = -1002,
    CARD_MANAGER_HAS_TRACK1AND2        = -1003,
    CARD_MANAGER_HAS_NOTRACKS          = -1004,

    /* Card sensing errors */
    CARD_MANAGER_CARD_NOT_REMOVED           = -1010,
    CARD_MANAGER_CARD_REMOVED_TOO_SLOWLY    = -1011,
    CARD_MANANGER_CARD_WITHDRAW_ERROR       = -1012
} ccr_card_manager_internal_errors_t;

typedef enum
{
    ccr_data_not_read = 20, // Data not read, or track buffer was cleared
    ccr_no_start_sentinel,  // No start sentinel to synchronize with
    ccr_parity_error,       // The track read generated a parity error
    ccr_no_end_sentinel,    // No end sentinel to terminate, more data than ISO spec
    ccr_lrc_error,          // LRC error, bad track data
    ccr_no_data_on_track,   // No magnetic data on the track
    ccr_no_data_block = 27  // Only start and end sentinels
} ccr_track_status_t;

typedef enum
{
    ccr_Command,
    ccr_Async,
    ccr_Monitor
} ccr_connection_type_t;


/*********************************************************
 * NAME:   cardMgrCmd
 * DESCRIPTION: Data Structure typedef used by the Kiosk
 *              application to send command requests to the
 *              Card Manager application.
 *
 *              Also see the cardrdrdatadefs.h header for
 *              associated data structures that may be
 *              sent with a specific command.
 *********************************************************/

typedef struct _cardMgrCmd
{
    ccr_card_manager_cmds command;  // Card Manager command
    ccr_connection_type_t connType; // ccr_Command, ccr_Async, or ccr_Monitor
    int protocolVersion;            // Version of communication protocol
} ccr_cardMgrCmd_t;

/*********************************************************
 * NAME:   cardMgrResult
 * DESCRIPTION: Data Structure typedef used by the Card
 *              Manager application to send command return
 *              status and optional data to the Kiosk
 *              application.
 *
 *              Also see the cardrdrdatadefs.h header for
 *              associated data structures that are
 *              sent with specific command responses.
 *********************************************************/

typedef struct _cardMgrResult
{
    ccr_card_manager_cmds command;  // Card Manager command
    int protocolVersion;            // Version of communication protocol
    int error;                      // Set to 1 if error, 0 if success
    int status;                     // Status/error code
    int readerStatus;               // The Status or Error code returned by the Reader Device
} ccr_cardMgrResult_t;

// ================= End of Card Manager Message Structure Definitions

/*********************************************************
 * NAME:   ccr_trackData_t
 * DESCRIPTION: Data Structure used for sending track data
 *              between Card Manager and Kiosk application.
 *              Also see the cmgrapi.h header for command
 *              and result data structures that precede
 *              The command specific data structures.
 *********************************************************/
typedef struct tagTrackData_t
{
    ccr_cardMgrResult_t cardMgrResult;  // The header information
    unsigned char track1Status;         // Track 1 Status and Data Len
    unsigned char track1DataLen;
    unsigned char track2Status;         // Track 2 Status and Data Len
    unsigned char track2DataLen;
    unsigned char track3Status;         // Track 3 Status and Data Len
    unsigned char track3DataLen;

    unsigned char track1Data[80];
    unsigned char track2Data[41];
    unsigned char track3Data[41];
} ccr_trackData_t;

/*********************************************************
 * NAME:   ccr_modelSerialNumber_t
 * DESCRIPTION: Data Structure typedef used for sending
 *              Card Reader Model and Serial numbers from
 *              the Card Manager to Kiosk application.
 *              Also see the cmgrapi.h header for command
 *              and result data structures that precede
 *              The command specific data structures.
 *********************************************************/
typedef struct tagModelSN_t
{
    unsigned char modelNumber[16];
    unsigned char serialNumber[9];
} ccr_modelSerialNumber_t;

/*********************************************************
 * NAME:   ccr_VersionNumbers_t
 * DESCRIPTION: Data Structure typedef used for sending
 *              Card Reader firmware version numbers from
 *              the Card Manager to Kiosk application.
 *              Also see the cmgrapi.h header for command
 *              and result data structures that precede
 *              The command specific data structures.
 *********************************************************/
typedef struct tagVersionNumbers
{
    // General function CPU Firmware Versions
    char supervisorVersion[10];
    char userVersion[10];
    char emv2000Version[10];
    // CRCC values for User code and EMV2000 code
    char userCRCC[6];
    char emv2000CRCC[6];
    // Security CPU Firmware Versions
    char secCPUSuperVersion[10]; // If not running Supervisor code will be empty
    char secCPUUserVersion[10];  // If not running User code will be empty.
} ccr_VersionNumbers_t;

/*********************************************************
 * NAME:   ccr_Info_t
 * DESCRIPTION: Data Structure typedef used for sending
 *              Card Reader firmware version, CRCC, Model
 *              and Serial numbers from
 *              the Card Manager to Kiosk application.
 *********************************************************/
typedef struct tagCCRInfo
{
    ccr_cardMgrResult_t cardMgrResult;  // The header information
    ccr_modelSerialNumber_t modelSerialNumber;
    ccr_VersionNumbers_t versionNumbers;
} ccr_Info_t;

// More to come

#endif // _CARD_RDR_DATA_DEFS_
