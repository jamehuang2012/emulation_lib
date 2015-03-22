
/*
 * powermanager.h
 *
 * Author: Nate Jozwiak njozwiak@mlsw.biz
 *
 * Description: Definitions for Power Manager daemon
 *
 */

#ifndef __PM_MANAGER_H__
#define __PM_MANAGER_H__
#define PDM_ACTIVED   1
#define PDM_INACTIVED   0

void *pm_thread_func(void *prt);




int pm_init(void);
int pm_exit(void);
int pdm_set_state(int port, char state,int wait_flag);
int pdm_get_state(int port, int *state);
int pdm_get_info(char *fm_ver, char *serial_no);
int pdm_get_firmware_version(char *fm_version);
int pdm_get_serial_number(char *serial_number);
int pdm_get_load(float *load_v, int *load_i);
int scm_get_info(char *fm_ver, char *serial_no);
int scm_get_firmware_version(char *fm_version);
int scm_get_serial_number(char *serial_no);
int scm_get_load(float *load_v, int *load_i,float *battery_v,int *battery_i);


#endif				// __PM_MANAGER_H__
