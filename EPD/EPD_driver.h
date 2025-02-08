/*****************************************************************************
* | File      	: DEV_Config.h
* | Author      : Waveshare team
* | Function    :	debug with prntf
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* |	This version:   V1.0
* | Date        :   2018-01-11
* | Info        :   Basic version
*
******************************************************************************/

#ifndef __EPD_DRIVER_H
#define __EPD_DRIVER_H

#include <stdint.h>
#include <stdlib.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)                    (sizeof(arr) / sizeof((arr)[0]))
#endif

/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

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
	uint16_t width;
	uint16_t height;
    void (*init)(void);                               /**< Initialize the e-Paper register */
    void (*clear)(void);                              /**< Clear screen */
    void (*send_command)(UBYTE Reg);                  /**< send command */
	void (*send_byte)(UBYTE Reg);                     /**< send byte */
    void (*send_data)(UBYTE *Data, UBYTE Len);        /**< send data */
    void (*write_image)(UBYTE *black, UBYTE *color, UWORD x, UWORD y, UWORD w, UWORD h); /**< write image */
    void (*display)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(void);                              /**< Enter sleep mode */
} epd_driver_t;

extern uint32_t EPD_MOSI_PIN;
extern uint32_t EPD_SCLK_PIN;
extern uint32_t EPD_CS_PIN;
extern uint32_t EPD_DC_PIN;
extern uint32_t EPD_RST_PIN;
extern uint32_t EPD_BUSY_PIN;
extern uint32_t EPD_BS_PIN;

/**
 * GPIO read and write
**/
#define DEV_Digital_Write(_pin, _value) nrf_gpio_pin_write(_pin, _value)
#define DEV_Digital_Read(_pin) nrf_gpio_pin_read(_pin)

/**
 * delay x ms
**/
#define DEV_Delay_ms(__xms) nrf_delay_ms(__xms);
#define DEV_Delay_us(__xus) nrf_delay_us(__xus);

UBYTE DEV_Module_Init(void);
void DEV_Module_Exit(void);

void DEV_SPI_WriteByte(UBYTE value);
void DEV_SPI_WriteBytes(UBYTE *value, UBYTE len);

void EPD_WriteCommand(UBYTE Reg);
void EPD_WriteByte(UBYTE Data);
void EPD_WriteData(UBYTE *Data, UBYTE Len);

epd_driver_t *epd_driver_get(void);
epd_driver_t *epd_driver_by_id(uint8_t id);
bool epd_driver_set(uint8_t id);

#endif
