#ifndef SVS_CONFIG_H
#define SVS_CONFIG_H

#include <libxml/parser.h>
#include <libxml/tree.h>

#define CONFIG_MSG_PAYLOAD_MAX     1024

typedef struct
{
    uint8_t     flags;     // acknowledge request, ignore reply
} svsMsgConfigHeader_t;

typedef struct
{
    svsMsgConfigHeader_t  header;
    uint8_t               payload[CONFIG_MSG_PAYLOAD_MAX];
} svsMsgConfig_t;

typedef struct
{
    xmlDoc  *doc;
    xmlNode *root_element;
} svsConfigInfo_t;

int svsConfigServerInit(void);
int svsConfigServerUninit(void);

int svsConfigModuleParamStrGet(const char *module, const char *param, char *strDefaultParam, char *strParam, int strLen);
int svsConfigParamIntGet(const char *module, const char *param, int intDefaultParam, int *intParam);

#endif // SVS_CONFIG_H

