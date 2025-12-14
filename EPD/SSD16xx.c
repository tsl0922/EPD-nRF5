#include "EPD_driver.h"
#include "nrf_log.h"

static void SSD16xx_WaitBusy(uint16_t timeout) { EPD_WaitBusy(HIGH, timeout); }

static void SSD16xx_Update(uint8_t seq) {
    EPD_Write(SSD16xx_DISP_CTRL2, seq);
    EPD_WriteCmd(SSD16xx_MASTER_ACTIVATE);
}

int8_t SSD16xx_Read_Temp(epd_model_t* epd) {
    SSD16xx_Update(0xB1);
    SSD16xx_WaitBusy(500);
    EPD_WriteCmd(SSD16xx_TSENSOR_READ);
    return (int8_t)EPD_ReadByte();
}

static void _setPartialRamArea(epd_model_t* epd, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    EPD_Write(SSD16xx_ENTRY_MODE, 0x03);  // set ram entry mode: x increase, y increase
    if (epd->drv->ic == EPD_DRIVER_IC_SSD1677) {
        EPD_Write(SSD16xx_RAM_XPOS, x % 256, x / 256, (x + w - 1) % 256, (x + w - 1) / 256);
        EPD_Write(SSD16xx_RAM_XCOUNT, x % 256, x / 256);
    } else {
        EPD_Write(SSD16xx_RAM_XPOS, x / 8, (x + w - 1) / 8);
        EPD_Write(SSD16xx_RAM_YPOS, y % 256, y / 256, (y + h - 1) % 256, (y + h - 1) / 256);
        EPD_Write(SSD16xx_RAM_XCOUNT, x / 8);
    }
    EPD_Write(SSD16xx_RAM_YPOS, y % 256, y / 256, (y + h - 1) % 256, (y + h - 1) / 256);
    EPD_Write(SSD16xx_RAM_YCOUNT, y % 256, y / 256);
}

void SSD16xx_Dump_LUT(void) {
    uint8_t lut[128];

    EPD_WriteCmd(SSD16xx_READ_LUT);
    EPD_ReadData(lut, sizeof(lut));

    NRF_LOG_DEBUG("=== LUT BEGIN ===\n");
    NRF_LOG_HEXDUMP_DEBUG(lut, sizeof(lut));
    NRF_LOG_DEBUG("=== LUT END ===\n");
}

void SSD16xx_Init(epd_model_t* epd) {
    EPD_Reset(HIGH, 10);

    EPD_WriteCmd(SSD16xx_SW_RESET);
    SSD16xx_WaitBusy(200);

    EPD_Write(SSD16xx_BORDER_CTRL, 0x01);
    EPD_Write(SSD16xx_TSENSOR_CTRL, 0x80);

    _setPartialRamArea(epd, 0+ALI213_OFFSET_X, 0, epd->width, epd->height);
}

static void SSD16xx_Refresh(epd_model_t* epd) {
    EPD_Write(SSD16xx_DISP_CTRL1, epd->color == BWR ? 0x80 : 0x40, 0x00);

    NRF_LOG_DEBUG("[EPD]: refresh begin\n");
    NRF_LOG_DEBUG("[EPD]: temperature: %d\n", SSD16xx_Read_Temp(epd));
    SSD16xx_Update(0xF7);
    SSD16xx_WaitBusy(30000);
    NRF_LOG_DEBUG("[EPD]: refresh end\n");

    //    SSD16xx_Dump_LUT();

    _setPartialRamArea(epd, 0, 0, epd->width, epd->height);  // DO NOT REMOVE!
    SSD16xx_Update(0x83);                                    // power off
}

void SSD16xx_Clear(epd_model_t* epd, bool refresh) {
    uint32_t ram_bytes = ((epd->width + 7) / 8) * epd->height;

    _setPartialRamArea(epd, 0+ALI213_OFFSET_X, 0, epd->width, epd->height);

    EPD_FillRAM(SSD16xx_WRITE_RAM1, 0xFF, ram_bytes);
    EPD_FillRAM(SSD16xx_WRITE_RAM2, 0xFF, ram_bytes);

    if (refresh) SSD16xx_Refresh(epd);
}

void SSD16xx_Write_Image(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                         uint16_t h) {
    uint16_t wb = (w + 7) / 8;  // width bytes, bitmaps are padded
    x -= x % 8;                 // byte boundary
    w = wb * 8;                 // byte boundary
    if (x + w > epd->width || y + h > epd->height) return;

    _setPartialRamArea(epd, x+ALI213_OFFSET_X, y, w, h);
    EPD_WriteCmd(SSD16xx_WRITE_RAM1);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
    }
    EPD_WriteCmd(SSD16xx_WRITE_RAM2);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            if (epd->color == BWR)
                EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
            else
                EPD_WriteByte(black[j + i * wb]);
        }
    }
}

void SSD16xx_Write_Ram(epd_model_t* epd, uint8_t cfg, uint8_t* data, uint8_t len) {
    bool begin = (cfg >> 4) == 0x00;
    bool black = (cfg & 0x0F) == 0x0F;
    if (begin) {
        if (epd->color == BWR)
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

static epd_driver_t epd_drv_ssd1619 = {
    .ic = EPD_DRIVER_IC_SSD1619,
    .init = SSD16xx_Init,
    .clear = SSD16xx_Clear,
    .write_image = SSD16xx_Write_Image,
    .write_ram = SSD16xx_Write_Ram,
    .refresh = SSD16xx_Refresh,
    .sleep = SSD16xx_Sleep,
    .read_temp = SSD16xx_Read_Temp,
};

static epd_driver_t epd_drv_ssd1677 = {
    .ic = EPD_DRIVER_IC_SSD1677,
    .init = SSD16xx_Init,
    .clear = SSD16xx_Clear,
    .write_image = SSD16xx_Write_Image,
    .write_ram = SSD16xx_Write_Ram,
    .refresh = SSD16xx_Refresh,
    .sleep = SSD16xx_Sleep,
    .read_temp = SSD16xx_Read_Temp,
};

// SSD1619 400x300 Black/White/Red
const epd_model_t epd_ssd1619_420_bwr = {EPD_SSD1619_420_BWR, BWR, &epd_drv_ssd1619, 400, 300};
// SSD1619 400x300 Black/White
const epd_model_t epd_ssd1619_420_bw = {EPD_SSD1619_420_BW, BW, &epd_drv_ssd1619, 400, 300};
// SSD1677 880x528 Black/White/Red
const epd_model_t epd_ssd1677_750_bwr = {EPD_SSD1677_750_HD_BWR, BWR, &epd_drv_ssd1677, 880, 528};
// SSD1677 880x528 Black/White
const epd_model_t epd_ssd1677_750_bw = {EPD_SSD1677_750_HD_BW, BW, &epd_drv_ssd1677, 880, 528};

// SSD1619 250x128 Black/White/Red
const epd_model_t epd_ssd1619_213_bwr = {
    .id = EPD_SSD1619_213_BWR,
    .color = BWR,
    .drv = &epd_drv_ssd1619,
    .width = 128,
    .height = 250,
};

// SSD1619 250x128 Black/White
const epd_model_t epd_ssd1619_213_bw = {
    .id = EPD_SSD1619_213_BW,
    .color = BW,
    .drv = &epd_drv_ssd1619,
    .width = 128,
    .height = 250,
};
