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

#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_spi.h"
#include "EPD_driver.h"
#include "nrf_log.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// GPIO Pins
uint32_t EPD_MOSI_PIN = 5;
uint32_t EPD_SCLK_PIN = 8;
uint32_t EPD_CS_PIN = 9;
uint32_t EPD_DC_PIN = 10;
uint32_t EPD_RST_PIN = 11;
uint32_t EPD_BUSY_PIN = 12;
uint32_t EPD_BS_PIN = 13;
uint32_t EPD_EN_PIN = 0xFF;
uint32_t EPD_LED_PIN = 0xFF;

// Display resolution
uint16_t EPD_WIDTH = 400;
uint16_t EPD_HEIGHT = 300;

// BWR mode
bool EPD_BWR_MODE = true;

// Arduino like function wrappers

void pinMode(uint32_t pin, uint32_t mode)
{
    switch (mode)
    {
        case INPUT:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
            break;
        case INPUT_PULLUP:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
            break;
        case INPUT_PULLDOWN:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
            break;
        case OUTPUT:
            nrf_gpio_cfg_output(pin);
            break;
        default:
            break;
    }
}

void digitalWrite(uint32_t pin, uint32_t value)
{
    if (value == LOW)
        nrf_gpio_pin_clear(pin);
    else
        nrf_gpio_pin_set(pin);
}

uint32_t digitalRead(uint32_t pin)
{
#if defined(S112)
    nrf_gpio_pin_dir_t dir = nrf_gpio_pin_dir_get(pin);
#else
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin);
    nrf_gpio_pin_dir_t dir = (nrf_gpio_pin_dir_t)((reg->PIN_CNF[pin] &
                                                   GPIO_PIN_CNF_DIR_Msk) >> GPIO_PIN_CNF_DIR_Pos);
#endif
    if (dir == NRF_GPIO_PIN_DIR_INPUT)
        return nrf_gpio_pin_read(pin);
    else
        return nrf_gpio_pin_out_read(pin);
}

void delay(uint32_t ms)
{
    nrf_delay_ms(ms);
}

// Hardware SPI (write only)

#define SPI_INSTANCE  0 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static bool spi_initialized = false;

static void EPD_SPI_Init(void)
{
    if (spi_initialized) return;
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin = EPD_SCLK_PIN;
    spi_config.mosi_pin = EPD_MOSI_PIN;
    spi_config.ss_pin = EPD_CS_PIN;
#if defined(S112)
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL, NULL));
#else
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL));
#endif
    spi_initialized = true;
}

static void EPD_SPI_Uninit(void)
{
    if (!spi_initialized) return;
    nrf_drv_spi_uninit(&spi);
    spi_initialized = false;
}

void EPD_SPI_WriteByte(uint8_t value)
{
    EPD_SPI_Init();
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, &value, 1, NULL, 0));
}

void EPD_SPI_WriteBytes(uint8_t *value, uint8_t len)
{
    EPD_SPI_Init();
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, value, len, NULL, 0));
}


// Software SPI (read / write)
void EPD_SPI_WriteByte_SW(uint8_t data)
{
    EPD_SPI_Uninit();
    pinMode(EPD_MOSI_PIN, OUTPUT);
    digitalWrite(EPD_CS_PIN, LOW);
    for (int i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0) digitalWrite(EPD_MOSI_PIN, LOW); 
        else                    digitalWrite(EPD_MOSI_PIN, HIGH);

        data <<= 1;
        digitalWrite(EPD_SCLK_PIN, HIGH);     
        digitalWrite(EPD_SCLK_PIN, LOW);
    }
    digitalWrite(EPD_CS_PIN, HIGH);
}

uint8_t EPD_SPI_ReadByte_SW(void)
{
    EPD_SPI_Uninit();
    uint8_t j = 0xff;
    pinMode(EPD_MOSI_PIN, INPUT);
    digitalWrite(EPD_CS_PIN, LOW);
    for (int i = 0; i < 8; i++)
    {
        j = j << 1;
        if (digitalRead(EPD_MOSI_PIN))  j = j | 0x01;
        else                            j = j & 0xfe;
        
        digitalWrite(EPD_SCLK_PIN, HIGH);     
        digitalWrite(EPD_SCLK_PIN, LOW);
    }
    digitalWrite(EPD_CS_PIN, HIGH);
    pinMode(EPD_MOSI_PIN, 1);
    return j;
}

void EPD_WriteCommand_SW(uint8_t Reg)
{
    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    EPD_SPI_WriteByte_SW(Reg);
    digitalWrite(EPD_CS_PIN, HIGH);
}

void EPD_WriteByte_SW(uint8_t Data)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    EPD_SPI_WriteByte_SW(Data);
    digitalWrite(EPD_CS_PIN, HIGH);
}

uint8_t EPD_ReadByte_SW(void)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    return EPD_SPI_ReadByte_SW();
}


// Hardware SPI
void EPD_WriteCommand(uint8_t Reg)
{
    digitalWrite(EPD_DC_PIN, LOW);
    EPD_SPI_WriteByte(Reg);
}

void EPD_WriteByte(uint8_t Data)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    EPD_SPI_WriteByte(Data);
}

void EPD_WriteData(uint8_t *Data, uint8_t Len)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    EPD_SPI_WriteBytes(Data, Len);
}

void EPD_Reset(uint32_t value, uint16_t duration)
{
    uint32_t rvalue = (value == LOW) ? HIGH : LOW;
    digitalWrite(EPD_RST_PIN, value);
    delay(10);
    digitalWrite(EPD_RST_PIN, rvalue);
    delay(duration);
    digitalWrite(EPD_RST_PIN, value);
    delay(duration);
}

void EPD_WaitBusy(uint32_t value, uint16_t timeout)
{
    NRF_LOG_DEBUG("[EPD]: check busy\n");
    while (digitalRead(EPD_BUSY_PIN) == value) {
        if (timeout % 100 == 0) EPD_LED_TOGGLE();
        delay(1);
        timeout--;
        if (timeout == 0) {
            NRF_LOG_DEBUG("[EPD]: busy timeout!\n");
            break;
        }
    }
    NRF_LOG_DEBUG("[EPD]: busy release\n");
}

// GPIO
void EPD_GPIO_Init(void)
{
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);
    pinMode(EPD_BUSY_PIN, INPUT);

    if (EPD_EN_PIN != 0xFF) {
        pinMode(EPD_EN_PIN, OUTPUT);
        digitalWrite(EPD_EN_PIN, HIGH);
    }

    pinMode(EPD_BS_PIN, OUTPUT);
    digitalWrite(EPD_BS_PIN, LOW);

    EPD_SPI_Init();

    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    digitalWrite(EPD_RST_PIN, HIGH);

    if (EPD_LED_PIN != 0xFF) {
        pinMode(EPD_LED_PIN, OUTPUT);
        EPD_LED_ON();
    }
}

void EPD_GPIO_Uninit(void)
{
    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    digitalWrite(EPD_RST_PIN, LOW);

    EPD_SPI_Uninit();

    EPD_LED_OFF();
}

// lED
void EPD_LED_ON(void)
{
    if (EPD_LED_PIN != 0xFF)
        digitalWrite(EPD_LED_PIN, LOW);
}

void EPD_LED_OFF(void)
{
    if (EPD_LED_PIN != 0xFF)
        digitalWrite(EPD_LED_PIN, HIGH);
}

void EPD_LED_TOGGLE(void)
{
    if (EPD_LED_PIN != 0xFF)
        nrf_gpio_pin_toggle(EPD_LED_PIN);
}

// EPD models
extern epd_model_t epd_uc8176_420_bw;
extern epd_model_t epd_uc8176_420_bwr;
extern epd_model_t epd_ssd1619_420_bwr;
extern epd_model_t epd_ssd1619_420_bw;
extern epd_model_t epd_uc8276_420_bwr;

static epd_model_t *epd_models[] = {
    &epd_uc8176_420_bw,
    &epd_uc8176_420_bwr,
    &epd_ssd1619_420_bwr,
    &epd_ssd1619_420_bw,
    &epd_uc8276_420_bwr,
};

static epd_model_t *epd_model_get(epd_model_id_t id)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(epd_models); i++) {
        if (epd_models[i]->id == id) {
            return epd_models[i];
        }
    }
    return epd_models[0];
}

epd_driver_t *epd_model_init(epd_model_id_t id)
{
    epd_model_t *model = epd_model_get(id);
    model->drv->init(model->res, model->bwr);
    return model->drv;
}
