
#ifndef __SCM_THREAD_H__
#define __SCM_THREAD_H__

#define DEFAULT_SCM_POLL_TIMER  60

void *scm_thread_func(void *prt);

void scmProcessMessage(void);

//========== SCM API ==========//
void scmGetFWVersion(void);
void scmGetSerialNum(void);
void scmGetState(void);
void scmGetBattery(void);
void scmGetPower(void);
void scmGetRuntime(void);
#endif
