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
#include "EPD_driver.h"

uint32_t EPD_MOSI_PIN = 5;
uint32_t EPD_SCLK_PIN = 8;
uint32_t EPD_CS_PIN = 9;
uint32_t EPD_DC_PIN = 10;
uint32_t EPD_RST_PIN = 11;
uint32_t EPD_BUSY_PIN = 12;
uint32_t EPD_BS_PIN = 13;

static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

extern epd_driver_t epd_driver_4in2;
extern epd_driver_t epd_driver_4in2bv2;

/** EPD drivers */
static epd_driver_t *epd_drivers[] = {
    &epd_driver_4in2,
    &epd_driver_4in2bv2,
};

/**< current EPD driver */
static epd_driver_t *m_driver = NULL;

epd_driver_t *epd_driver_get(void)
{
    if (m_driver == NULL)
        m_driver = epd_drivers[0];
    return m_driver;
}

epd_driver_t *epd_driver_by_id(uint8_t id)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(epd_drivers); i++)
    {
      if (epd_drivers[i]->id == id)
      {
          return epd_drivers[i];
      }
    }
    return NULL;
}

bool epd_driver_set(uint8_t id)
{
    epd_driver_t *driver = epd_driver_by_id(id);
    if (driver )
    {
        m_driver = driver;
        return true;
    }
    return false;
}

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

void DEV_Module_Exit(void)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);

    //close 5V
    DEV_Digital_Write(EPD_RST_PIN, 0);

    nrf_drv_spi_uninit(&spi);
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

void EPD_WriteCommand(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

void EPD_WriteByte(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

void EPD_WriteData(UBYTE *Data, UBYTE Len)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteBytes(Data, Len);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}
