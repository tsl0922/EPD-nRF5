#ifndef _LUNAR_H_
#define _LUNAR_H_
#include <stdint.h>
#include <string.h>

#define YEAR0 (1900)       /* The first year */
#define EPOCH_YR (1970)    /* EPOCH = Jan 1 1970 00:00:00 */
#define SEC_PER_DY (86400) // 一天的秒数
#define SEC_PER_HR (3600)  // 一小时的秒数

typedef struct devtm
{
    uint16_t tm_year;
    uint8_t tm_mon;
    uint8_t tm_mday;
    uint8_t tm_hour;
    uint8_t tm_min;
    uint8_t tm_sec;
    uint8_t tm_wday;
} tm_t;

struct Lunar_Date
{
    uint8_t IsLeap;
    uint8_t Date;
    uint8_t Month;
    uint16_t Year;
};

extern const char Lunar_MonthString[13][7];
extern const char Lunar_MonthLeapString[2][4];
extern const char Lunar_DateString[31][7];
extern const char Lunar_DayString[7][4];
extern const char Lunar_ZodiacString[12][4];
extern const char Lunar_StemStrig[10][4];
extern const char Lunar_BranchStrig[12][4];
extern const char JieQiStr[24][7];

void LUNAR_SolarToLunar(struct Lunar_Date *lunar, uint16_t solar_year, uint8_t solar_month, uint8_t solar_date);
uint8_t LUNAR_GetZodiac(const struct Lunar_Date *lunar);
uint8_t LUNAR_GetStem(const struct Lunar_Date *lunar);
uint8_t LUNAR_GetBranch(const struct Lunar_Date *lunar);
uint8_t GetJieQiStr(uint16_t myear, uint8_t mmonth, uint8_t mday, uint8_t *day);
uint8_t GetJieQi(uint16_t myear, uint8_t mmonth, uint8_t mday, uint8_t *JQdate);

void transformTime(uint32_t unix_time, struct devtm *result);
uint32_t transformTimeStruct(struct devtm *result);
uint8_t get_first_day_week(uint16_t year, uint8_t month);
uint8_t get_last_day(uint16_t year, uint8_t month);
unsigned char day_of_week_get(unsigned char month, unsigned char day, unsigned short year);
uint8_t thisMonthMaxDays(uint8_t year, uint8_t month);

#endif
