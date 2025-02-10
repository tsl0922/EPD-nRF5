/*****************************************************************************
* | File      	:   EPD_4in2b_V2.c
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper b V2
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-11-27
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

// Display resolution
#define EPD_4IN2B_V2_WIDTH       400
#define EPD_4IN2B_V2_HEIGHT      300

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4IN2B_V2_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(200);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(2);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(200);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_4IN2B_V2_ReadBusy(void)
{
    do{
        EPD_WriteCommand(0x71);
		DEV_Delay_ms(50);
    }while(!(DEV_Digital_Read(EPD_BUSY_PIN)));
    DEV_Delay_ms(50);
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void EPD_4IN2B_V2_TurnOnDisplay(void)
{
    EPD_WriteCommand(0x12); // DISPLAY_REFRESH
    DEV_Delay_ms(100);
    EPD_4IN2B_V2_ReadBusy();
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Init(void)
{
    EPD_4IN2B_V2_Reset();

    EPD_WriteCommand(0x00);
    EPD_WriteByte(0x0f);

    EPD_WriteCommand(0x04); 
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Clear(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2B_V2_WIDTH % 8 == 0)? (EPD_4IN2B_V2_WIDTH / 8 ): (EPD_4IN2B_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2B_V2_HEIGHT;

    EPD_WriteCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_WriteCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_4IN2B_V2_TurnOnDisplay();
}

static void _setPartialRamArea(UWORD x, UWORD y, UWORD w, UWORD h)
{
    UWORD xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
    UWORD ye = y + h - 1;
    x &= 0xFFF8; // byte boundary
    xe |= 0x0007; // byte boundary
    EPD_WriteCommand(0x90); // partial window
    EPD_WriteByte(x / 256);
    EPD_WriteByte(x % 256);
    EPD_WriteByte(xe / 256);
    EPD_WriteByte(xe % 256);
    EPD_WriteByte(y / 256);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(ye / 256);
    EPD_WriteByte(ye % 256);
    EPD_WriteByte(0x00); // distortion on right half
}

void EPD_4IN2B_V2_Write_Image(UBYTE *black, UBYTE *color, UWORD x, UWORD y, UWORD w, UWORD h)
{
    UWORD wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD_4IN2B_V2_WIDTH || y + h > EPD_4IN2B_V2_HEIGHT) return;
    EPD_WriteCommand(0x91); // partial in
    _setPartialRamArea(x, y, w, h);
    EPD_WriteCommand(0x10);
    for (UWORD i = 0; i < h; i++) {
        for (UWORD j = 0; j < w / 8; j++) {
            EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCommand(0x13);
    for (UWORD i = 0; i < h; i++) {
        for (UWORD j = 0; j < w / 8; j++) {
            EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCommand(0x92); // partial out
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Sleep(void)
{
    EPD_WriteCommand(0X50);
    EPD_WriteByte(0xf7);		//border floating	

    EPD_WriteCommand(0X02);  	//power off
    EPD_4IN2B_V2_ReadBusy(); //waiting for the electronic paper IC to release the idle signal
    EPD_WriteCommand(0X07);  	//deep sleep
    EPD_WriteByte(0xA5);
}

const epd_driver_t epd_driver_4in2bv2 = {
    .id = EPD_DRIVER_4IN2B_V2,
	.width = EPD_4IN2B_V2_WIDTH,
	.height = EPD_4IN2B_V2_HEIGHT,
    .init = EPD_4IN2B_V2_Init,
    .clear = EPD_4IN2B_V2_Clear,
    .send_command = EPD_WriteCommand,
	.send_byte = EPD_WriteByte,
    .send_data = EPD_WriteData,
    .write_image = EPD_4IN2B_V2_Write_Image,
    .display = EPD_4IN2B_V2_TurnOnDisplay,
    .sleep = EPD_4IN2B_V2_Sleep,
};
