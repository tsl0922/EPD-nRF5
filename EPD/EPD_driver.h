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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define BIT(n)  (1UL << (n))

// Display resolution
typedef enum
{
    EPD_RES_400x300,
    EPD_RES_320x300,
    EPD_RES_320x240,
    EPD_RES_200x300,
} epd_res_t;

/**@brief EPD driver structure.
 *
 * @details This structure contains epd driver functions.
 */
typedef struct
{
    void (*init)(epd_res_t res, bool bwr);            /**< Initialize the e-Paper register */
    void (*clear)(void);                              /**< Clear screen */
    void (*write_image)(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h); /**< write image */
    void (*refresh)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(void);                              /**< Enter sleep mode */
    int8_t (*read_temp)(void);                        /**< Read temperature from driver chip */
    void (*force_temp)(int8_t value);                 /**< Force temperature (will trigger OTP LUT switch) */
} epd_driver_t;

typedef enum
{
    EPD_UC8176_420_BW = 1,
    EPD_UC8176_420_BWR = 3,
    EPD_SSD1619_420_BWR = 2,
    EPD_SSD1619_420_BW = 4,
    EPD_UC8276_420_BWR = 5,
} epd_model_id_t;

typedef struct
{
    epd_model_id_t id;
    epd_driver_t *drv;
    epd_res_t res;
    bool bwr;
} epd_model_t;

extern uint32_t EPD_MOSI_PIN;
extern uint32_t EPD_SCLK_PIN;
extern uint32_t EPD_CS_PIN;
extern uint32_t EPD_DC_PIN;
extern uint32_t EPD_RST_PIN;
extern uint32_t EPD_BUSY_PIN;
extern uint32_t EPD_BS_PIN;
extern uint32_t EPD_EN_PIN;
extern uint32_t EPD_LED_PIN;

extern uint16_t EPD_WIDTH;
extern uint16_t EPD_HEIGHT;

extern bool EPD_BWR_MODE;

#define LOW             (0x0)
#define HIGH            (0x1)

#define INPUT           (0x0)
#define OUTPUT          (0x1)
#define INPUT_PULLUP    (0x2)
#define INPUT_PULLDOWN  (0x3)

// Arduino like function wrappers
void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t value);
uint32_t digitalRead(uint32_t pin);
void delay(uint32_t ms);

// GPIO
void EPD_GPIO_Init(void);
void EPD_GPIO_Uninit(void);

// Software SPI (read / write)
void EPD_SPI_WriteByte_SW(uint8_t data);
uint8_t EPD_SPI_ReadByte_SW(void);
void EPD_WriteCommand_SW(uint8_t Reg);
void EPD_WriteByte_SW(uint8_t Data);
uint8_t EPD_ReadByte_SW(void);

// Hardware SPI (write only)
void EPD_SPI_WriteByte(uint8_t value);
void EPD_SPI_WriteBytes(uint8_t *value, uint8_t len);
void EPD_WriteCommand(uint8_t Reg);
void EPD_WriteByte(uint8_t Data);
void EPD_WriteData(uint8_t *Data, uint8_t Len);

void EPD_Reset(uint32_t value, uint16_t duration);
void EPD_WaitBusy(uint32_t value, uint16_t timeout);

// lED
void EPD_LED_ON(void);
void EPD_LED_OFF(void);
void EPD_LED_TOGGLE(void);

epd_driver_t *epd_model_init(epd_model_id_t id);

#endif
