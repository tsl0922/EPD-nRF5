/*****************************************************************************
* | File      	:   EPD_4in2_V2.h
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper V2
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2023-09-12
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
#define EPD_4IN2_V2_WIDTH       400
#define EPD_4IN2_V2_HEIGHT      300

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4IN2_V2_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(100);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(2);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(100);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_4IN2_V2_ReadBusy(void)
{
    while(DEV_Digital_Read(EPD_BUSY_PIN) == 1) {      //LOW: idle, HIGH: busy
        DEV_Delay_ms(10);
    }
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void EPD_4IN2_V2_TurnOnDisplay(void)
{
    EPD_WriteCommand(0x22);
	EPD_WriteByte(0xF7);
    EPD_WriteCommand(0x20);
    EPD_4IN2_V2_ReadBusy();
}

/******************************************************************************
function :	Setting the display window
parameter:
******************************************************************************/
static void EPD_4IN2_V2_SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    EPD_WriteCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_WriteByte((Xstart>>3) & 0xFF);
    EPD_WriteByte((Xend>>3) & 0xFF);
	
    EPD_WriteCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_WriteByte(Ystart & 0xFF);
    EPD_WriteByte((Ystart >> 8) & 0xFF);
    EPD_WriteByte(Yend & 0xFF);
    EPD_WriteByte((Yend >> 8) & 0xFF);
}

/******************************************************************************
function :	Set Cursor
parameter:
******************************************************************************/
static void EPD_4IN2_V2_SetCursor(UWORD Xstart, UWORD Ystart)
{
    EPD_WriteCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    EPD_WriteByte(Xstart & 0xFF);

    EPD_WriteCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    EPD_WriteByte(Ystart & 0xFF);
    EPD_WriteByte((Ystart >> 8) & 0xFF);
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4IN2_V2_Init(void)
{
    EPD_4IN2_V2_Reset();

    EPD_4IN2_V2_ReadBusy();   
    EPD_WriteCommand(0x12);   // soft  reset
    EPD_4IN2_V2_ReadBusy();
	
    // EPD_WriteCommand(0x01); //Driver output control      
    // EPD_WriteByte((EPD_4IN2_V2_HEIGHT-1)%256);   
    // EPD_WriteByte((EPD_4IN2_V2_HEIGHT-1)/256);
    // EPD_WriteByte(0x00);

    EPD_WriteCommand(0x21); //  Display update control
    EPD_WriteByte(0x40);		
    EPD_WriteByte(0x00);		

    EPD_WriteCommand(0x3C); //BorderWavefrom
    EPD_WriteByte(0x05);
	
    EPD_WriteCommand(0x11);	// data  entry  mode
    EPD_WriteByte(0x03);		// X-mode   
		
	EPD_4IN2_V2_SetWindows(0, 0, EPD_4IN2_V2_WIDTH-1, EPD_4IN2_V2_HEIGHT-1);
	 
	EPD_4IN2_V2_SetCursor(0, 0);
	
    EPD_4IN2_V2_ReadBusy();
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4IN2_V2_Clear(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2_V2_WIDTH % 8 == 0)? (EPD_4IN2_V2_WIDTH / 8 ): (EPD_4IN2_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2_V2_HEIGHT;

    EPD_WriteCommand(0x24);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_WriteCommand(0x26);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }
    EPD_4IN2_V2_TurnOnDisplay();
}

static void _setPartialRamArea(UWORD x, UWORD y, UWORD w, UWORD h)
{
    EPD_WriteCommand(0x11); // set ram entry mode
    EPD_WriteByte(0x03);    // x increase, y increase : normal mode
    EPD_WriteCommand(0x44);
    EPD_WriteByte(x / 8);
    EPD_WriteByte((x + w - 1) / 8);
    EPD_WriteCommand(0x45);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(y / 256);
    EPD_WriteByte((y + h - 1) % 256);
    EPD_WriteByte((y + h - 1) / 256);
    EPD_WriteCommand(0x4e);
    EPD_WriteByte(x / 8);
    EPD_WriteCommand(0x4f);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(y / 256);
}

void EPD_4IN2_V2_Write_Image(UBYTE *black, UBYTE *color, UWORD x, UWORD y, UWORD w, UWORD h)
{
    int32_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD_4IN2_V2_WIDTH || y + h > EPD_4IN2_V2_HEIGHT) return;
    _setPartialRamArea(x, y, w, h);
    EPD_WriteCommand(0x24);
    for (UWORD i = 0; i < h; i++) {
        for (UWORD j = 0; j < w / 8; j++) {
            EPD_WriteByte(black[j + i * wb]);
        }
    }
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4IN2_V2_Sleep(void)
{
    EPD_WriteCommand(0x10); // DEEP_SLEEP
    EPD_WriteByte(0x01);
	DEV_Delay_ms(200);
}

const epd_driver_t epd_driver_4in2v2 = {
    .id = EPD_DRIVER_4IN2_V2,
	.width = EPD_4IN2_V2_WIDTH,
	.height = EPD_4IN2_V2_HEIGHT,
    .init = EPD_4IN2_V2_Init,
    .clear = EPD_4IN2_V2_Clear,
    .send_command = EPD_WriteCommand,
	.send_byte = EPD_WriteByte,
    .send_data = EPD_WriteData,
    .write_image = EPD_4IN2_V2_Write_Image,
    .display = EPD_4IN2_V2_TurnOnDisplay,
    .sleep = EPD_4IN2_V2_Sleep,
};
