/* svsApi.c */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <svsErr.h>
#include <svsLog.h>
#include <svsCommon.h>
#include <libSVS.h>
#include <svsApi.h>
#include <svsCallback.h>
#include <svsBdp.h>
#include <svsKr.h>
#include <svsSocket.h>

// ------------------------------------------------------------------
//  UTILITIES
// ------------------------------------------------------------------

svs_err_t svs_err[] =
{
    SVS_ERROR_STR
};

svs_err_t svs_err_unknown = {ERR_UNKNOWN, "UNKNOWN"};

#define ERR_MAX     (sizeof(svs_err) / sizeof(svs_err_t))

svs_err_t *errUpdate(err_t code)
{
    int i;

    for(i=0; i<ERR_MAX; i++)
    {
        if(svs_err[i].code == code)
        {
            return(&svs_err[i]);
        }
    }
    return(&svs_err_unknown);
}

// ------------------------------------------------------------------
//  SVS
// ------------------------------------------------------------------

// Get available BDP count
int svsBdpAvailableGet(uint16_t *bdp_available)
{
    int rc;
    svs_bdp_available_rsp_t rsp;

    if(bdp_available == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }

    rc = svsSocketServerSvsTransferSafe(0, SVS_MSG_ID_BDP_AVAILABLE, TIMEOUT_DEFAULT, 0, 0, (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    *bdp_available = rsp.bdp_available;

    return(rc);
}

int svsLogVerbositySet(log_verbosity_t verbosity)
{
    int rc;
    svs_log_verbosity_set_req_t req;

    req.verb = verbosity;

    rc = svsSocketServerSvsTransferSafe(0, SVS_MSG_ID_LOG_VERBOSITY_SET, TIMEOUT_DEFAULT, (uint8_t *)&req, sizeof(req), 0, 0);
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    return(rc);
}

svs_err_t *svsModulePowerSet(module_power_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc = ERR_FAIL;
    bdp_power_t bdp_power;

    if(data == 0)
    {
        logError("data is null");
        return(errUpdate(ERR_FAIL));
    }
    switch(data->power_state)
    {
        case MODULE_POWER_STATE_ON:
        case MODULE_POWER_STATE_OFF:
        case MODULE_POWER_STATE_STANDBY:
            break;
        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    switch(data->module_id)
    {
        case MODULE_ID_BDP:
            bdp_power.bdp_num = data->dev_num;
            bdp_power.bdp_power_state = data->power_state;
            return(svsBdpPowerSet(&bdp_power, blocking, timeout_ms, callback));
            break;

        case MODULE_ID_CCR:
        case MODULE_ID_KR:
        case MODULE_ID_PRT:
        case MODULE_ID_SCM:
        case MODULE_ID_WIM:
        case MODULE_ID_MODEM:
            rc = ERR_NOT_IMPLEMENTED;
            break;

        default:
            break;
    }

    return(errUpdate(rc));
}

svs_err_t *svsModulePowerGet(module_power_t *data, int timeout_ms)
{
    int rc = ERR_FAIL;
    bdp_power_t bdp_power;

    if(data == 0)
    {
        logError("data is null");
        return(errUpdate(ERR_FAIL));
    }

    switch(data->module_id)
    {
        case MODULE_ID_BDP:
            bdp_power.bdp_num = data->dev_num;
            return(svsBdpPowerGet(&bdp_power, timeout_ms));
            break;

        case MODULE_ID_CCR:
        case MODULE_ID_KR:
        case MODULE_ID_PRT:
        case MODULE_ID_SCM:
        case MODULE_ID_WIM:
        case MODULE_ID_MODEM:
            rc = ERR_NOT_IMPLEMENTED;
            break;

        default:
            break;
    }

    return(errUpdate(rc));
}

svs_err_t *svsModuleMsnGet(module_msn_t *data, int timeout_ms)
{
    svs_err_t *svs_err;
    int rc = ERR_FAIL;
    bdp_address_t bdp_address;

    svs_err = errUpdate(rc);

    if(data == 0)
    {
        logError("data is null");
        return(errUpdate(ERR_FAIL));
    }
    if(data->msn_addr.msn == 0)
    {
        logError("data->msn_addr.msn is null");
        return(errUpdate(ERR_FAIL));
    }

    switch(data->module_id)
    {
        case MODULE_ID_BDP:
            bdp_address.bdp_num = data->dev_num;
            svs_err = svsBdpMsnGet(&bdp_address);
            if(svs_err->code == ERR_PASS)
            {
                memcpy(data->msn_addr.msn, bdp_address.bdp_addr.octet, MSN_LENGTH);
            }
            break;

        case MODULE_ID_CCR:
        case MODULE_ID_KR:
        case MODULE_ID_PRT:
        case MODULE_ID_SCM:
        case MODULE_ID_WIM:
        case MODULE_ID_MODEM:
            rc = ERR_NOT_IMPLEMENTED;
            svs_err = errUpdate(rc);
            break;

        default:
            break;
    }

    return(svs_err);
}

// ------------------------------------------------------------------
//  BDP
// ------------------------------------------------------------------

svs_err_t *svsBdpScan(void)
{
    int rc = ERR_PASS;
    uint16_t bdp_max = 0;

    // Check if scan was done already
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // No BDPs detected, force a scan
    if(bdp_max == 0)
    {
        return(svsBdpScanForce());
    }

    return(errUpdate(rc));
}

// Perform BDP scan to retreive BDP ID and assign them to the
svs_err_t *svsBdpScanForce(void)
{
    int rc = ERR_PASS;
    svs_err_t *svs_err;
    uint16_t bdp_max = 0;
    uint16_t bdp_max_old;
    bdp_address_t bdp_address;

    svsBdpMsnFlush();

    // send broadcast command to ask ALL BDPs to send their IDs
    bdp_address.bdp_num = BDP_NUM_ALL;
    int retry = 0;
    // perform several iterations to make sure all BDPs respond to the ARP request
    do
    {   // perform the ARP request
        svs_err = svsBdpMsnForceGet(&bdp_address, BLOCKING_OFF, 0, 0);
        if((svs_err->code != ERR_PASS))
        {
            rc = svs_err->code;
            break;
        }

        int64_t tstart;
        tstart = svsTimeGet_ms();
        do
        {   // check to see how many BDPs responded
            bdp_max_old = bdp_max;

            rc = svsBdpAvailableGet(&bdp_max);
            if(rc != ERR_PASS)
            {
                return(errUpdate(ERR_FAIL));
            }
            // tell the BDPs who responded to go quiet,
            // we need to let the BDP timeout so they don't get stuck in the quiet mode forever
            // XXX

            // at this point more BDPs should have responded, if so we keep on getting bdp_max
            // until bdp_max stays constant for at least the time chosen in usleep() below
            // it is expected for the BDPs to respond within a few 100ms

            if(bdp_max_old != bdp_max)
            {   // update time as we detected a change
                tstart = svsTimeGet_ms();
                logInfo("BDPs detected so far: %d", bdp_max);
            }
            if(svsTimeGet_ms() - tstart >= 1000)
            {   // keep on retrying until this time has been reached
                // we expect to have responses between 0 and 700ms
                break;
            }
            // allow some time before the next request, most messages will be sent within a few 100ms
            // so we wait a little longer before checking again
            usleep(10000);
        } while(1);

        // retry this process several times, this will guarantee that all BDPs were able to send their data
        retry++;
        logDebug("retrying...");
    } while(retry < 10); // loop for as long as there are new BDPs responding

    // tell the BDPs who went quiet to start talking again
    // XXX

    if(bdp_max == 0)
    {
        logWarning("No BDPs detected");
    }

    logInfo("Total BDPs detected: %d", bdp_max);

    return(errUpdate(rc));
}

svs_err_t *svsBdpLedSet(bdp_led_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    bdp_msg_id_t msgId;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_led_state)
    {
        case BDP_LED_STATE_OFF:
        case BDP_LED_STATE_ON:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_led_color)
    {
        case BDP_LED_COLOR_GREEN:
            msgId = MSG_ID_BDP_GRN_LED_SET;
            break;
        case BDP_LED_COLOR_RED:
            msgId = MSG_ID_BDP_RED_LED_SET;
            break;
        case BDP_LED_COLOR_YELLOW:
            msgId = MSG_ID_BDP_YLW_LED_SET;
            break;
        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_led_set_msg_req_t req;
    bdp_led_set_msg_rsp_t rsp;
    req.bdp_led_state = data->bdp_led_state;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, msgId, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpLedGet(bdp_led_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    return(errUpdate(ERR_NOT_IMPLEMENTED));
}

svs_err_t *svsBdpPowerSet(bdp_power_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_power_state)
    {
        case MODULE_POWER_STATE_ON:
        case MODULE_POWER_STATE_OFF:
        case MODULE_POWER_STATE_STANDBY:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_power_set_msg_req_t req;
    bdp_power_set_msg_rsp_t rsp;
    req.bdp_power_state = data->bdp_power_state;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_POWER_SET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpPowerGet(bdp_power_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_power_state)
    {
        case MODULE_POWER_STATE_ON:
        case MODULE_POWER_STATE_OFF:
        case MODULE_POWER_STATE_STANDBY:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_power_get_msg_req_t req;
    bdp_power_get_msg_rsp_t rsp;
    // Send message to the SVS server, not BDP, we store the state in the server for this message
    rc = svsSocketServerSvsTransferSafe(data->bdp_num, SVS_MSG_ID_BDP_POWER_GET, timeout_ms, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->bdp_power_state   = rsp.bdp_power_state;
    rc                      = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpBikeLockGet(bdp_bike_lock_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_bike_lock_get_msg_req_t req;
    bdp_bike_lock_get_msg_rsp_t rsp;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_BIKE_LOCK_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    logDebug("lock state reported from BDP: %d\n", rsp.bdp_bike_lock_state);
    data->bdp_bike_lock_state = rsp.bdp_bike_lock_state;
    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpBikeLockSet(bdp_bike_lock_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_bike_lock_state)
    {
        case BDP_BIKE_LOCK_ENGAGE:
        case BDP_BIKE_LOCK_DISENGAGE:
        case BDP_BIKE_LOCK_FORCE_ENGAGE:
        case BDP_BIKE_LOCK_FORCE_DISENGAGE:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_bike_lock_set_msg_req_t req;
    bdp_bike_lock_set_msg_rsp_t rsp;
    req.bdp_bike_lock_state = data->bdp_bike_lock_state;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_BIKE_LOCK_SET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpBuzzerGet(bdp_buzzer_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    return(errUpdate(ERR_NOT_IMPLEMENTED));
}

svs_err_t *svsBdpBuzzerSet(bdp_buzzer_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_buzzer_state)
    {
        case BDP_BUZZER_STATE_OFF:
        case BDP_BUZZER_STATE_ON:
        case BDP_BUZZER_STATE_BEEP:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_buzzer_set_msg_req_t req;
    bdp_buzzer_set_msg_rsp_t rsp;
    req.bdp_buzzer_state = data->bdp_buzzer_state;
    //req.freq_hz          = data->freq_hz;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_BUZZER_SET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpLockKeyGet(bdp_unlock_code_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->unlock_code == 0)
    {
        logError("data->unlock_code null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_unlock_code_get_msg_req_t req;
    bdp_unlock_code_get_msg_rsp_t rsp;

    memset(&rsp, 0, sizeof(rsp));

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_UNLOCK_CODE_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->unlock_code_num = rsp.unlock_code_num;
#if 1
    int i;
    for(i=0; i<rsp.unlock_code_num; i++)
    {
        data->unlock_code[i] = rsp.unlock_code[i];
    }
#else
    // this does not work due to packing
    memcpy(data->unlock_code, rsp.unlock_code, rsp.unlock_code_num);
#endif
    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpRfidGet(bdp_rfid_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->rfid_uid == 0)
    {
        logError("data->rfid_uid null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_rfid_get_msg_req_t req;
    bdp_rfid_get_msg_rsp_t rsp;

    // set the reader to get data for
    req.rfid_reader = data->rfid_reader;

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_RFID_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->rfid_uid_len  = rsp.rfid_uid_len;
    data->rfid_type     = rsp.rfid_type;
    memcpy(data->rfid_uid, rsp.rfid_uid, rsp.rfid_uid_len);
    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpChangeMode(bdp_change_mode_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_change_mode_msg_req_t req;
    bdp_change_mode_msg_rsp_t rsp;
    req.mode = data->mode;

    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_CHANGE_MODE, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpGetMode(bdp_get_mode_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_get_mode_msg_req_t req;
    bdp_get_mode_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_GET_MODE, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->mode = rsp.mode;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpGetApplicationRev(bdp_get_rev_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_get_rev_msg_req_t req;
    bdp_get_rev_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_GET_APPLICATION_REV, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->rev = rsp.rev;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpGetBootblockRev(bdp_get_rev_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_get_rev_msg_req_t req;
    bdp_get_rev_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_GET_BOOTBLOCK_REV, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->rev = rsp.rev;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpFirmwareUpgrade(bdp_firmware_upgrade_t *data, int timeout_ms)
{
    int      rc;
    uint16_t bdp_max, len,last_len=0;
    uint8_t  tries;
    int      timeout;

    // check pointer
    if(data->filename == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_firmware_upgrade_msg_req_t req;
    bdp_firmware_upgrade_msg_rsp_t rsp;

    // Open the file
    FILE *imgFd;
    imgFd = fopen(data->filename, "r");
    if(imgFd == 0)
    {
        logError("failed to open file: %s", strerror(errno));
        return(errUpdate(ERR_FAIL));
    }

    req.offset = 0; // assume we're starting at the  start of the file
    if(data->timeout_erase == 0)
    {
        timeout = timeout_ms;
    }
    else
    {
        timeout = data->timeout_erase;
    }

    logDebug("BDP%d application programming", data->bdp_num, req.len, req.offset);

    do
    {
        memset(req.data, 0, FIRMWARE_IMAGE_BLOCK_MAX);
        // Read file in blocks
        len = fread(req.data, sizeof(uint8_t), FIRMWARE_IMAGE_BLOCK_MAX, imgFd);
        //logDebug("fread returned len: %d", len);
        if(feof(imgFd) != 0)
        {   // are we done?  Only if the last block wasn't full
            // else we have to send a zero length block
            if(last_len != FIRMWARE_IMAGE_BLOCK_MAX)
                break;
        }
        else
        {
            if(ferror(imgFd))
            {
                logError("failed to read file: %s", strerror(errno));
                rc = ERR_FAIL;
                break;
            }
        }
        req.len = len;
        //logDebug("BDP%d sending %d bytes @ %08X", data->bdp_num, req.len, req.offset);
        printf(".");fflush(stdout);

        // Send block as message
        tries = 0;
        do
        {
            usleep(100000); // minimum 100ms delay between packets
            tries++;
            rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_FIRMWARE_IMAGE_BLOCK_SEND, BLOCKING_ON, timeout, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
            if(rc == ERR_PASS)
            {
                rc = rsp.status; // get the status from the BDP
            }
        } while((tries < data->retries) && (rc != ERR_PASS));


        // its definitely no longer the first packet
        // so all erases are done, use the 'normal' timeout from
        // here
        if(data->timeout_normal == 0)
        {
            timeout = timeout_ms;
        }
        else
        {
            timeout = data->timeout_normal;
        }

        // update the file offset for the next pass
        // by the # of bytes read from the file
        req.offset += req.len;
        last_len    = req.len;
    } while((rc == ERR_PASS) && (req.len > 0));

    printf("%08X\n", req.offset);fflush(stdout);

    fclose(imgFd);

    return(errUpdate(rc));
}

//
// upload a temporary copy of the bootblock to unused portion of memory in the BDP
// for later use in the BOOTBLOCK_INSTALL command
//
svs_err_t *svsBdpBootblockUpload(bdp_firmware_upgrade_t *data, int timeout_ms)
{
    int      rc;
    uint16_t bdp_max, len,last_len=0;
    uint8_t  tries;
    int      timeout;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // check pointer
    if(data->filename == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_firmware_upgrade_msg_req_t req;
    bdp_firmware_upgrade_msg_rsp_t rsp;

    // Open the file
    FILE *imgFd;
    imgFd = fopen(data->filename, "r");
    if(imgFd == 0)
    {
        logError("failed to open file: %s", strerror(errno));
        return(errUpdate(ERR_FAIL));
    }

    req.offset = 0; // assume we're starting at the  start of the file
    if(data->timeout_erase == 0)
    {
        timeout = timeout_ms;
    }
    else
    {
        timeout = data->timeout_erase;
    }

    logDebug("BDP%d bootblock programming", data->bdp_num, req.len, req.offset);

    do
    {
        memset(req.data, 0, FIRMWARE_IMAGE_BLOCK_MAX);
        // Read file in blocks
        len = fread(req.data, sizeof(uint8_t), FIRMWARE_IMAGE_BLOCK_MAX, imgFd);
        //logDebug("fread returned len: %d", len);
        if(feof(imgFd) != 0)
        {   // are we done?  Only if the last block wasn't full
            // else we have to send a zero length block
            if(last_len != FIRMWARE_IMAGE_BLOCK_MAX)
                break;
        }
        else
        {
            if(ferror(imgFd))
            {
                logError("failed to read file: %s", strerror(errno));
                rc = ERR_FAIL;
                break;
            }
        }
        req.len = len;
        //logDebug("BDP%d sending %d bytes @ %08X", data->bdp_num, req.len, req.offset);
        printf(".");fflush(stdout);

        // Send block as message
        tries = 0;
        do
        {
            usleep(100000); // minimum 100ms delay between packets
            tries++;
            rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_BOOTBLOCK_IMAGE_UPLOAD, BLOCKING_ON, timeout, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
            if(rc == ERR_PASS)
            {
                rc = rsp.status; // get the status from the BDP
            }
        } while((tries < data->retries) && (rc != ERR_PASS));


        // its definitely no longer the first packet
        // so all erases are done, use the 'normal' timeout from
        // here
        if(data->timeout_normal == 0)
        {
            timeout = timeout_ms;
        }
        else
        {
            timeout = data->timeout_normal;
        }

        // update the file offset for the next pass
        // by the # of bytes read from the file
        req.offset += req.len;
        last_len    = req.len;
    } while((rc == ERR_PASS) && (req.len > 0));

    printf("%08X\n", req.offset);fflush(stdout);

    fclose(imgFd);

    return(errUpdate(rc));
}

//
// tell the BDP to copy the recently send bootblock image from scratch memory
// to its proper location.
//
svs_err_t *svsBdpBootblockInstall(bdp_bootblock_install_t *data, int timeout_ms)
{
    int      rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_bootblock_install_msg_req_t req;
    bdp_bootblock_install_msg_rsp_t rsp;
    memset(&req, 0, sizeof(bdp_bootblock_install_msg_req_t));
    memset(&rsp, 0, sizeof(bdp_bootblock_install_msg_rsp_t));

    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_BOOTBLOCK_IMAGE_INSTALL, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;
    return(errUpdate(rc));
}

//
// Ask a BDP to compute a 16bit CRC of a region of its memory
//
svs_err_t *svsBdpMemoryCrc(bdp_memory_crc_t *data, int timeout_ms)
{
    int      rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_memory_crc_msg_req_t req;
    bdp_memory_crc_msg_rsp_t rsp;
    memset(&req, 0, sizeof(bdp_memory_crc_msg_req_t));
    memset(&rsp, 0, sizeof(bdp_memory_crc_msg_rsp_t));

    req.bdp_num         = data->bdp_num;
    req.start_address   = data->start_address;
    req.length_in_bytes = data->length_in_bytes;

    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_MEMORY_CRC, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->returned_crc = rsp.crc;
    rc = rsp.status;
    return(errUpdate(rc));
}

//
// Get the MSN Address directly from the BDP
//
svs_err_t *svsBdpMsnForceGet(bdp_address_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->bdp_addr.octet == 0)
    {
        logError("data->bdp_addr.octet null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    // this is a special case where we only check for the possible maximum BDPs
    // in the system rather than the ones available
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= BDP_MAX)
        {
            logError("bdp number out of range %d %d", data->bdp_num, BDP_MAX);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_address_get_msg_req_t req;
    bdp_address_get_msg_rsp_t rsp;
    memset(&req, 0, sizeof(bdp_address_get_msg_req_t));
    memset(&rsp, 0, sizeof(bdp_address_get_msg_rsp_t));

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_ADDR_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    if(rc == ERR_PASS)
    {
        memcpy(data->bdp_addr.octet, rsp.bdp_addr.octet, BDP_MSG_ADDR_LENGTH);
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpMsnSet(bdp_address_t *data, int timeout_ms)
{
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    return(errUpdate(ERR_NOT_IMPLEMENTED));
}

//
// Get the MSN from the ARP table that is cached within the BDP module
// This table is initially filled by performing the svsBdpScan() which does direct access
// to the BDP devices to retrieve the MSNs from the physical BDPs.
//
svs_err_t *svsBdpMsnGet(bdp_address_t *data)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->bdp_addr.octet == 0)
    {
        logError("data->bdp_addr.octet null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    // this is a special case where we only check for the possible maximum BDPs
    // in the system rather than the ones available
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= BDP_MAX)
        {
            logError("bdp number out of range %d %d", data->bdp_num, BDP_MAX);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_address_get_msg_req_t req;
    bdp_address_get_msg_rsp_t rsp;
    memset(&req, 0, sizeof(bdp_address_get_msg_req_t));
    memset(&rsp, 0, sizeof(bdp_address_get_msg_rsp_t));

    // Send message
    rc = svsSocketServerSvsTransferSafe(data->bdp_num, SVS_MSG_ID_BDP_MSN_GET, 10, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    if(rc == ERR_PASS)
    {
        memcpy(data->bdp_addr.octet, rsp.bdp_addr.octet, BDP_MSG_ADDR_LENGTH);
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpMsnFlush(void)
{
    int rc;

    // Send message
    rc = svsSocketServerSvsTransferSafe(0, SVS_MSG_ID_BDP_MSN_FLUSH, TIMEOUT_DEFAULT, 0, 0, 0, 0);
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpSwitchGet(bdp_switch_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc = ERR_PASS;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->bdp_switch_state == 0)
    {
        logError("data->bdp_switch_state null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_switch_get_msg_req_t req;
    bdp_switch_get_msg_rsp_t rsp;

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_SWITCH_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    // copy data into the API structure, memspy did not work (most likely the enum type)
    int i;
    for(i=0; i<BDP_SWITCH_MAX; i++)
    {
        data->bdp_switch_state[i] = rsp.bdp_switch_state[i];
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpJtagDebugSet(bdp_jtag_debug_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }
    switch(data->bdp_jtag_debug_state)
    {
        case BDP_JTAG_DEBUG_STATE_OFF:
        case BDP_JTAG_DEBUG_STATE_ON:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_jtag_debug_set_msg_req_t req;
    bdp_jtag_debug_set_msg_rsp_t rsp;
    req.bdp_jtag_debug_state = data->bdp_jtag_debug_state;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_JTAG_DEBUG_SET, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpConsole(bdp_console_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->payload == 0)
    {
        logError("data->payload null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_console_msg_req_t req;
    bdp_console_msg_rsp_t rsp;

    memcpy(req.payload, data->payload, BDP_CONSOLE_PAYLOAD_MAX);
    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_CONSOLE, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            memcpy(data->payload, rsp.payload, BDP_CONSOLE_PAYLOAD_MAX);
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpStatsGet(bdp_get_stats_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->stats == 0)
    {
        logError("data->stats null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_get_stats_msg_req_t req;
    bdp_get_stats_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_GET_STATS, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc == ERR_PASS)
    {   // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            memcpy(data->stats, rsp.stats, sizeof(rsp.stats));
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpEcho(bdp_echo_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->payload == 0)
    {
        logError("data->payload null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        logError("BdpAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num != BDP_NUM_ALL)
    {
        if(data->bdp_num >= bdp_max)
        {
            logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    bdp_echo_msg_req_t req;
    bdp_echo_msg_rsp_t rsp;

    memcpy(req.payload, data->payload, BDP_ECHO_PAYLOAD_MAX);
    // Send block as message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_ECHO, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {
        // get the status from the BDP
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            memcpy(data->payload, rsp.payload, BDP_ECHO_PAYLOAD_MAX);
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsBdpMotorGet(bdp_motor_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_motor_get_msg_req_t req;
    bdp_motor_get_msg_rsp_t rsp;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_MOTOR_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    // get the status from the BDP
    data->timeout_ms         = rsp.timeout_ms;
    data->driver_on_timeout  = rsp.driver_on_timeout;
    data->driver_off_timeout = rsp.driver_off_timeout;
    data->retry_after_timeout= rsp.retry_after_timeout;

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpMotorSet(bdp_motor_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_motor_set_msg_req_t req;
    bdp_motor_set_msg_rsp_t rsp;

    req.timeout_ms          = data->timeout_ms;
    req.driver_on_timeout   = data->driver_on_timeout;
    req.driver_off_timeout  = data->driver_off_timeout;
    req.retry_after_timeout = data->retry_after_timeout;

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_MOTOR_SET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpCompatibilityGet(bdp_compatibility_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_compatibility_get_msg_req_t req;
    bdp_compatibility_get_msg_rsp_t rsp;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_COMPATIBILITY_GET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    // get the status from the BDP
    data->rev = rsp.rev;

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpCompatibilitySet(bdp_compatibility_t *data, blocking_t blocking, int timeout_ms, callback_fn_t callback)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_compatibility_set_msg_req_t req;
    bdp_compatibility_set_msg_rsp_t rsp;

    req.rev = data->rev;

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_COMPATIBILITY_SET, blocking, timeout_ms, 0, callback, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpCommsGet(bdp_comms_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_comms_get_msg_req_t req;
    bdp_comms_get_msg_rsp_t rsp;
    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_COMMS_GET, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    // get the status from the BDP
    data->tx_ctrl = rsp.tx_ctrl;

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsBdpCommsSet(bdp_comms_t *data, int timeout_ms)
{
    int rc;
    uint16_t bdp_max;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsBdpAvailableGet(&bdp_max);
    if(rc != ERR_PASS)
    {
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->bdp_num >= bdp_max)
    {
        logError("bdp number out of range %d %d", data->bdp_num, bdp_max);
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    bdp_comms_set_msg_req_t req;
    bdp_comms_set_msg_rsp_t rsp;

    req.tx_ctrl = data->tx_ctrl;

    // Send message
    rc = svsSocketServerBdpTransferSafe(data->bdp_num, MSG_ID_BDP_COMMS_SET, BLOCKING_ON, timeout_ms, 0, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

// ------------------------------------------------------------------
//  KR
// ------------------------------------------------------------------

svs_err_t *svsKrAddressGet(kr_address_t *data, int timeout_ms)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->kr_addr.octet == 0)
    {
        logError("data->kr_addr.octet null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    // this is a special case where we only check for the possible maximum BDPs
    // in the system rather than the ones available
    if(data->kr_num != KR_NUM_ALL)
    {
        if(data->kr_num >= KR_MAX)
        {
            logError("kr number out of range %d %d", data->kr_num, KR_MAX);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    kr_address_get_msg_req_t req;
    kr_address_get_msg_rsp_t rsp;
    // Send message
    rc = svsSocketServerKrTransferSafe(data->kr_num, MSG_ID_KR_ADDR_GET, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    if(rc == ERR_PASS)
    {
        memcpy(data->kr_addr.octet, rsp.kr_addr.octet, KR_MSG_ADDR_LENGTH);
    }

    return(errUpdate(rc));
}

svs_err_t *svsKrAddressSet(kr_address_t *data, int timeout_ms)
{
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    return(errUpdate(ERR_NOT_IMPLEMENTED));
}

svs_err_t *svsKrPowerSet(kr_power_t *data, int timeout_ms)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    switch(data->kr_power_state)
    {
        case MODULE_POWER_STATE_ON:
        case MODULE_POWER_STATE_OFF:
        case MODULE_POWER_STATE_STANDBY:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    kr_power_set_msg_req_t req;
    kr_power_set_msg_rsp_t rsp;
    req.kr_power_state = data->kr_power_state;
    // Send message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_POWER_SET, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsKrPowerGet(kr_power_t *data, int timeout_ms)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    switch(data->kr_power_state)
    {
        case MODULE_POWER_STATE_ON:
        case MODULE_POWER_STATE_OFF:
        case MODULE_POWER_STATE_STANDBY:
            break;

        default:
            logError("invalid state");
            return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    kr_power_get_msg_req_t req;
    kr_power_get_msg_rsp_t rsp;
    // Send message to the SVS server, not KR, we store the state in the server for this message
    rc = svsSocketServerSvsTransferSafe(0, SVS_MSG_ID_KR_POWER_GET, timeout_ms, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->kr_power_state    = rsp.kr_power_state;
    rc                      = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsKrRfidGet(kr_rfid_t *data, int timeout_ms)
{
    int rc;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->rfid_uid == 0)
    {
        logError("data->rfid_uid null pointer");
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    kr_rfid_get_msg_req_t req;
    kr_rfid_get_msg_rsp_t rsp;

    // set the reader to get data for
    req.rfid_reader = data->rfid_reader;

    // Send message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_RFID_GET, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->rfid_uid_len  = rsp.rfid_uid_len;
    data->rfid_type     = rsp.rfid_type;
    memcpy(data->rfid_uid, rsp.rfid_uid, rsp.rfid_uid_len);
    rc = rsp.status;

    return(errUpdate(rc));
}

svs_err_t *svsKrChangeMode(kr_change_mode_t *data, int timeout_ms)
{
    int rc;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // prepare the message structure
    kr_change_mode_msg_req_t req;
    kr_change_mode_msg_rsp_t rsp;
    req.mode = data->mode;

    // Send block as message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_CHANGE_MODE, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc == ERR_PASS)
    {   // get the status from the KR
        rc = rsp.status;
    }

    return(errUpdate(rc));
}

svs_err_t *svsKrGetMode(kr_get_mode_t *data, int timeout_ms)
{
    int rc;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // prepare the message structure
    kr_get_mode_msg_req_t req;
    kr_get_mode_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_GET_MODE, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {   // get the status from the KR
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->mode = rsp.mode;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsKrGetApplicationRev(kr_get_rev_t *data, int timeout_ms)
{
    int rc;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // prepare the message structure
    kr_get_rev_msg_req_t req;
    kr_get_rev_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_GET_APPLICATION_REV, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {   // get the status from the KR
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->rev = rsp.rev;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsKrGetBootblockRev(kr_get_rev_t *data, int timeout_ms)
{
    int rc;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    // prepare the message structure
    kr_get_rev_msg_req_t req;
    kr_get_rev_msg_rsp_t rsp;

    // Send block as message
    rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_GET_BOOTBLOCK_REV, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {   // get the status from the KR
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            data->rev = rsp.rev;
        }
    }

    return(errUpdate(rc));
}

svs_err_t *svsKrFirmwareUpgrade(kr_firmware_upgrade_t *data, int timeout_ms)
{
    int      rc;
    uint16_t len, last_len=0;
    uint8_t  tries;
    int      timeout;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->filename == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }

    // prepare the message structure
    kr_firmware_upgrade_msg_req_t req;
    kr_firmware_upgrade_msg_rsp_t rsp;

    // Open the file
    FILE *imgFd;
    imgFd = fopen(data->filename, "r");
    if(imgFd == 0)
    {
        logError("failed to open file: %s", strerror(errno));
        return(errUpdate(ERR_FAIL));
    }

    req.offset = 0; // assume we're starting at the  start of the file
    if(data->timeout_erase == 0)
    {
        timeout = timeout_ms;
    }
    else
    {
        timeout = data->timeout_erase;
    }

    do
    {
        memset(req.data, 0, FIRMWARE_IMAGE_BLOCK_MAX);
        // Read file in blocks
        len = fread(req.data, sizeof(uint8_t), FIRMWARE_IMAGE_BLOCK_MAX, imgFd);
        //logDebug("fread returned len: %d", len);
        if(feof(imgFd) != 0)
        {   // are we done?  Only if the last block wasn't full
            // else we have to send a zero length block
            if(last_len != FIRMWARE_IMAGE_BLOCK_MAX)
                break;
        }
        else
        {
            if(ferror(imgFd))
            {
                logError("failed to read file: %s", strerror(errno));
                rc = ERR_FAIL;
                break;
            }
        }
        req.len = len;
        logDebug("sending %d bytes @ %08X", req.len, req.offset);

        // Send block as message
        tries = 0;
        do
        {
            tries++;
            rc = svsSocketServerKrTransferSafe(0, MSG_ID_KR_FIRMWARE_IMAGE_BLOCK_SEND, timeout, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
            if(rc == ERR_PASS)
            {
                rc = rsp.status; // get the status from the KR
            }
        } while((tries < data->retries) && (rc != ERR_PASS));


        // its deffinately no longer the first packet
        // so all erases are done, use the 'normal' timeout from
        // here
        if(data->timeout_normal == 0)
        {
            timeout = timeout_ms;
        }
        else
        {
            timeout = data->timeout_normal;
        }

        // update the file offset for the next pass
        // by the # of bytes read from the file
        req.offset += req.len;
        last_len    = req.len;
    } while((rc == ERR_PASS) && (req.len > 0));

    fclose(imgFd);

    return(errUpdate(rc));
}

svs_err_t *svsKrScan(void)
{
    int rc = ERR_PASS;
    svs_err_t *svs_err;
    uint16_t kr_max = 0;
    uint16_t kr_max_old;
    kr_address_t kr_address;

    // send broadcast command to ask ALL KRs to send their IDs
    kr_address.kr_num = KR_NUM_ALL;

    // perform several iterations to make sure all KRs respond to the ARP request
    do
    {
        // check to see how many KRs responded
        kr_max_old = kr_max;
        rc = svsKrAvailableGet(&kr_max);
        if(rc != ERR_PASS)
        {
            return(errUpdate(ERR_FAIL));
        }
        // perform the ARP request
        svs_err = svsKrAddressGet(&kr_address, 1000);
        if((svs_err->code != ERR_PASS) && (svs_err->code != ERR_COMMS_TIMEOUT))
        {
            break;
        }
        // allow some times before the next request
        sleep(1);
    } while(kr_max_old != kr_max); // loop for as long as there are new KRs responding

    return(errUpdate(rc));
}

// Get available KR count
int svsKrAvailableGet(uint16_t *kr_available)
{
    int rc;
    svs_kr_available_rsp_t rsp;

    if(kr_available == 0)
    {
        logError("null pointer");
        return(ERR_FAIL);
    }

    rc = svsSocketServerSvsTransferSafe(0, SVS_MSG_ID_KR_AVAILABLE, TIMEOUT_DEFAULT, 0, 0, (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(rc);
    }

    *kr_available = rsp.kr_available;

    return(rc);
}

svs_err_t *svsKrEcho(kr_echo_t *data, int timeout_ms)
{
    int rc;
    uint16_t kr_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->payload == 0)
    {
        logError("data->payload null pointer");
        return(errUpdate(ERR_FAIL));
    }

    rc = svsKrAvailableGet(&kr_max);
    if(rc != ERR_PASS)
    {
        logError("KrAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->kr_num != KR_NUM_ALL)
    {
        if(data->kr_num >= kr_max)
        {
            logError("kr number out of range %d %d", data->kr_num, kr_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    kr_echo_msg_req_t req;
    kr_echo_msg_rsp_t rsp;

    memcpy(req.payload, data->payload, KR_ECHO_PAYLOAD_MAX);
    // Send block as message
    rc = svsSocketServerKrTransferSafe(data->kr_num, MSG_ID_KR_ECHO, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));

    if(rc == ERR_PASS)
    {
        // get the status from the KR
        rc = rsp.status;
        if(rc == ERR_PASS)
        {
            memcpy(data->payload, rsp.payload, KR_ECHO_PAYLOAD_MAX);
        }
    }

    return(errUpdate(rc));
}

//
// upload a temporary copy of the bootblock to unused portion of memory in the KR
// for later use in the BOOTBLOCK_INSTALL command
//
svs_err_t *svsKrBootblockUpload(kr_firmware_upgrade_t *data, int timeout_ms)
{
    int      rc;
    uint16_t kr_max, len,last_len=0;
    uint8_t  tries;
    int      timeout;

    // check pointer
    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    if(data->filename == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsKrAvailableGet(&kr_max);
    if(rc != ERR_PASS)
    {
        logError("KrAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->kr_num != KR_NUM_ALL)
    {
        if(data->kr_num >= kr_max)
        {
            logError("kr number out of range %d %d", data->kr_num, kr_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    kr_firmware_upgrade_msg_req_t req;
    kr_firmware_upgrade_msg_rsp_t rsp;

    // Open the file
    FILE *imgFd;
    imgFd = fopen(data->filename, "r");
    if(imgFd == 0)
    {
        logError("failed to open file: %s", strerror(errno));
        return(errUpdate(ERR_FAIL));
    }

    req.offset = 0; // assume we're starting at the  start of the file
    if(data->timeout_erase == 0)
    {
        timeout = timeout_ms;
    }
    else
    {
        timeout = data->timeout_erase;
    }

    logDebug("KR%d bootblock programming", data->kr_num, req.len, req.offset);

    do
    {
        memset(req.data, 0, FIRMWARE_IMAGE_BLOCK_MAX);
        // Read file in blocks
        len = fread(req.data, sizeof(uint8_t), FIRMWARE_IMAGE_BLOCK_MAX, imgFd);
        //logDebug("fread returned len: %d", len);
        if(feof(imgFd) != 0)
        {   // are we done?  Only if the last block wasn't full
            // else we have to send a zero length block
            if(last_len != FIRMWARE_IMAGE_BLOCK_MAX)
                break;
        }
        else
        {
            if(ferror(imgFd))
            {
                logError("failed to read file: %s", strerror(errno));
                rc = ERR_FAIL;
                break;
            }
        }
        req.len = len;
        //logDebug("KR%d sending %d bytes @ %08X", data->kr_num, req.len, req.offset);
        printf(".");fflush(stdout);

        // Send block as message
        tries = 0;
        do
        {
            usleep(100000); // minimum 100ms delay between packets
            tries++;
            rc = svsSocketServerKrTransferSafe(data->kr_num, MSG_ID_KR_BOOTBLOCK_IMAGE_UPLOAD, timeout, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
            if(rc == ERR_PASS)
            {
                rc = rsp.status; // get the status from the KR
            }
        } while((tries < data->retries) && (rc != ERR_PASS));


        // its definitely no longer the first packet
        // so all erases are done, use the 'normal' timeout from
        // here
        if(data->timeout_normal == 0)
        {
            timeout = timeout_ms;
        }
        else
        {
            timeout = data->timeout_normal;
        }

        // update the file offset for the next pass
        // by the # of bytes read from the file
        req.offset += req.len;
        last_len    = req.len;
    } while((rc == ERR_PASS) && (req.len > 0));

    printf("%08X\n", req.offset);fflush(stdout);

    fclose(imgFd);

    return(errUpdate(rc));
}

//
// tell the KR to copy the recently send bootblock image from scratch memory
// to its proper location.
//
svs_err_t *svsKrBootblockInstall(kr_bootblock_install_t *data, int timeout_ms)
{
    int      rc;
    uint16_t kr_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsKrAvailableGet(&kr_max);
    if(rc != ERR_PASS)
    {
        logError("KRAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->kr_num != KR_NUM_ALL)
    {
        if(data->kr_num >= kr_max)
        {
            logError("kr number out of range %d %d", data->kr_num, kr_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    kr_bootblock_install_msg_req_t req;
    kr_bootblock_install_msg_rsp_t rsp;
    memset(&req, 0, sizeof(kr_bootblock_install_msg_req_t));
    memset(&rsp, 0, sizeof(kr_bootblock_install_msg_rsp_t));

    rc = svsSocketServerKrTransferSafe(data->kr_num, MSG_ID_KR_BOOTBLOCK_IMAGE_INSTALL, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    rc = rsp.status;
    return(errUpdate(rc));
}
//
// Ask a KR to compute a 16bit CRC of a region of its memory
//
svs_err_t *svsKrMemoryCrc(kr_memory_crc_t *data, int timeout_ms)
{
    int      rc;
    uint16_t kr_max;

    if(data == 0)
    {
        logError("null pointer");
        return(errUpdate(ERR_FAIL));
    }
    rc = svsKrAvailableGet(&kr_max);
    if(rc != ERR_PASS)
    {
        logError("KrAvailableGet() failed");
        return(errUpdate(ERR_FAIL));
    }
    // validate structure fields
    if(data->kr_num != KR_NUM_ALL)
    {
        if(data->kr_num >= kr_max)
        {
            logError("kr number out of range %d %d", data->kr_num, kr_max);
            return(errUpdate(ERR_FAIL));
        }
    }

    // prepare the message structure
    kr_memory_crc_msg_req_t req;
    kr_memory_crc_msg_rsp_t rsp;
    memset(&req, 0, sizeof(kr_memory_crc_msg_req_t));
    memset(&rsp, 0, sizeof(kr_memory_crc_msg_rsp_t));

    req.kr_num          = data->kr_num;
    req.start_address   = data->start_address;
    req.length_in_bytes = data->length_in_bytes;

    rc = svsSocketServerKrTransferSafe(data->kr_num, MSG_ID_KR_MEMORY_CRC, timeout_ms, 0, (uint8_t *)&req, sizeof(req), (uint8_t *)&rsp, sizeof(rsp));
    if(rc != ERR_PASS)
    {
        return(errUpdate(rc));
    }

    data->returned_crc = rsp.crc;
    rc = rsp.status;
    return(errUpdate(rc));
}
