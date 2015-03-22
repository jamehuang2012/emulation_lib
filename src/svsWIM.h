#ifndef SVS_WIM_H
#define SVS_WIM_H

#define GPSW_ADDR_LEN   24
#define GPSW_ADDR_POS   10
#define GPS_LOG_LEN     100
#define BIKE_MSN_LEN    21
#define TIMESTAMP_LEN   50
#define DOCKING_PT_LEN  17

#define GPSLOG_COMPLETE     "GPSLOG_COMPLETE"

typedef struct
{
    char gpswAddr[GPSW_ADDR_LEN];
    char gpsLogPath[GPS_LOG_LEN];
} get_log_request_t;

int svsWIMServerInit(void);
int svsWIMServerUninit(void);
int svsWIMInit(void);
int svsWIMUninit(void);
int svsWIMGetGPSLog(char *addr);
get_log_request_t *svsProcessGPSLogRequest(char *gpswShortAddr, char *gpswGPSLog);

#endif // SVS_WIM_H
