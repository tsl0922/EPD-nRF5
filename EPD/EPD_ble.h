/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
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

#ifndef EPD_BLE_H__
#define EPD_BLE_H__

#include "ble.h"
#include "ble_srv_common.h"
#include <stdint.h>
#include <stdbool.h>
#include "DEV_Config.h"

#define BLE_UUID_EPD_SERVICE  0x0001
#define EPD_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN
#define BLE_EPD_MAX_DATA_LEN  (GATT_MTU_SIZE_DEFAULT - 3) /**< Maximum length of data (in bytes) that can be transmitted to the peer. */

/**< EPD Service Configs */
typedef struct
{
    uint8_t mosi_pin;
    uint8_t sclk_pin;
    uint8_t cs_pin;
    uint8_t dc_pin;
    uint8_t rst_pin;
    uint8_t busy_pin;
    uint8_t bs_pin;
    uint8_t driver_id;
    uint8_t wakeup_pin;
    uint8_t led_pin;
    
    uint8_t reserved[6];
} epd_config_t;

/**< EPD Service command IDs. */
enum EPD_CMDS
{
    EPD_CMD_SET_PINS,                                 /**< set EPD pin mapping. */
    EPD_CMD_INIT,                                     /**< init EPD display driver */
    EPD_CMD_CLEAR,                                    /**< clear EPD screen */
    EPD_CMD_SEND_COMMAND,                             /**< send command to EPD */
    EPD_CMD_SEND_DATA,                                /**< send data to EPD */
    EPD_CMD_DISPLAY,                                  /**< diaplay EPD ram on screen */
    EPD_CMD_SLEEP,                                    /**< EPD enter sleep mode */

    EPD_CMD_SET_CONFIG = 0x90,                        /**< set full EPD config */
    EPD_CMD_SYS_RESET  = 0x91,                        /**< MCU reset */
    EPD_CMD_SYS_SLEEP  = 0x92,                        /**< MCU enter sleep mode */
    EPD_CMD_CFG_ERASE  = 0x99,                        /**< Erase config and reset */
};

/**< EPD driver IDs. */
enum EPD_DRIVER_IDS
{
    EPD_DRIVER_4IN2 = 1,
    EPD_DRIVER_4IN2_V2,
    EPD_DRIVER_4IN2B_V2,
};

/**@brief EPD driver structure.
 *
 * @details This structure contains epd driver functions.
 */
typedef struct
{
    uint8_t id;                                       /**< driver ID. */
    void (*init)(void);                               /**< Initialize the e-Paper register */
    void (*clear)(void);                              /**< Clear screen */
    void (*send_command)(UBYTE Reg);                  /**< send command */
    void (*send_data)(UBYTE *Data, UBYTE Len);        /**< send data */
    void (*display)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(void);                              /**< Enter sleep mode */
} epd_driver_t;

/**@brief EPD Service structure.
 *
 * @details This structure contains status information related to the service.
 */
typedef struct
{
    uint8_t                  uuid_type;               /**< UUID type for EPD Service Base UUID. */
    uint16_t                 service_handle;          /**< Handle of EPD Service (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t char_handles;            /**< Handles related to the EPD characteristic (as provided by the S110 SoftDevice). */
    uint16_t                 conn_handle;             /**< Handle of the current connection (as provided by the S110 SoftDevice). BLE_CONN_HANDLE_INVALID if not in a connection. */
    bool                     is_notification_enabled; /**< Variable to indicate if the peer has enabled notification of the RX characteristic.*/
    epd_driver_t             *driver;                 /**< current EPD driver */
    epd_config_t             config;                  /**< EPD config */
} ble_epd_t;

/**@brief Function for preparing sleep mode.
 *
 * @param[in] p_epd       EPD Service structure.
 */
void ble_epd_sleep_prepare(ble_epd_t * p_epd);

/**@brief Function for initializing the EPD Service.
 *
 * @param[out] p_epd      EPD Service structure. This structure must be supplied
 *                        by the application. It is initialized by this function and will
 *                        later be used to identify this particular service instance.
 * @param[in] p_epd_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was successfully initialized. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If either of the pointers p_epd or p_epd_init is NULL.
 */
uint32_t ble_epd_init(ble_epd_t * p_epd);

/**@brief Function for handling the EPD Service's BLE events.
 *
 * @details The EPD Service expects the application to call this function each time an
 * event is received from the S110 SoftDevice. This function processes the event if it
 * is relevant and calls the EPD Service event handler of the
 * application if necessary.
 *
 * @param[in] p_epd       EPD Service structure.
 * @param[in] p_ble_evt   Event received from the S110 SoftDevice.
 */
void ble_epd_on_ble_evt(ble_epd_t * p_epd, ble_evt_t * p_ble_evt);

/**@brief Function for sending a string to the peer.
 *
 * @details This function sends the input string as an RX characteristic notification to the
 *          peer.
 *
 * @param[in] p_epd       Pointer to the EPD Service structure.
 * @param[in] p_string    String to be sent.
 * @param[in] length      Length of the string.
 *
 * @retval NRF_SUCCESS If the string was sent successfully. Otherwise, an error code is returned.
 */
uint32_t ble_epd_string_send(ble_epd_t * p_epd, uint8_t * p_string, uint16_t length);

#endif // EPD_BLE_H__

/** @} */
