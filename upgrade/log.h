#ifndef __LOG_H__
#define __LOG_H__

#define MESBUFFER 2048
#define MAXLOGLEN 400
#define MAXLOGSIZE 524288
#define MESFORMAT 6
#define MAXAPPLEN 20

#define LEVEL_OFF        0x00
#define LEVEL_FATAL      0x02
#define LEVEL_CRITICAL   0x04
#define LEVEL_ERROR      0x08
#define LEVEL_WARNING    0x10
#define LEVEL_INFO       0x20
#define LEVEL_DEBUG      0x40

typedef enum
{
    eslLog = 1,
    eslCommand = 2,
    eslRegistration = 4,
    eslGetClientlist = 8
} MessageType;

struct Header
{
    MessageType type;
    uint32_t pllen;
};

struct LogPayload
{
    char name[MAXAPPLEN];
    uint8_t level;
    uint8_t display;
    char message[];
};

struct RegPayload
{
    uint8_t level;
    char name[];
};

struct CmdPayload
{
    uint8_t level;
    uint8_t display;
    char name[MAXAPPLEN];
};

struct ServerResponse
{
    struct Header header;
    uint32_t listSize;
    struct CmdPayload client[];
};
    
#ifdef __cplusplus
extern "C"
{
#endif
/* Interfacing prototypes */
extern void logMessage(uint8_t level, char *message, ...);

#endif /* __LOG_H__ */
