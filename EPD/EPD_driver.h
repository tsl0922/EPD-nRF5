#ifndef __EPD_DRIVER_H
#define __EPD_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "EPD_config.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

// EPD driver IC types
typedef enum {
    EPD_DRIVER_IC_UC8159 = 0x10,
    EPD_DRIVER_IC_UC8176 = 0x11,
    EPD_DRIVER_IC_UC8179 = 0x12,
    EPD_DRIVER_IC_SSD1619 = 0x20,
    EPD_DRIVER_IC_SSD1677 = 0x21,
    EPD_DRIVER_IC_JD79668 = 0x30,
    EPD_DRIVER_IC_JD79665 = 0x31,
} epd_driver_ic_t;

// UC81xx commands
enum {
    UC81xx_PSR = 0x00,    // Panel Setting
    UC81xx_PWR = 0x01,    // Power Setting
    UC81xx_POF = 0x02,    // Power OFF
    UC81xx_PFS = 0x03,    // Power OFF Sequence Setting
    UC81xx_PON = 0x04,    // Power ON
    UC81xx_PMES = 0x05,   // Power ON Measure
    UC81xx_BTST = 0x06,   // Booster Soft Start
    UC81xx_DSLP = 0x07,   // Deep sleep
    UC81xx_DTM1 = 0x10,   // Display Start Transmission 1
    UC81xx_DSP = 0x11,    // Data Stop
    UC81xx_DRF = 0x12,    // Display Refresh
    UC81xx_DTM2 = 0x13,   // Display Start transmission 2
    UC81xx_LUTC = 0x20,   // VCOM LUT (LUTC)
    UC81xx_LUTWW = 0x21,  // W2W LUT (LUTWW)
    UC81xx_LUTBW = 0x22,  // B2W LUT (LUTBW / LUTR)
    UC81xx_LUTWB = 0x23,  // W2B LUT (LUTWB / LUTW)
    UC81xx_LUTBB = 0x24,  // B2B LUT (LUTBB / LUTB)
    UC81xx_PLL = 0x30,    // PLL control
    UC81xx_TSC = 0x40,    // Temperature Sensor Calibration
    UC81xx_TSE = 0x41,    // Temperature Sensor Selection
    UC81xx_TSW = 0x42,    // Temperature Sensor Write
    UC81xx_TSR = 0x43,    // Temperature Sensor Read
    UC81xx_CDI = 0x50,    // Vcom and data interval setting
    UC81xx_LPD = 0x51,    // Lower Power Detection
    UC81xx_TCON = 0x60,   // TCON setting
    UC81xx_TRES = 0x61,   // Resolution setting
    UC81xx_GSST = 0x65,   // GSST Setting
    UC81xx_REV = 0x70,    // Revision
    UC81xx_FLG = 0x71,    // Get Status
    UC81xx_AMV = 0x80,    // Auto Measurement Vcom
    UC81xx_VV = 0x81,     // Read Vcom Value
    UC81xx_VDCS = 0x82,   // VCM_DC Setting
    UC81xx_PTL = 0x90,    // Partial Window
    UC81xx_PTIN = 0x91,   // Partial In
    UC81xx_PTOUT = 0x92,  // Partial Out
    UC81xx_PGM = 0xA0,    // Program Mode
    UC81xx_APG = 0xA1,    // Active Progrmming
    UC81xx_ROTP = 0xA2,   // Read OTP
    UC81xx_CCSET = 0xE0,  // Cascade Setting
    UC81xx_PWS = 0xE3,    // Power Saving
    UC81xx_TSSET = 0xE5,  // Force Temperauture
};

// SSD16xx commands
enum {
    SSD16xx_GDO_CTR = 0x01,             // Driver Output control
    SSD16xx_GDV_CTRL = 0x03,            // Gate Driving voltage Control
    SSD16xx_SDV_CTRL = 0x04,            // Source Driving voltage Control
    SSD16xx_SOFTSTART = 0x0C,           // Booster Soft start Control
    SSD16xx_GSCAN_START = 0x0F,         // Gate scan start position
    SSD16xx_SLEEP_MODE = 0x10,          // Deep Sleep mode
    SSD16xx_ENTRY_MODE = 0x11,          // Data Entry mode setting
    SSD16xx_SW_RESET = 0x12,            // SW RESET
    SSD16xx_HV_RD_DETECT = 0x14,        // HV Ready Detection
    SSD16xx_VCI_DETECT = 0x15,          // VCI Detection
    SSD16xx_TSENSOR_CTRL = 0x18,        // Temperature Sensor Control
    SSD16xx_TSENSOR_WRITE = 0x1A,       // Temperature Sensor Control (Write to temperature register)
    SSD16xx_TSENSOR_READ = 0x1B,        // Temperature Sensor Control (Read from temperature register)
    SSD16xx_TSENSOR_WRITE_EXT = 0x1C,   // Temperature Sensor Control (Write Command to External temperature sensor)
    SSD16xx_MASTER_ACTIVATE = 0x20,     // Master Activation
    SSD16xx_DISP_CTRL1 = 0x21,          // Display Update Control 1
    SSD16xx_DISP_CTRL2 = 0x22,          // Display Update Control 2
    SSD16xx_WRITE_RAM1 = 0x24,          // Write RAM (BW)
    SSD16xx_WRITE_RAM2 = 0x26,          // Write RAM (RED)
    SSD16xx_READ_RAM = 0x27,            // Read RAM
    SSD16xx_VCOM_SENSE = 0x28,          // VCOM Sense
    SSD16xx_VCOM_SENSE_DURATON = 0x29,  // VCOM Sense Duration
    SSD16xx_PRGM_VCOM_OTP = 0x2A,       // Program VCOM OTP
    SSD16xx_VCOM_CTRL = 0x2B,           // Write Register for VCOM Control
    SSD16xx_VCOM_VOLTAGE = 0x2C,        // Write VCOM register
    SSD16xx_READ_OTP_REG = 0x2D,        // OTP Register Read for Display Option
    SSD16xx_READ_USER_ID = 0x2E,        // User ID Read
    SSD16xx_READ_STATUS = 0x2F,         // Status Bit Read
    SSD16xx_PRGM_WS_OTP = 0x30,         // Program WS OTP
    SSD16xx_LOAD_WS_OTP = 0x31,         // Load WS OTP
    SSD16xx_WRITE_LUT = 0x32,           // Write LUT register
    SSD16xx_READ_LUT = 0x33,            // Read LUT
    SSD16xx_CRC_CALC = 0x34,            // CRC calculation
    SSD16xx_CRC_STATUS = 0x35,          // CRC Status Read
    SSD16xx_PRGM_OTP_SELECTION = 0x36,  // Program OTP selection
    SSD16xx_OTP_SELECTION_CTRL = 0x37,  // Write OTP selection
    SSD16xx_USER_ID_CTRL = 0x38,        // Write Register for User ID
    SSD16xx_OTP_PROG_MODE = 0x39,       // OTP program mode
    SSD16xx_DUMMY_LINE = 0x3A,          // Set dummy line period
    SSD16xx_GATE_LINE_WIDTH = 0x3B,     // Set Gate line width
    SSD16xx_BORDER_CTRL = 0x3C,         // Border Waveform Control
    SSD16xx_RAM_READ_CTRL = 0x41,       // Read RAM Option
    SSD16xx_RAM_XPOS = 0x44,            // Set RAM X - address Start / End position
    SSD16xx_RAM_YPOS = 0x45,            // Set Ram Y- address Start / End position
    SSD16xx_AUTO_WRITE_RED_RAM = 0x46,  // Auto Write RED RAM for Regular Pattern
    SSD16xx_AUTO_WRITE_BW_RAM = 0x47,   // Auto Write B/W RAM for Regular Pattern
    SSD16xx_RAM_XCOUNT = 0x4E,          // Set RAM X address counter
    SSD16xx_RAM_YCOUNT = 0x4F,          // Set RAM Y address counter
    SSD16xx_ANALOG_BLOCK_CTRL = 0x74,   // Set Analog Block Control
    SSD16xx_DIGITAL_BLOCK_CTRL = 0x7E,  // Set Digital Block Control
    SSD16xx_NOP = 0x7F,                 // NOP
};

typedef enum {
    BW = 1,
    BWR = 2,
    BWRY = 3,
} epd_color_t;

// Do not change the existing IDs!
typedef enum {
    EPD_UC8176_420_BW = 1,
    EPD_UC8176_420_BWR = 3,
    EPD_SSD1619_420_BWR = 2,
    EPD_SSD1619_420_BW = 4,
    EPD_JD79668_420_BWRY = 5,
    EPD_UC8179_750_BW = 6,
    EPD_UC8179_750_BWR = 7,
    EPD_UC8159_750_LOW_BW = 8,
    EPD_UC8159_750_LOW_BWR = 9,
    EPD_SSD1677_750_HD_BW = 10,
    EPD_SSD1677_750_HD_BWR = 11,
    EPD_JD79665_750_BWRY = 12,
    EPD_JD79665_583_BWRY = 13,
} epd_model_id_t;

struct epd_driver;

typedef struct {
    epd_model_id_t id;
    epd_color_t color;
    struct epd_driver* drv;
    uint16_t width;
    uint16_t height;
} epd_model_t;

/**@brief EPD driver structure.
 *
 * @details This structure contains epd driver functions.
 */
typedef struct epd_driver {
    epd_driver_ic_t ic;                            /**< EPD driver IC type */
    void (*init)(epd_model_t* epd);                /**< Initialize the e-Paper register */
    void (*clear)(epd_model_t* epd, bool refresh); /**< Clear screen */
    void (*write_image)(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h);                                              /**< write image */
    void (*write_ram)(epd_model_t* epd, uint8_t cfg, uint8_t* data, uint8_t len); /* write data to epd ram */
    void (*refresh)(epd_model_t* epd);     /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(epd_model_t* epd);       /**< Enter sleep mode */
    int8_t (*read_temp)(epd_model_t* epd); /**< Read temperature from driver chip */
} epd_driver_t;

#define LOW (0x0)
#define HIGH (0x1)

#define DEFAULT (0xFF)
#define INPUT (0x0)
#define OUTPUT (0x1)
#define INPUT_PULLUP (0x2)
#define INPUT_PULLDOWN (0x3)

// Arduino like function wrappers
void pinMode(uint32_t pin, uint32_t mode);
#define digitalWrite(pin, value) nrf_gpio_pin_write(pin, value)
#define digitalRead(pin) nrf_gpio_pin_read(pin)
#define delay(ms) nrf_delay_ms(ms)

// GPIO
void EPD_GPIO_Load(epd_config_t* cfg);
void EPD_GPIO_Init(void);
void EPD_GPIO_Uninit(void);

// SPI
void EPD_SPI_Write(uint8_t* value, uint8_t len);
void EPD_SPI_Read(uint8_t* value, uint8_t len);

// EPD
void EPD_WriteCmd(uint8_t cmd);
void EPD_WriteData(uint8_t* value, uint8_t len);
void EPD_ReadData(uint8_t* value, uint8_t len);
void EPD_WriteByte(uint8_t value);
uint8_t EPD_ReadByte(void);
#define EPD_Write(cmd, ...)                  \
    do {                                     \
        uint8_t _data[] = {__VA_ARGS__};     \
        EPD_WriteCmd(cmd);                   \
        EPD_WriteData(_data, sizeof(_data)); \
    } while (0)
void EPD_FillRAM(uint8_t cmd, uint8_t value, uint32_t len);
void EPD_Reset(uint32_t value, uint16_t duration);
void EPD_WaitBusy(uint32_t value, uint16_t timeout);

// LED
void EPD_LED_ON(void);
void EPD_LED_OFF(void);
void EPD_LED_Toggle(void);
void EPD_LED_BLINK(void);

// VDD voltage
float EPD_ReadVoltage(void);

epd_model_t* epd_init(epd_model_id_t id);

#endif
