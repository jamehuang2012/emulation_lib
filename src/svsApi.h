#ifndef SVSAPI_H
#define SVSAPI_H

#include <svsErr.h>
#include <svsCallback.h>
#include <svsBdpMsg.h>
#include <svsKrMsg.h>

#define TIMEOUT_INFINITE    (-1)
#define TIMEOUT_IMMEDIATE   (0)
#define TIMEOUT_MAX         (5000)
#define TIMEOUT_DEFAULT     (10)

// ------------------------------------------------------------------
//  SVS
// ------------------------------------------------------------------

typedef struct // message structure
{
    uint8_t bdp_available;
} svs_bdp_available_rsp_t;

typedef struct // message structure
{
    uint8_t kr_available;
} svs_kr_available_rsp_t;

typedef struct // message structure
{
    log_verbosity_t verb;
} svs_log_verbosity_set_req_t;

typedef struct
{
    uint8_t                 module_id;
    uint8_t                 dev_num;
    module_power_state_t    power_state;
} module_power_t;

#define MSN_LENGTH      8
typedef struct
{
    uint8_t  msn[MSN_LENGTH];
} msn_addr_t;

typedef struct
{
    uint8_t       module_id;
    uint8_t       dev_num;
    msn_addr_t    msn_addr;
} module_msn_t;

// ------------------------------------------------------------------

/* retrieve error message string, given a code. Previously, this was
   Only being used locally by svsApi.c, but higher level system
   components (e.g. SCU Quick Test) may need the string to pass on to
   the user / technician.
 */
svs_err_t *errUpdate(err_t code);

int svsLogVerbositySet(log_verbosity_t verbosity);
svs_err_t *svsModuleMsnGet(module_msn_t *data, int timeout_ms);

// ------------------------------------------------------------------
//  KR
// ------------------------------------------------------------------
int svsBdpAvailableGet(uint16_t *bdp_available);
svs_err_t *svsBdpScan(void);
svs_err_t *svsBdpScanForce(void);
svs_err_t *svsBdpMsnFlush(void);

svs_err_t *svsModulePowerSet(module_power_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsModulePowerGet(module_power_t *data, int timeout_ms);

svs_err_t *svsBdpLedSet(bdp_led_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpLedGet(bdp_led_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpPowerSet(bdp_power_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpPowerGet(bdp_power_t *data, int timeout_ms);

svs_err_t *svsBdpBikeLockGet(bdp_bike_lock_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpBikeLockSet(bdp_bike_lock_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpBuzzerSet(bdp_buzzer_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpBuzzerGet(bdp_buzzer_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpLockKeyGet(bdp_unlock_code_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpSwitchGet(bdp_switch_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpRfidGet(bdp_rfid_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpMotorGet(bdp_motor_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpMotorSet(bdp_motor_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpJtagDebugSet(bdp_jtag_debug_t *data, int timeout_ms);
svs_err_t *svsBdpEcho(bdp_echo_t *data, int timeout_ms);
svs_err_t *svsBdpConsole(bdp_console_t *data, int timeout_ms);
svs_err_t *svsBdpStatsGet(bdp_get_stats_t *data, int timeout_ms);

svs_err_t *svsBdpChangeMode(bdp_change_mode_t *data, int timeout_ms);
svs_err_t *svsBdpGetMode(bdp_get_mode_t *data, int timeout_ms);
svs_err_t *svsBdpFirmwareUpgrade(bdp_firmware_upgrade_t *data, int timeout_ms);
svs_err_t *svsBdpBootblockUpload(bdp_firmware_upgrade_t *data, int timeout_ms);
svs_err_t *svsBdpBootblockInstall(bdp_bootblock_install_t *data, int timeout_ms);
svs_err_t *svsBdpMemoryCrc(bdp_memory_crc_t *data, int timeout_ms);

svs_err_t *svsBdpGetApplicationRev(bdp_get_rev_t *data, int timeout_ms);
svs_err_t *svsBdpGetBootblockRev(bdp_get_rev_t *data, int timeout_ms);

svs_err_t *svsBdpMsnGet(bdp_address_t *data);
svs_err_t *svsBdpMsnSet(bdp_address_t *data, int timeout_ms);

svs_err_t *svsBdpMsnForceGet(bdp_address_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);

svs_err_t *svsBdpCompatibilitySet(bdp_compatibility_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);
svs_err_t *svsBdpCompatibilityGet(bdp_compatibility_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback);


// ------------------------------------------------------------------
//  KR
// ------------------------------------------------------------------

svs_err_t *svsKrScan(void);
int svsKrAvailableGet(uint16_t *kr_available);

svs_err_t *svsKrAddressSet(kr_address_t *data, int timeout_ms);
svs_err_t *svsKrAddressGet(kr_address_t *data, int timeout_ms);

svs_err_t *svsKrEcho(kr_echo_t *data, int timeout_ms);

svs_err_t *svsKrPowerSet(kr_power_t *data, int timeout_ms);
svs_err_t *svsKrPowerGet(kr_power_t *data, int timeout_ms);

svs_err_t *svsKrRfidGet(kr_rfid_t *data, int timeout_ms);

svs_err_t *svsKrChangeMode(kr_change_mode_t *data, int timeout_ms);
svs_err_t *svsKrGetMode(kr_get_mode_t *data, int timeout_ms);
svs_err_t *svsKrGetApplicationRev(kr_get_rev_t *data, int timeout_ms);
svs_err_t *svsKrGetBootblockRev(kr_get_rev_t *data, int timeout_ms);
svs_err_t *svsKrFirmwareUpgrade(kr_firmware_upgrade_t *data, int timeout_ms);
svs_err_t *svsKrBootblockUpload(kr_firmware_upgrade_t *data, int timeout_ms);
svs_err_t *svsKrBootblockInstall(kr_bootblock_install_t *data, int timeout_ms);
svs_err_t *svsKrMemoryCrc(kr_memory_crc_t *data, int timeout_ms);

extern svs_err_t *svsRemoteStatusGet(bool *stayConnected);
extern svs_err_t *svsRemoteConnect(void);
extern svs_err_t *svsRemoteDisconnect(void);

#endif /* SVSAPI_H */
