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
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <stdint.h>
#include <stdlib.h>

/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

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
UBYTE DEV_SPI_ReadByte(void);

#endif
