#include "Adafruit_GFX.h"
#include "fonts.h"
#include "EPD_driver.h"
#include "Lunar.h"
#include "Calendar.h"

#define PAGE_HEIGHT 32

static void DrawDateHeader(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm, struct Lunar_Date *Lunar)
{
    GFX_setCursor(gfx, x, y);
    GFX_setFont(gfx, u8g2_font_wqy12b_t_lunar);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_printf(gfx, "%d年%02d月%02d日 星期%s", tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday, Lunar_DayString[tm->tm_wday]);

    LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x + 226, y);
    GFX_printf(gfx, "农历: %s%s%s %s%s[%s]年", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
                     Lunar_DateString[Lunar->Date], Lunar_StemStrig[LUNAR_GetStem(Lunar)],
                     Lunar_BranchStrig[LUNAR_GetBranch(Lunar)], Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);
}

static void DrawWeekHeader(Adafruit_GFX *gfx, int16_t x, int16_t y)
{
    GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
    GFX_fillRect(gfx, x, y, 380, 18, GFX_RED);
    for (int i = 0; i < 7; i++)
    {
        GFX_setCursor(gfx, x + 15 + i * 55, y + 14);
        GFX_printf(gfx, "%s", Lunar_DayString[i]);
    }
}

static void DrawMonthDay(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm, struct Lunar_Date *Lunar, uint8_t day)
{
    if (day == tm->tm_mday)
    {
        GFX_fillCircle(gfx, x + 10, y + 9, 22, GFX_RED);
        GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
    }
    else
    {
        GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    }

    GFX_setFont(gfx, u8g2_font_wqy12b_t_lunar);
    GFX_setCursor(gfx, x + 2, y + 4);
    GFX_printf(gfx, "%d", day);

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x, y + 24);
    uint8_t JQdate;
    if (GetJieQi(tm->tm_year + YEAR0, tm->tm_mon + 1, day, &JQdate) && JQdate == day)
    {
        uint8_t JQ = (tm->tm_mon + 1 - 1) * 2;
        if (day >= 15)
            JQ++;
        if (day != tm->tm_mday) GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
        GFX_printf(gfx, "%s", JieQiStr[JQ]);
    }
    else
    {
        LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, day);
        if (Lunar->Date == 1)
            GFX_printf(gfx, "%s", Lunar_MonthString[Lunar->Month]);
        else
            GFX_printf(gfx, "%s", Lunar_DateString[Lunar->Date]);
    }
}

void DrawCalendar(uint32_t timestamp)
{
    tm_t tm = {0};
    struct Lunar_Date Lunar;
    epd_driver_t *driver = epd_driver_get();

    transformTime(timestamp, &tm);

    uint8_t firstDayWeek = get_first_day_week(tm.tm_year + YEAR0, tm.tm_mon + 1);
    uint8_t monthMaxDays = thisMonthMaxDays(tm.tm_year + YEAR0, tm.tm_mon + 1);

    Adafruit_GFX gfx;

    if (driver->id == EPD_DRIVER_4IN2B_V2)
      GFX_begin_3c(&gfx, driver->width, driver->height, PAGE_HEIGHT);
    else
      GFX_begin(&gfx, driver->width, driver->height, PAGE_HEIGHT);

    GFX_firstPage(&gfx);
    do {
        GFX_fillScreen(&gfx, GFX_WHITE);

        DrawDateHeader(&gfx, 10, 22, &tm, &Lunar);
        DrawWeekHeader(&gfx, 10, 26);

        for (uint8_t i = 0; i < monthMaxDays; i++)
        {
            DrawMonthDay(&gfx, 22 + (firstDayWeek + i) % 7 * 55, 60 + (firstDayWeek + i) / 7 * 50, &tm, &Lunar, i + 1);
        }
    } while(GFX_nextPage(&gfx, driver->write_image));

    GFX_end(&gfx);

    driver->display();
}
