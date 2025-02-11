/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_nvmc.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "pstorage.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "app_util_platform.h"
#include "nrf_drv_gpiote.h"
#include "EPD_ble.h"
#include "Calendar.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT  1                                              /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define DEVICE_NAME                      "NRF_EPD"                                      /**< Name of device. Will be included in the advertising data. */
#define APP_ADV_INTERVAL                 320                                            /**< The advertising interval (in units of 0.625 ms. This value corresponds to 200 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS       120                                            /**< The advertising timeout (in units of seconds). */
#define APP_TIMER_PRESCALER              0                                              /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE          4                                              /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(7.5, UNIT_1_25_MS)               /**< Minimum connection interval (7.5 ms) */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(30, UNIT_1_25_MS)                /**< Maximum connection interval (30 ms). */
#define SLAVE_LATENCY                    6                                              /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(430, UNIT_10_MS)                 /**< Connection supervisory timeout (430 ms). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)     /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)    /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                              /**< Number of attempts before giving up the connection parameter negotiation. */

#define SCHED_MAX_EVENT_DATA_SIZE        0                                              /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                 10                                             /**< Maximum number of events in the scheduler queue. */

#define CLOCK_TIMER_INTERVAL             APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)     /**< Clock timer interval (ticks). */

#define DEAD_BEEF                        0xDEADBEEF                                     /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static uint16_t                          m_driver_refs = 0;
static uint16_t                          m_conn_handle = BLE_CONN_HANDLE_INVALID;       /**< Handle of the current connection. */
static ble_uuid_t                        m_adv_uuids[] = {{BLE_UUID_EPD_SERVICE, \
                                                           EPD_SERVICE_UUID_TYPE}};     /**< Universally unique service identifier. */
static ble_epd_t                         m_epd;                                         /**< Structure to identify the EPD Service. */
static uint32_t                          m_timestamp = 1735689600;                      /**< Current timestamp. */
static bool                              m_calendar_mode = false;                       /**< Whether we are in calendar mode */

APP_TIMER_DEF(m_clock_timer_id);                                                        /**< Clock timer. */

static void epd_driver_init()
{
    if (m_driver_refs == 0) {
        NRF_LOG_PRINTF("[EPD]: driver init\n");
        DEV_Module_Init();
    }
    m_driver_refs++;
}

static void epd_driver_exit()
{
    m_driver_refs--;
    if (m_driver_refs == 0) {
        NRF_LOG_PRINTF("[EPD]: driver exit\n");
        DEV_Module_Exit();
    }
}

static void calendar_update(void * p_event_data, uint16_t event_size)
{
    m_calendar_mode = true;
    epd_driver_init();
    m_epd.driver->init();
    DrawCalendar(m_timestamp);
    epd_driver_exit();
}

static uint32_t calendar_update_schedule(void)
{
    return app_sched_event_put(NULL, 0, calendar_update);
}

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void clock_timer_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    m_timestamp++;

    // Update calendar on 00:00:00
    if (m_calendar_mode && m_timestamp % 86400 == 0)
        calendar_update_schedule();
}

/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
    
    // Create timers.
    uint32_t err_code = app_timer_create(&m_clock_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                clock_timer_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
    // Start application timers.
    uint32_t err_code = app_timer_start(m_clock_timer_id, CLOCK_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

bool epd_cmd_callback(uint8_t cmd, uint8_t *data, uint16_t len)
{
    switch (cmd)
    {
        case EPD_CMD_SET_TIME:
            if (len < 4) {
                NRF_LOG_PRINTF("invalid time data!\n");
                return false;
            }

            NRF_LOG_PRINTF("time: %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
            if (len > 4) {
                NRF_LOG_PRINTF("timezone: %d\n", (int8_t)data[4]);
            }

            app_timer_stop(m_clock_timer_id);
            m_timestamp = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            m_timestamp += (len > 4 ? (int8_t)data[4] : 8) * 60 * 60; // timezone
            app_timer_start(m_clock_timer_id, CLOCK_TIMER_INTERVAL, NULL);

            calendar_update_schedule();
            return true;
        case EPD_CMD_CLEAR:
        case EPD_CMD_DISPLAY:
            m_calendar_mode = false;
            break;
        default:
            break;
    }
    return false;
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;

    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    memset(&m_epd, 0, sizeof(ble_epd_t));
    err_code = ble_epd_init(&m_epd, epd_cmd_callback);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    char                    device_name[20];
    ble_gap_addr_t          addr;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
    err_code = sd_ble_gap_address_get(&addr);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_PRINTF("Bluetooth MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   addr.addr[5], addr.addr[4], addr.addr[3],
                   addr.addr[2], addr.addr[1], addr.addr[0]);

    snprintf(device_name, 20, "%s_%02X%02X", DEVICE_NAME, addr.addr[1],addr.addr[0]);
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)device_name,
                                          strlen(device_name));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

static void advertising_start(void)
{
    NRF_LOG_PRINTF("advertising start\n");
    uint32_t err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    NRF_LOG_PRINTF("Entering deep sleep mode\n");

    // Prepare wakeup pin
    ble_epd_sleep_prepare(&m_epd);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    uint32_t err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

void gpiote_evt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    NRF_LOG_PRINTF("pin: %d, event: %d\n", pin, action);

    nrf_drv_gpiote_in_event_disable(pin);
    nrf_drv_gpiote_in_uninit(pin);
    nrf_drv_gpiote_uninit();

    advertising_start();
}

static void setup_wakeup_pin(nrf_drv_gpiote_pin_t pin) {
    NRF_LOG_PRINTF("Setting up wakeup pin\n");

    ret_code_t err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);

    err_code = nrf_drv_gpiote_in_init(pin, &config, gpiote_evt_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(pin, true);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            break;
        case BLE_ADV_EVT_IDLE:
            NRF_LOG_PRINTF("advertising timeout\n");
            if (m_calendar_mode) {
                setup_wakeup_pin(m_epd.config.wakeup_pin);
            } else {
                sleep_mode_enter();
            }
            break;
        default:
            break;
    }
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            epd_driver_init();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            epd_driver_exit();
            advertising_start();
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;
        
        case BLE_GATTC_EVT_TIMEOUT:
        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server and Client timeout events.
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;
        
        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(m_conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_epd_on_ble_evt(&m_epd, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, NULL);

    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    
    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);

    NRF_LOG_PRINTF("timestamp: %d\n", m_timestamp);
}

/**@brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    err_code = NRF_LOG_INIT();
    APP_ERROR_CHECK(err_code);

    NRF_LOG_PRINTF("start..\n");

    timers_init();
    ble_stack_init();
    scheduler_init();
    gap_params_init();
    services_init();
    advertising_init();
    conn_params_init();

    // Start execution.
    application_timers_start();

    advertising_start();

    NRF_LOG_PRINTF("done.\n");

    for (;;)
    {
        app_sched_execute();

        power_manage();
    }
}
