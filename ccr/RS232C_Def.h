/**************************************************************************

	RS232C_Def.h

	RS-232C Card Reader 8bit Protocol Library
	Common Header File

***************************************************************************/

#ifndef	_RS232C_DEF_H_
#define	_RS232C_DEF_H_


#ifdef __cplusplus
extern "C"
{
#endif



//----------------------------------------------------------------------
//	Define Data type
//----------------------------------------------------------------------

#define	MAX_DATA_ARRAY_SIZE		(1024)



//----------------------------------------------------------------------
//	Define Data type
//----------------------------------------------------------------------

#pragma pack(8)

// Data structure for command message
typedef	struct
{
	unsigned char		CommandCode;
	unsigned char		ParameterCode;

	struct
	{
		unsigned long	Size;
		unsigned char	*pBody;
	}
	Data;
}
COMMAND, *PCOMMAND;
 

// Received message type
typedef	enum
{
	PositiveReply,
	NegativeReply,
	Unavailable,
}
REPLY_TYPE, *PREPLY_TYPE;


// Data structure for positive reply message
typedef	struct
{
	unsigned char		CommandCode;
	unsigned char		ParameterCode;

	struct
	{
		unsigned char	St1;
		unsigned char	St0;
	}
	StatusCode;

	struct
	{
		unsigned long	Size;
		unsigned char	Body[ MAX_DATA_ARRAY_SIZE ];
	}
	Data;
}
POSITIVE_REPLY, *PPOSITIVE_REPLY;


// Data structure for negative reply message
typedef	struct
{
	unsigned char		CommandCode;
	unsigned char		ParameterCode;

	struct
	{
		unsigned char	E1;
		unsigned char	E0;
	}
	ErrorCode;

	struct
	{
		unsigned long	Size;
		unsigned char	Body[ MAX_DATA_ARRAY_SIZE ];
	}
	Data;
}
NEGATIVE_REPLY, *PNEGATIVE_REPLY;


// Data structure for reply message
typedef struct
{
	REPLY_TYPE		replyType;

	union
	{
		POSITIVE_REPLY	positiveReply;
		NEGATIVE_REPLY	negativeReply;
	}
	message;
}
REPLY, *PREPLY;


// Data structure for Initialze Command Parameter of UpdateFirmware
typedef	struct
{
	unsigned char		ParameterCode;

	struct
	{
		unsigned long	Size;
		unsigned char	*pBody;
	}
	Data;
}
UPDATE_INIT_COMM, *PUPDATE_INIT_COMM;


// Data structure for Library Revision typedef	struct
{
	struct
	{
		char	Filename[32];
		char	Revision[32];
	}
	InterfaceLib;

	struct
	{
		char	Filename[32];
		char	Revision[32];
	}
	CheckLib;

	struct
	{
		char	Filename[32];
		char	Revision[32];
	}
	ProtocolLib;
}
LIB_INFORMATION, *PLIB_INFORMATION;


#ifdef __cplusplus
}
#endif


#endif	// _RS232C_DEF_H_

