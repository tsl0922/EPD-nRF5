#include "EPD_driver.h"
#include "nrf_log.h"

bool UC81xx_ReadBusy(epd_model_t* epd) { return EPD_ReadBusy() == false; }

static void UC81xx_WaitBusy(uint16_t timeout) { EPD_WaitBusy(false, timeout); }

static void UC81xx_PowerOn(epd_model_t* epd) {
    EPD_WriteCmd(UC81xx_PON);
    UC81xx_WaitBusy(200);
}

static void UC81xx_PowerOff(epd_model_t* epd) {
    EPD_WriteCmd(UC81xx_POF);
    if (epd->color == COLOR_BWRY) EPD_WriteByte(0x00);
    UC81xx_WaitBusy(200);
}

// Read temperature from driver chip
int8_t UC81xx_Read_Temp(epd_model_t* epd) {
    EPD_WriteCmd(UC81xx_TSC);
    UC81xx_WaitBusy(100);
    return (int8_t)EPD_ReadByte();
}

static void _setPartialRamArea(epd_model_t* epd, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    switch (epd->ic) {
        case DRV_IC_JD79668:
        case DRV_IC_JD79665:
            EPD_Write(0x83,  // partial window
                      x / 256, x % 256, (x + w - 1) / 256, (x + w - 1) % 256, y / 256, y % 256, (y + h - 1) / 256,
                      (y + h - 1) % 256, 0x01);
            break;
        default: {
            uint16_t xe = (x + w - 1) | 0x0007;  // byte boundary inclusive (last byte)
            uint16_t ye = y + h - 1;
            x &= 0xFFF8;           // byte boundary
            EPD_Write(UC81xx_PTL,  // partial window
                      x / 256, x % 256, xe / 256, xe % 256, y / 256, y % 256, ye / 256, ye % 256, 0x00);
        } break;
    }
}

void UC81xx_Refresh(epd_model_t* epd) {
    NRF_LOG_DEBUG("[EPD]: refresh begin\n");

    _setPartialRamArea(epd, 0, 0, epd->width, epd->height);

    EPD_WriteCmd(UC81xx_DRF);
    if (epd->color == COLOR_BWRY) EPD_WriteByte(0x00);
    delay(100);
    UC81xx_WaitBusy(30000);

    NRF_LOG_DEBUG("[EPD]: refresh end\n");
}

void UC81xx_Dump_OTP(epd_model_t* epd) {
    uint8_t data[128];

    EPD_Write(UC81xx_ROTP, 0x00);

    NRF_LOG_DEBUG("=== OTP BEGIN ===\n");
    for (int i = 0; i < 0xFFF; i += sizeof(data)) {
        EPD_ReadData(data, sizeof(data));
        NRF_LOG_HEXDUMP_DEBUG(data, sizeof(data));
    }
    NRF_LOG_DEBUG("=== OTP END ===\n");
}

void UC81xx_Init(epd_model_t* epd) {
    EPD_Reset(true, 50);
    switch (epd->ic) {
        case DRV_IC_UC8159:
            EPD_Write(UC81xx_PWR, 0x37, 0x00);
            EPD_Write(UC81xx_PSR, 0xCF, 0x08);
            EPD_Write(UC81xx_PLL, 0x3A);
            EPD_Write(UC81xx_VDCS, 0x28);
            EPD_Write(UC81xx_BTST, 0xc7, 0xcc, 0x15);
            EPD_Write(UC81xx_CDI, 0x77);
            EPD_Write(UC81xx_TCON, 0x22);
            EPD_Write(0x65, 0x00);  // FLASH CONTROL
            EPD_Write(0xe5, 0x03);  // FLASH MODE
            EPD_Write(UC81xx_TRES, epd->width >> 8, epd->width & 0xff, epd->height >> 8, epd->height & 0xff);
            break;
        case DRV_IC_JD79668:
            EPD_Write(0x4D, 0x78);
            EPD_Write(UC81xx_PSR, 0x0F, 0x29);
            EPD_Write(UC81xx_BTST, 0x0D, 0x12, 0x24, 0x25, 0x12, 0x29, 0x10);
            EPD_Write(UC81xx_PLL, 0x08);
            EPD_Write(UC81xx_CDI, 0x37);
            EPD_Write(UC81xx_TRES, epd->width / 256, epd->width % 256, epd->height / 256, epd->height % 256);
            EPD_Write(0xAE, 0xCF);
            EPD_Write(0xB0, 0x13);
            EPD_Write(0xBD, 0x07);
            EPD_Write(0xBE, 0xFE);
            EPD_Write(0xE9, 0x01);
            break;
        case DRV_IC_JD79665:
            EPD_Write(0x4D, 0x78);
            EPD_Write(UC81xx_PSR, 0x2F, 0x29);
            EPD_Write(UC81xx_BTST, 0x0F, 0x8B, 0x93, 0xA1);
            EPD_Write(UC81xx_TSE, 0x00);
            EPD_Write(UC81xx_CDI, 0x37);
            EPD_Write(UC81xx_TCON, 0x02, 0x02);
            EPD_Write(UC81xx_TRES, epd->width / 256, epd->width % 256, epd->height / 256, epd->height % 256);
            EPD_Write(0x62, 0x98, 0x98, 0x98, 0x75, 0xCA, 0xB2, 0x98, 0x7E);
            if (epd->id == JD79665_750_BWRY)
                EPD_Write(UC81xx_GSST, 0x00, 0x00, 0x00, 0x00);
            else
                EPD_Write(UC81xx_GSST, 0x00, 0x10, 0x00, 0x00);
            EPD_Write(0xE7, 0x1C);
            EPD_Write(UC81xx_PWS, 0x00);
            EPD_Write(0xE9, 0x01);
            EPD_Write(UC81xx_PLL, 0x08);
            break;
        default:
            EPD_Write(UC81xx_PSR, epd->color == COLOR_BWR ? 0x0F : 0x1F);
            EPD_Write(UC81xx_CDI, epd->color == COLOR_BWR ? 0x77 : 0x97);
            break;
    }
    UC81xx_PowerOn(epd);
}

void UC81xx_Clear(epd_model_t* epd, bool refresh) {
    uint32_t wb = (epd->width + 7) / 8;
    switch (epd->ic) {
        case DRV_IC_UC8159:
            EPD_WriteCmd(UC81xx_DTM1);
            for (uint32_t j = 0; j < epd->height; j++) {
                for (uint32_t i = 0; i < wb; i++) {
                    for (uint8_t k = 0; k < 4; k++) {
                        EPD_WriteByte(0x33);
                    }
                }
            }
            break;
        case DRV_IC_JD79668:
        case DRV_IC_JD79665:
            wb = (epd->width + 3) / 4;  // 2bpp
            EPD_WriteCmd(UC81xx_DTM1);
            for (uint16_t i = 0; i < epd->height; i++) {
                for (uint16_t j = 0; j < wb; j++) {
                    EPD_WriteByte(0x55);
                }
            }
            break;
        default:
            EPD_FillRAM(UC81xx_DTM1, 0xFF, wb * epd->height);
            EPD_FillRAM(UC81xx_DTM2, 0xFF, wb * epd->height);
            break;
    }
    if (refresh) UC81xx_Refresh(epd);
}

void UC8176_Write_Image(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h) {
    uint16_t wb = (w + 7) / 8;  // width bytes, bitmaps are padded
    x -= x % 8;                 // byte boundary
    w = wb * 8;                 // byte boundary
    if (x + w > epd->width || y + h > epd->height) return;

    EPD_WriteCmd(UC81xx_PTIN);  // partial in
    _setPartialRamArea(epd, x, y, w, h);
    if (epd->color == COLOR_BWR) {
        EPD_WriteCmd(UC81xx_DTM1);
        for (uint16_t i = 0; i < h; i++) {
            for (uint16_t j = 0; j < w / 8; j++) EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCmd(UC81xx_DTM2);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            if (epd->color == COLOR_BWR)
                EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
            else
                EPD_WriteByte(black[j + i * wb]);
        }
    }
    EPD_WriteCmd(UC81xx_PTOUT);  // partial out
}

static void UC8159_Send_Pixel(uint8_t black_data, uint8_t color_data) {
    uint8_t data;
    for (uint8_t j = 0; j < 8; j++) {
        if ((color_data & 0x80) == 0x00)
            data = 0x04;  // red
        else if ((black_data & 0x80) == 0x00)
            data = 0x00;  // black
        else
            data = 0x03;  // white
        data = (data << 4) & 0xFF;
        black_data = (black_data << 1) & 0xFF;
        color_data = (color_data << 1) & 0xFF;
        j++;
        if ((color_data & 0x80) == 0x00)
            data |= 0x04;  // red
        else if ((black_data & 0x80) == 0x00)
            data |= 0x00;  // black
        else
            data |= 0x03;  // white
        black_data = (black_data << 1) & 0xFF;
        color_data = (color_data << 1) & 0xFF;
        EPD_WriteByte(data);
    }
}

void UC8159_Write_Image(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h) {
    uint16_t wb = (w + 7) / 8;  // width bytes, bitmaps are padded
    x -= x % 8;                 // byte boundary
    w = wb * 8;                 // byte boundary
    if (x + w > epd->width || y + h > epd->height) return;

    EPD_WriteCmd(UC81xx_PTIN);  // partial in
    _setPartialRamArea(epd, x, y, w, h);
    EPD_WriteCmd(UC81xx_DTM1);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            uint8_t black_data = 0xFF;
            uint8_t color_data = 0xFF;
            if (black) black_data = black[j + i * wb];
            if (color) color_data = color[j + i * wb];
            UC8159_Send_Pixel(black_data, color_data);
        }
    }
    EPD_WriteCmd(UC81xx_PTOUT);  // partial out
}

void JD79668_Write_Image(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                         uint16_t h) {
    uint16_t wb = (w + 3) / 4;  // width bytes, bitmaps are padded
    x -= x % 4;                 // byte boundary
    w = wb * 4;                 // byte boundary
    if (x + w > epd->width || y + h > epd->height) return;

    _setPartialRamArea(epd, x, y, w, h);
    EPD_WriteCmd(UC81xx_DTM1);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < wb; j++) {
            // black buffer contains the packed 2bpp data
            // If black is NULL, write 0x55 (White: 01 01 01 01)
            EPD_WriteByte(black ? black[j + i * wb] : 0x55);
        }
    }
}

void UC81xx_Write_Image(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h) {
    switch (epd->ic) {
        case DRV_IC_UC8159:
            UC8159_Write_Image(epd, black, color, x, y, w, h);
            break;
        case DRV_IC_JD79668:
        case DRV_IC_JD79665:
            JD79668_Write_Image(epd, black, color, x, y, w, h);
            break;
        default:
            UC8176_Write_Image(epd, black, color, x, y, w, h);
            break;
    }
}

void UC81xx_Write_Ram(epd_model_t* epd, uint8_t cfg, uint8_t* data, uint8_t len) {
    bool begin = (cfg >> 4) == 0x00;
    bool black = (cfg & 0x0F) == 0x0F;
    switch (epd->ic) {
        case DRV_IC_UC8159:
        case DRV_IC_JD79665:
        case DRV_IC_JD79668:
            if (begin) EPD_WriteCmd(UC81xx_DTM1);
            EPD_WriteData(data, len);
            break;
        default:
            if (begin) {
                if (epd->color == COLOR_BWR)
                    EPD_WriteCmd(black ? UC81xx_DTM1 : UC81xx_DTM2);
                else
                    EPD_WriteCmd(UC81xx_DTM2);
            }
            EPD_WriteData(data, len);
            break;
    }
}

void UC81xx_Sleep(epd_model_t* epd) {
    UC81xx_PowerOff(epd);
    delay(100);
    EPD_Write(UC81xx_DSLP, 0xA5);
}

// Declare driver and models
static const epd_driver_t epd_drv_uc81xx = {
    .init = UC81xx_Init,
    .clear = UC81xx_Clear,
    .write_image = UC81xx_Write_Image,
    .write_ram = UC81xx_Write_Ram,
    .refresh = UC81xx_Refresh,
    .sleep = UC81xx_Sleep,
    .read_temp = UC81xx_Read_Temp,
    .read_busy = UC81xx_ReadBusy,
};

// UC8176 400x300 Black/White
const epd_model_t epd_uc8176_420_bw = {UC8176_420_BW, COLOR_BW, &epd_drv_uc81xx, DRV_IC_UC8176, 400, 300};
// UC8176 400x300 Black/White/Red
const epd_model_t epd_uc8176_420_bwr = {UC8176_420_BWR, COLOR_BWR, &epd_drv_uc81xx, DRV_IC_UC8176, 400, 300};
// UC8159 640x384 Black/White
const epd_model_t epd_uc8159_750_bw = {UC8159_750_LOW_BW, COLOR_BW, &epd_drv_uc81xx, DRV_IC_UC8159, 640, 384};
// UC8159 640x384 Black/White/Red
const epd_model_t epd_uc8159_750_bwr = {UC8159_750_LOW_BWR, COLOR_BWR, &epd_drv_uc81xx, DRV_IC_UC8159, 640, 384};
// UC8179 800x480 Black/White/Red
const epd_model_t epd_uc8179_750_bw = {UC8179_750_BW, COLOR_BW, &epd_drv_uc81xx, DRV_IC_UC8179, 800, 480};
// UC8179 800x480 Black/White/Red
const epd_model_t epd_uc8179_750_bwr = {UC8179_750_BWR, COLOR_BWR, &epd_drv_uc81xx, DRV_IC_UC8179, 800, 480};
// JD79668 400x300 Black/White/Red/Yellow
const epd_model_t epd_jd79668_420_bwry = {JD79668_420_BWRY, COLOR_BWRY, &epd_drv_uc81xx, DRV_IC_JD79668, 400, 300};
// JD79665 800x480 Black/White/Red/Yellow
const epd_model_t epd_jd79665_750_bwry = {JD79665_750_BWRY, COLOR_BWRY, &epd_drv_uc81xx, DRV_IC_JD79665, 800, 480};
// JD79665 648x480 Black/White/Red/Yellow
const epd_model_t epd_jd79665_583_bwry = {JD79665_583_BWRY, COLOR_BWRY, &epd_drv_uc81xx, DRV_IC_JD79665, 648, 480};
