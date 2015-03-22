
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <svsLog.h>
#include <svsErr.h>
#include <svsCommon.h>
#include <svsSocket.h>
#include <svsConfig.h>

//static socket_thread_info_t socket_thread_info;
static svsConfigInfo_t      svsConfigInfo;

//static int svsSocketClientConfigHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload);

int svsConfigServerInit(void)
{
    int     rc              = 0;
    const char *fileName    = "/usr/scu/etc/svsConfig.xml";

    memset(&svsConfigInfo, 0, sizeof(svsConfigInfo));

    svsConfigInfo.doc = xmlReadFile(fileName, 0, 0);
    if(svsConfigInfo.doc == 0)
    {
        logError("cannot parse configuration file %s", fileName);
        return(ERR_FAIL);
    }

    // Get the root element node
    svsConfigInfo.root_element = xmlDocGetRootElement(svsConfigInfo.doc);
    if(svsConfigInfo.root_element == 0)
    {
        logError("empty configuration file");
        return(ERR_FAIL);
    }
    // Validate entry
    if(xmlStrcmp(svsConfigInfo.root_element->name, (const xmlChar *)"svsLib"))
    {
        logError("configuration document not for SVSD: %s", svsConfigInfo.root_element->name);
        return(ERR_FAIL);
    }


#if 0
    memset(&socket_thread_info, 0, sizeof(socket_thread_info_t));
    // configure server info
    socket_thread_info.name        = "CONFIG";
    socket_thread_info.ipAddr      = SVS_SOCKET_SERVER_IP;
    socket_thread_info.port        = SOCKET_PORT_CONFIG;
    socket_thread_info.devFd1      = -1;
    socket_thread_info.devFd2      = -1;
    socket_thread_info.fnServer    = 0;
    socket_thread_info.fnClient    = svsSocketClientConfigHandler;
    socket_thread_info.fnDev1      = 0;
    socket_thread_info.fnDev2      = 0;
    socket_thread_info.fnThread    = svsSocketServerThread;
    socket_thread_info.len_max     = 0;

    // create the server
    rc = svsSocketServerCreate(&socket_thread_info);
    if(rc != ERR_PASS)
    {   // XXX
        return(rc);
    }
#endif

    return(rc);
}

int svsConfigServerUninit(void)
{
    int rc = ERR_PASS;

    if(svsConfigInfo.doc)
    {
        // free the document
        xmlFreeDoc(svsConfigInfo.doc);
        // Free the global variables that may have been allocated by the parser.
        xmlCleanupParser();
    }

    return(rc);
}
#if 0
//
// Description:
// This handler is called when the server receives request from client
//
static int svsSocketClientConfigHandler(int sockFd, svsSocketMsgHeader_t *hdr, uint8_t *payload)
{
    int rc = ERR_PASS;

    if(hdr == 0)
    {
        logError("");
        return(ERR_FAIL);
    }
    if(hdr->len != 0)
    {
        if(payload == 0)
        {
            logError("");
            return(ERR_FAIL);
        }
    }

    return(rc);
}
#endif

xmlNodePtr svsConfigNodeByNameFind(xmlNode *node, const char *nodeName)
{
    xmlNode *cur = 0;

    if(node == 0)
    {
        logError("node is null");
        return(0);
    }

    if(nodeName == 0)
    {
        logWarning("nodeName is null");
        return(0);
    }

    cur = node->xmlChildrenNode;
    if(cur == 0)
    {
        //logInfo("cur is null");
        return(0);
    }

    for(cur = node->xmlChildrenNode; cur; cur = cur->next)
    {
        if(cur->type == XML_ELEMENT_NODE)
        {
            if(xmlStrcmp(cur->name, (const xmlChar *)nodeName) == 0)
            {
                //logDebug("node type: Element, name: %s", cur->name);
                return(cur);
            }
        }
    }
    return(0);
}

xmlNodePtr svsConfigModuleNodeGet(const char *module)
{
    xmlNode *node;

    node = svsConfigNodeByNameFind(svsConfigInfo.root_element, "modules");
    node = svsConfigNodeByNameFind(node, module);

    return(node);
}

int svsConfigModuleParamStrGet(const char *module, const char *param, char *strDefaultParam, char *strParam, int strLen)
{
    int rc = ERR_PASS;
    xmlNode *node = 0;
    xmlChar *str = 0;

    node = svsConfigModuleNodeGet(module);
    node = svsConfigNodeByNameFind(node, param);

    if(node)
    {
        //logDebug("param found");
        str = xmlNodeListGetString(svsConfigInfo.doc, node->xmlChildrenNode, 1);
        if(str)
        {
            strncpy(strParam, (const char *)str, strLen);
            //logDebug("param value: %s", strParam);
        }
        else
        {
            logWarning("param [%s] value not defined", param);
        }
        xmlFree(str);
    }

    if(node == 0 || str == 0)
    {   // param not found or not defined, use default if provided
        if(strDefaultParam)
        {
            strncpy(strParam, strDefaultParam, strLen);
            logWarning("param [%s] not found, using default value: [%s]", param, strParam);
        }
        else
        {
            logWarning("param [%s] not found", param);
            rc = ERR_FAIL;
        }
    }

    return(rc);
}

int svsConfigParamIntGet(const char *module, const char *param, int intDefaultParam, int *intParam)
{
    int rc;
    char str[80];

    // convert to string
    snprintf(str, sizeof(str), "%d", *intParam);

    rc = svsConfigModuleParamStrGet(module, param, 0, str, sizeof(str));
    if(rc == ERR_FAIL)
    {
        *intParam = intDefaultParam;
        logWarning("param [%s] not found, using default value: [%d]", param, *intParam);
    }
    else
    {
        *intParam = atoi(str);
        //logDebug("param found with value: %d", *intParam);
    }

    return(ERR_PASS);
}


