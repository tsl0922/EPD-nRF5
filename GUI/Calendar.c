#include "Adafruit_GFX.h"
#include "fonts.h"
#include "EPD_driver.h"
#include "Lunar.h"
#include "Calendar.h"
#include "nrf_log.h"

#define PAGE_HEIGHT 72

static void DrawDateHeader(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm, struct Lunar_Date *Lunar)
{
    GFX_setCursor(gfx, x, y);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_helvB18_tn);
    GFX_printf(gfx, "%d", tm->tm_year + YEAR0);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_wqy12_t_lunar);
    GFX_printf(gfx, "年");
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_helvB18_tn);
    GFX_printf(gfx, "%02d", tm->tm_mon + 1);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_wqy12_t_lunar);
    GFX_printf(gfx, "月");
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_helvB18_tn);
    GFX_printf(gfx, "%02d", tm->tm_mday);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_wqy12_t_lunar);
    GFX_printf(gfx, "日 ");
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "星期%s", Lunar_DayString[tm->tm_wday]);

    LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x + 270, y);
    GFX_printf(gfx, "%s%s%s %s%s", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
                     Lunar_DateString[Lunar->Date], Lunar_StemStrig[LUNAR_GetStem(Lunar)],
                     Lunar_BranchStrig[LUNAR_GetBranch(Lunar)]);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_printf(gfx, "%s", Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_printf(gfx, "年");
}

static void DrawWeekHeader(Adafruit_GFX *gfx, int16_t x, int16_t y)
{
    GFX_fillRect(gfx, x, y, 380, 24, GFX_RED);
    GFX_fillRect(gfx, x + 50, y, 280, 24, GFX_BLACK);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    for (int i = 0; i < 7; i++)
    {
        if (i > 0 && i < 6)
            GFX_setTextColor(gfx, GFX_WHITE, GFX_BLACK);
        else
            GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
        GFX_setCursor(gfx, x + 15 + i * 55, y + 16);
        GFX_printf(gfx, "%s", Lunar_DayString[i]);
    }
}

static void DrawMonthDays(Adafruit_GFX *gfx, tm_t *tm, struct Lunar_Date *Lunar)
{
    uint8_t firstDayWeek = get_first_day_week(tm->tm_year + YEAR0, tm->tm_mon + 1);
    uint8_t monthMaxDays = thisMonthMaxDays(tm->tm_year + YEAR0, tm->tm_mon + 1);
    uint8_t monthDayRows = 1 + (monthMaxDays - (7 - firstDayWeek) + 6) / 7;

    for (uint8_t i = 0; i < monthMaxDays; i++) {
        uint8_t day = i + 1;

        int16_t w = (firstDayWeek + i) % 7;
        bool weekend = (w  == 0) || (w == 6);

        int16_t x = 22 + w * 55;
        int16_t y = (monthDayRows > 5 ? 65 : 68) + (firstDayWeek + i) / 7 * (monthDayRows > 5 ? 39 : 48);

        if (day == tm->tm_mday) {
            GFX_fillCircle(gfx, x + 10, y + (monthDayRows > 5 ? 10 : 12), 20, GFX_RED);
            GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
        } else {
            GFX_setTextColor(gfx, weekend ? GFX_RED : GFX_BLACK, GFX_WHITE);
        }

        GFX_setFont(gfx, u8g2_font_helvB14_tn);
        if (day < 10)
            GFX_setCursor(gfx, x + 6, y + 10);
        else
            GFX_setCursor(gfx, x + 2, y + 10);
        GFX_printf(gfx, "%d", day);

        GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
        GFX_setCursor(gfx, x, y + 24);
        uint8_t JQdate;
        if (GetJieQi(tm->tm_year + YEAR0, tm->tm_mon + 1, day, &JQdate) && JQdate == day) {
            uint8_t JQ = (tm->tm_mon + 1 - 1) * 2;
            if (day >= 15) JQ++;
            if (day != tm->tm_mday) GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
            GFX_printf(gfx, "%s", JieQiStr[JQ]);
        } else {
            LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, day);
            if (Lunar->Date == 1)
                GFX_printf(gfx, "%s", Lunar_MonthString[Lunar->Month]);
            else
                GFX_printf(gfx, "%s", Lunar_DateString[Lunar->Date]);
        }
    }
}

void DrawCalendar(uint32_t timestamp)
{
    tm_t tm = {0};
    struct Lunar_Date Lunar;
    epd_driver_t *driver = epd_driver_get();

    transformTime(timestamp, &tm);

    Adafruit_GFX gfx;

    if (driver->id == EPD_DRIVER_4IN2B_V2)
      GFX_begin_3c(&gfx, driver->width, driver->height, PAGE_HEIGHT);
    else
      GFX_begin(&gfx, driver->width, driver->height, PAGE_HEIGHT);

    GFX_firstPage(&gfx);
    do {
        NRF_LOG_PRINTF("page %d\n", gfx.current_page);
        GFX_fillScreen(&gfx, GFX_WHITE);

        DrawDateHeader(&gfx, 10, 26, &tm, &Lunar);
        DrawWeekHeader(&gfx, 10, 30);
        DrawMonthDays(&gfx, &tm, &Lunar);
    } while(GFX_nextPage(&gfx, driver->write_image));

    GFX_end(&gfx);

    NRF_LOG_PRINTF("display start\n");
    driver->refresh();
    NRF_LOG_PRINTF("display end\n");
}
