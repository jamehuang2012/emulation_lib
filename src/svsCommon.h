#ifndef SVS_COMMON_H
#define SVS_COMMON_H

typedef unsigned char bool;

#define false 0
#define true 1

#define TIMEOUT_INFINITE    (-1)
#define TIMEOUT_IMMEDIATE   (0)
#define TIMEOUT_MAX         (5000)

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef enum
{
    BLOCKING_OFF,
    BLOCKING_ON
} blocking_t;

typedef enum
{
    MODULE_ID_LOG,
    MODULE_ID_CALLBACK,
    MODULE_ID_BDP,
    MODULE_ID_KR,       // key reader
    MODULE_ID_CCR,      // credit card reader
    MODULE_ID_PRT,      // printer
    MODULE_ID_SCM,      //
    MODULE_ID_WIM,      //
    MODULE_ID_MODEM,    // GSM modem
    MODULE_ID_DIO,      // GPIO monitor
    MODULE_ID_UPGRADE,  //
    MODULE_ID_PM,       // Power Manager

    MODULE_ID_SVS,      // the libSVS itself

    MODULE_ID_MAX       // KEEP LAST
} module_id_t;

#define STATS_ENABLE    // define to enable statistic counters used in drivers
#ifdef STATS_ENABLE
    #define STATS_INC(x)    ((x)++)       // increment statistic counter
#else
    #define STATS_INC(x)    ((void)0)   // empty statement
#endif

#endif // SVS_COMMON_H
