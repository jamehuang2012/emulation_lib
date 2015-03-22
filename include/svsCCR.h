#ifndef _SVS_CCR_H_
#define _SVS_CCR_H_

#include <svsErr.h>
#include <cardrdrdatadefs.h>

#define CARDMANAGER_IP "127.0.0.1"
#define CARDMANAGER_PORT 3357

/* Error codes for commands */

typedef enum
{
    ERR_CCR_SUCCESS = 0,
    /* Unimplemented command */
    ERR_CCR_UNIMPLEMENTED_ERROR,

    /* Data / Informational / Log related errors */
    ERR_CCR_MODELSN_ERROR,
    ERR_CCR_IN_ACQUIRE_CARD_STATE,

    /* Low level related errors */
    ERR_CCR_CONNECT_ERROR,
    ERR_CCR_INITIALIZE_ERROR,
    ERR_CCR_AUTHENTICATE_ERROR,
    ERR_CCR_SECURITY_LOCK_ERROR,
    ERR_CCR_DEVELOPMENT_READER,
    ERR_CCR_RELEASE_READER,

    /* Track and Card Reading related errors/status */
    ERR_CCR_CARD_READ_ERROR,
    ERR_CCR_HAS_TRACK1,
    ERR_CCR_HAS_TRACK2,
    ERR_CCR_HAS_TRACK1AND2,
    ERR_CCR_HAS_NOTRACKS,

    /* Card sensing errors */
    ERR_CCR_CARD_NOT_REMOVED,
    ERR_CCR_CARD_REMOVED_TOO_SLOWLY,
    ERR_CCR_CARD_WITHDRAW_ERROR,

    ERR_CCR_UNRECOVERABLE_ERROR,

    ERR_CCR_END
} ccr_card_manager_errors_t;

#define CCR_ERROR_STR \
    {ERR_CCR_SUCCESS,                  "PASS"},                     \
    {ERR_CCR_UNIMPLEMENTED_ERROR,      "CCR UNIMPLEMENTED CMD"},    \
    {ERR_CCR_MODELSN_ERROR,            "CCR MODEL/SN"},             \
    {ERR_CCR_IN_ACQUIRE_CARD_STATE,    "CCR IN ACQUIRE CARD STATE"},\
    {ERR_CCR_CONNECT_ERROR,            "CCR CONNECT ERROR"},        \
    {ERR_CCR_INITIALIZE_ERROR,         "CCR INIT ERROR"},           \
    {ERR_CCR_AUTHENTICATE_ERROR,       "CCR AUTH ERROR"},           \
    {ERR_CCR_SECURITY_LOCK_ERROR,      "CCR SEC LOCK ERROR"},       \
    {ERR_CCR_DEVELOPMENT_READER,       "CCR DEVELOPMENT READER"},   \
    {ERR_CCR_RELEASE_READER,           "CCR RELEASE READER"},       \
    {ERR_CCR_CARD_READ_ERROR,          "CCR READ ERROR"},           \
    {ERR_CCR_HAS_TRACK1,               "CC HAS TRACK 1"},           \
    {ERR_CCR_HAS_TRACK2,               "CC HAS TRACK 2"},           \
    {ERR_CCR_HAS_TRACK1AND2,           "CC HAS TRACK 1 & 2"},       \
    {ERR_CCR_HAS_NOTRACKS,             "CC HAS NO TRACKS"},         \
    {ERR_CCR_CARD_NOT_REMOVED,         "CC NOT REMOVED"},           \
    {ERR_CCR_CARD_REMOVED_TOO_SLOWLY,  "CC REMOVED TOO SLOWLY"},    \
    {ERR_CCR_CARD_WITHDRAW_ERROR,      "CC REMOVAL ERROR"},         \
    {ERR_CCR_UNRECOVERABLE_ERROR, "CCR UNRECOVERABLE ERROR"}

typedef struct tagStatusError
{
    int statusError; // 0 For informational status, 1 - hard error
    int cardReaderCode; // If statusError is 0 contains the
                        // Status value returned by the Card Reader
                        // device.
                        // If statusError is 1 contains the Error value
                        // returned by the Card Reader device.  This is
                        // useful for diagnostics.
    svs_err_t *svsError; // Pointer to the svs Error structure

} ccr_status_error_t;

extern int svsCCRInit(void);
extern int svsCCRUninit(void);

extern int svsCCRServerInit(void);
extern int svsCCRServerUninit(void);

extern svs_err_t *svsCCRStart(void);
extern svs_err_t *svsCCRStop(void);
extern svs_err_t *svsCCRModelSerial(ccr_modelSerialNumber_t *pModelSerial);
extern svs_err_t *svsCCRFWVersions(ccr_VersionNumbers_t *pVersionNumbers);

int ccr_init(void);
int exit_ccr(void);
int stop_ccr(void);
int start_ccr(void);
int ccr_callback_register(callback_fn_t callback);
#endif // _SVS_CCR_H_
