#include "EPD_driver.h"

bool SSD16xx_ReadBusy(epd_model_t* epd) { return EPD_ReadBusy(); }

static void SSD16xx_WaitBusy(uint16_t timeout) { EPD_WaitBusy(true, timeout); }

static void SSD16xx_Update(uint8_t seq) {
    EPD_Write(SSD16xx_DISP_CTRL2, seq);
    EPD_WriteCmd(SSD16xx_MASTER_ACTIVATE);
}

int8_t SSD16xx_ReadTemp(epd_model_t* epd) {
    SSD16xx_Update(0xB1);
    SSD16xx_WaitBusy(500);
    EPD_WriteCmd(SSD16xx_TSENSOR_READ);
    return (int8_t)EPD_ReadByte();
}

static void SSD16xx_SetWindow(epd_model_t* epd, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    EPD_Write(SSD16xx_ENTRY_MODE, 0x03);  // set ram entry mode: x increase, y increase
    switch (epd->ic) {
        case DRV_IC_SSD1677:
            EPD_Write(SSD16xx_RAM_XPOS, x % 256, x / 256, (x + w - 1) % 256, (x + w - 1) / 256);
            EPD_Write(SSD16xx_RAM_XCOUNT, x % 256, x / 256);
            break;
        default:
            EPD_Write(SSD16xx_RAM_XPOS, x / 8, (x + w - 1) / 8);
            EPD_Write(SSD16xx_RAM_YPOS, y % 256, y / 256, (y + h - 1) % 256, (y + h - 1) / 256);
            EPD_Write(SSD16xx_RAM_XCOUNT, x / 8);
            break;
    }
    EPD_Write(SSD16xx_RAM_YPOS, y % 256, y / 256, (y + h - 1) % 256, (y + h - 1) / 256);
    EPD_Write(SSD16xx_RAM_YCOUNT, y % 256, y / 256);
}

void SSD16xx_Init(epd_model_t* epd) {
    EPD_Reset(true, 10);

    EPD_WriteCmd(SSD16xx_SW_RESET);
    SSD16xx_WaitBusy(200);

    EPD_Write(SSD16xx_BORDER_CTRL, 0x01);
    EPD_Write(SSD16xx_TSENSOR_CTRL, 0x80);

    SSD16xx_SetWindow(epd, 0, 0, epd->width, epd->height);
}

static void SSD16xx_Refresh(epd_model_t* epd) {
    EPD_Write(SSD16xx_DISP_CTRL1, epd->color == COLOR_BWR ? 0x80 : 0x40, 0x00);

    EPD_DEBUG("refresh begin");
    EPD_DEBUG("temperature: %d", SSD16xx_ReadTemp(epd));
    SSD16xx_Update(0xF7);
    SSD16xx_WaitBusy(UINT16_MAX);
    EPD_DEBUG("refresh end");
    SSD16xx_SetWindow(epd, 0, 0, epd->width, epd->height);  // DO NOT REMOVE!
}

void SSD16xx_Clear(epd_model_t* epd, bool refresh) {
    uint32_t ram_bytes = ((epd->width + 7) / 8) * epd->height;

    SSD16xx_SetWindow(epd, 0, 0, epd->width, epd->height);

    EPD_FillRAM(SSD16xx_WRITE_RAM1, 0xFF, ram_bytes);
    EPD_FillRAM(SSD16xx_WRITE_RAM2, 0xFF, ram_bytes);

    if (refresh) SSD16xx_Refresh(epd);
}

void SSD16xx_WriteImage(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h) {
    uint16_t wb = (w + 7) / 8;  // width bytes, bitmaps are padded
    x -= x % 8;                 // byte boundary
    w = wb * 8;                 // byte boundary
    if (x + w > epd->width || y + h > epd->height) return;

    SSD16xx_SetWindow(epd, x, y, w, h);
    EPD_WriteCmd(SSD16xx_WRITE_RAM1);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
    }
    EPD_WriteCmd(SSD16xx_WRITE_RAM2);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            if (epd->color == COLOR_BWR)
                EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
            else
                EPD_WriteByte(black[j + i * wb]);
        }
    }
}

void SSD16xx_WriteRam(epd_model_t* epd, bool begin, bool black, uint8_t* data, uint8_t len) {
    if (begin && black) SSD16xx_SetWindow(epd, 0, 0, epd->width, epd->height);
    if (begin) {
        if (epd->color == COLOR_BWR)
            EPD_WriteCmd(black ? SSD16xx_WRITE_RAM1 : SSD16xx_WRITE_RAM2);
        else
            EPD_WriteCmd(SSD16xx_WRITE_RAM1);
    }
    EPD_WriteData(data, len);
}

void SSD16xx_Sleep(epd_model_t* epd) {
    EPD_Write(SSD16xx_SLEEP_MODE, 0x01);
    delay(100);
}

static const epd_driver_t epd_drv_ssd16xx = {
    .init = SSD16xx_Init,
    .clear = SSD16xx_Clear,
    .write_image = SSD16xx_WriteImage,
    .write_ram = SSD16xx_WriteRam,
    .refresh = SSD16xx_Refresh,
    .sleep = SSD16xx_Sleep,
    .read_temp = SSD16xx_ReadTemp,
    .read_busy = SSD16xx_ReadBusy,
};

// SSD1619 400x300 Black/White/Red
const epd_model_t epd_ssd1619_420_bwr = {SSD1619_420_BWR, COLOR_BWR, &epd_drv_ssd16xx, DRV_IC_SSD1619, 400, 300};
// SSD1619 400x300 Black/White
const epd_model_t epd_ssd1619_420_bw = {SSD1619_420_BW, COLOR_BW, &epd_drv_ssd16xx, DRV_IC_SSD1619, 400, 300};
// SSD1677 880x528 Black/White/Red
const epd_model_t epd_ssd1677_750_bwr = {SSD1677_750_HD_BWR, COLOR_BWR, &epd_drv_ssd16xx, DRV_IC_SSD1677, 880, 528};
// SSD1677 880x528 Black/White
const epd_model_t epd_ssd1677_750_bw = {SSD1677_750_HD_BW, COLOR_BW, &epd_drv_ssd16xx, DRV_IC_SSD1677, 880, 528};
