/*****************************************************************************
* | File        :   EPD_4in2.c
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper
* | Info        :
*----------------
* | This version:   V3.0
* | Date        :   2019-06-13
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_driver.h"
#include "nrf_log.h"

// commands used by this driver
#define CMD_PSR     0x00        // Panel Setting
#define CMD_POF     0x02        // Power OFF
#define CMD_PON     0x04        // Power ON
#define CMD_DSLP    0x07        // Deep sleep
#define CMD_DTM1    0x10        // Display Start Transmission 1
#define CMD_DRF     0x12        // Display Refresh
#define CMD_DTM2    0x13        // Display Start transmission 2 
#define CMD_TSC     0x40        // Temperature Sensor Calibration
#define CMD_CDI     0x50        // Vcom and data interval setting 
#define CMD_PTL     0x90        // Partial Window
#define CMD_PTIN    0x91        // Partial In
#define CMD_PTOUT   0x92        // Partial Out
#define CMD_CCSET   0xE0        // Cascade Setting 
#define CMD_TSSET   0xE5        // Force Temperauture

// PSR registers
#define PSR_RES1      BIT(7)
#define PSR_RES0      BIT(6)
#define PSR_REG       BIT(5)
#define PSR_BWR       BIT(4)
#define PSR_UD        BIT(3)
#define PSR_SHL       BIT(2)
#define PSR_SHD       BIT(1)
#define PSR_RST       BIT(0)

static void UC8176_PowerOn(void)
{
    EPD_WriteCommand(CMD_PON);
    EPD_WaitBusy(LOW, 100);
}

static void UC8176_PowerOff(void)
{
    EPD_WriteCommand(CMD_POF);
    EPD_WaitBusy(LOW, 100);
}

// Read temperature from driver chip
int8_t UC8176_Read_Temp(void)
{
    EPD_WriteCommand_SW(CMD_TSC);
    return (int8_t) EPD_ReadByte_SW();
}

// Force temperature (will trigger OTP LUT switch)
void UC8176_Force_Temp(int8_t value)
{
    EPD_WriteCommand_SW(CMD_CCSET);
    EPD_WriteByte_SW(0x02);
    EPD_WriteCommand_SW(CMD_TSSET);
    EPD_WriteByte_SW(value);
}

/******************************************************************************
function :  Turn On Display
parameter:
******************************************************************************/
void UC8176_Refresh(void)
{
    NRF_LOG_DEBUG("[EPD]: refresh begin\n");
    UC8176_PowerOn();
    NRF_LOG_DEBUG("[EPD]: temperature: %d\n", UC8176_Read_Temp());
    EPD_WriteCommand(CMD_DRF);
    delay(100);
    EPD_WaitBusy(LOW, 30000);
    UC8176_PowerOff();
    NRF_LOG_DEBUG("[EPD]: refresh end\n");
}

/******************************************************************************
function :  Initialize the e-Paper register
parameter:
******************************************************************************/
void UC8176_Init(epd_res_t res, bool bwr)
{
    EPD_BWR_MODE = bwr;
    EPD_Reset(HIGH, 10);

    uint8_t psr = PSR_UD | PSR_SHL | PSR_SHD | PSR_RST;
    if (!EPD_BWR_MODE) psr |= PSR_BWR;
    switch (res) {
        case EPD_RES_320x300:
            EPD_WIDTH = 320;
            EPD_HEIGHT = 300;
            psr |= PSR_RES0;
            break;
        case EPD_RES_320x240:
            EPD_WIDTH = 320;
            EPD_HEIGHT = 240;
            psr |= PSR_RES1;
            break;
        case EPD_RES_200x300:
            EPD_WIDTH = 200;
            EPD_HEIGHT = 300;
            psr |= PSR_RES1 | PSR_RES0;
            break;
        case EPD_RES_400x300:
        default:
            EPD_WIDTH = 400;
            EPD_HEIGHT = 300;
            break;
    }
    NRF_LOG_DEBUG("[EPD]: PSR=%02x\n", psr);
    EPD_WriteCommand(CMD_PSR);
    EPD_WriteByte(psr);

    EPD_WriteCommand(CMD_CDI);
    EPD_WriteByte(EPD_BWR_MODE ? 0xF7 : 0xD7);
}

/******************************************************************************
function :  Clear screen
parameter:
******************************************************************************/
void UC8176_Clear(void)
{
    uint16_t Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    uint16_t Height = EPD_HEIGHT;

    EPD_WriteCommand(CMD_DTM1);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_WriteCommand(CMD_DTM2);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    UC8176_Refresh();
}

static void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
    uint16_t ye = y + h - 1;
    x &= 0xFFF8; // byte boundary
    xe |= 0x0007; // byte boundary
    EPD_WriteCommand(CMD_PTL); // partial window
    EPD_WriteByte(x / 256);
    EPD_WriteByte(x % 256);
    EPD_WriteByte(xe / 256);
    EPD_WriteByte(xe % 256);
    EPD_WriteByte(y / 256);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(ye / 256);
    EPD_WriteByte(ye % 256);
    EPD_WriteByte(0x01);
}

void UC8176_Write_Image(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD_WIDTH || y + h > EPD_HEIGHT) return;
    EPD_WriteCommand(CMD_PTIN); // partial in
    _setPartialRamArea(x, y, w, h);
    if (EPD_BWR_MODE) {
        EPD_WriteCommand(CMD_DTM1);
        for (uint16_t i = 0; i < h; i++) {
            for (uint16_t j = 0; j < w / 8; j++) {
                EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
            }
        }
    }
    EPD_WriteCommand(CMD_DTM2);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            if (EPD_BWR_MODE)
                EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
            else
                EPD_WriteByte(black[j + i * wb]);
        }
    }
    EPD_WriteCommand(CMD_PTOUT); // partial out
}

/******************************************************************************
function :  Enter sleep mode
parameter:
******************************************************************************/
void UC8176_Sleep(void)
{
    UC8176_PowerOff();

    EPD_WriteCommand(CMD_DSLP);
    EPD_WriteByte(0xA5);
}

// Declare driver and models
static epd_driver_t epd_drv_uc8176 = {
    .init = UC8176_Init,
    .clear = UC8176_Clear,
    .write_image = UC8176_Write_Image,
    .refresh = UC8176_Refresh,
    .sleep = UC8176_Sleep,
    .read_temp = UC8176_Read_Temp,
    .force_temp = UC8176_Force_Temp,
};

const epd_model_t epd_4in2 = {
    .id = EPD_4IN2,
    .drv = &epd_drv_uc8176,
    .res = EPD_RES_400x300,
    .bwr = false,
};

const epd_model_t epd_4in2bv2 = {
    .id = EPD_4IN2B_V2,
    .drv = &epd_drv_uc8176,
    .res = EPD_RES_400x300,
    .bwr = true,
};
