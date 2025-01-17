/*****************************************************************************
* | File        : DEV_Config.cpp
* | Author      : Waveshare team
* | Function    :
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* | This version:   V1.0
* | Date        :   2018-01-11
* | Info        :   Basic version
*
******************************************************************************/
#include "nrf_drv_spi.h"
#include "DEV_Config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

uint32_t EPD_MOSI_PIN = 5;
uint32_t EPD_SCLK_PIN = 8;
uint32_t EPD_CS_PIN = 9;
uint32_t EPD_DC_PIN = 10;
uint32_t EPD_RST_PIN = 11;
uint32_t EPD_BUSY_PIN = 12;
uint32_t EPD_BS_PIN = 13;

static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

/******************************************************************************
function: Initialize Arduino, Initialize Pins, and SPI
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
    nrf_gpio_cfg_output(EPD_CS_PIN);
    nrf_gpio_cfg_output(EPD_DC_PIN);
    nrf_gpio_cfg_output(EPD_RST_PIN);
    nrf_gpio_cfg_input(EPD_BUSY_PIN, NRF_GPIO_PIN_NOPULL);
  
    nrf_gpio_cfg_output(EPD_BS_PIN);
    DEV_Digital_Write(EPD_BS_PIN, 0);

    nrf_drv_spi_config_t spi_config =
    {
        .sck_pin      = EPD_SCLK_PIN,
        .mosi_pin     = EPD_MOSI_PIN,
        .miso_pin     = NRF_DRV_SPI_PIN_NOT_USED,
        .ss_pin       = NRF_DRV_SPI_PIN_NOT_USED,
        .frequency    = NRF_DRV_SPI_FREQ_4M,
        .mode         = NRF_DRV_SPI_MODE_0,
    };
    nrf_drv_spi_init(&spi, &spi_config, NULL);

    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_Digital_Write(EPD_RST_PIN, 1);
  
    return 0;
}

/*********************************************
function: Hardware interface
note:
  SPI4W_Write_Byte(value) : 
    Register hardware SPI
*********************************************/  
void DEV_SPI_WriteByte(UBYTE value)
{
    nrf_drv_spi_transfer(&spi, &value, 1, NULL, 0);
}

void DEV_SPI_WriteBytes(UBYTE *value, UBYTE len)
{
    nrf_drv_spi_transfer(&spi, value, len, NULL, 0);
}

UBYTE DEV_SPI_ReadByte(void)
{
    UBYTE value;
    nrf_drv_spi_transfer(&spi, NULL, 0, &value, 1);
    return value;
}

void DEV_Module_Exit(void)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);

    //close 5V
    DEV_Digital_Write(EPD_RST_PIN, 0);
  
    nrf_drv_spi_uninit(&spi);
}
