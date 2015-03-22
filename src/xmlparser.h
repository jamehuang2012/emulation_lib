/**
 * xml parsing library
 */
#ifndef __XMLPARSER_H__
#define __XMLPARSER_H__

#include <libxml/parser.h>
#include <libxml/tree.h>

//#include "common.h"


extern char *getNodeContentByName(xmlNodePtr parent, char* name);
extern xmlNodePtr getNodeByName(xmlNodePtr nodePtr, char * name);

#endif /* __XMLPARSER_H__ */

