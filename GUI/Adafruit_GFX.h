#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "u8g2_font.h"

#define GFX_BLACK     0x0000
#define GFX_WHITE     0xFFFF
#define GFX_RED       0xF800

typedef void (*buffer_callback)(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

typedef enum {
  GFX_ROTATE_0   = 0,
  GFX_ROTATE_90  = 1,
  GFX_ROTATE_180 = 2,
  GFX_ROTATE_270 = 3,
} GFX_Rotate;

// GRAPHICS CONTEXT
typedef struct {
  int16_t WIDTH;        ///< This is the 'raw' display width - never changes
  int16_t HEIGHT;       ///< This is the 'raw' display height - never changes
  int16_t _width;       ///< Display width as modified by current rotation
  int16_t _height;      ///< Display height as modified by current rotation
  GFX_Rotate rotation;  ///< Display rotation (0 thru 3)

  u8g2_font_t u8g2;
  int16_t tx, ty;       // current position for the print command
  uint16_t encoding;    // the unicode, detected by the utf-8 decoder
  uint8_t utf8_state;   // current state of the utf-8 decoder, contains the remaining bytes for a detected unicode glyph 

  uint8_t *buffer;      // black pixel buffer
  uint8_t *color;       // color pixel buffer
  int16_t page_height;
  int16_t current_page;
  int16_t total_pages;
} Adafruit_GFX;

// CONTROL API
void GFX_begin(Adafruit_GFX *gfx, int16_t w, int16_t h, int16_t buffer_height);
void GFX_begin_3c(Adafruit_GFX *gfx, int16_t w, int16_t h, int16_t buffer_height);
void GFX_setRotation(Adafruit_GFX *gfx, GFX_Rotate r);
void GFX_firstPage(Adafruit_GFX *gfx);
bool GFX_nextPage(Adafruit_GFX *gfx, buffer_callback callback);
void GFX_end(Adafruit_GFX *gfx);

// DRAW API
void GFX_drawPixel(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t color);
void GFX_drawLine(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void GFX_drawFastVLine(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t h, uint16_t color);
void GFX_drawFastHLine(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, uint16_t color);
void GFX_fillRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void GFX_fillScreen(Adafruit_GFX *gfx, uint16_t color);
void GFX_drawRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void GFX_drawCircle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r, uint16_t color);
void GFX_drawCircleHelper(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
                          uint16_t color);
void GFX_fillCircle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r, uint16_t color);
void GFX_fillCircleHelper(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
                          int16_t delta, uint16_t color);
void GFX_drawTriangle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                      int16_t y2, uint16_t color);
void GFX_fillTriangle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                      int16_t y2, uint16_t color);
void GFX_drawRoundRect(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t w, int16_t h,
                       int16_t radius, uint16_t color);
void GFX_fillRoundRect(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t w, int16_t h,
                       int16_t radius, uint16_t color);
void GFX_drawBitmap(Adafruit_GFX *gfx, int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
                    int16_t h, uint16_t color, bool invert);

// U8G2 FONT API
void GFX_setCursor(Adafruit_GFX *gfx, int16_t x, int16_t y);
void GFX_setFont(Adafruit_GFX *gfx, const uint8_t *font);
void GFX_setFontMode(Adafruit_GFX *gfx, uint8_t is_transparent);
void GFX_setFontDirection(Adafruit_GFX *gfx, GFX_Rotate d);
void GFX_setTextColor(Adafruit_GFX *gfx, uint16_t fg, uint16_t bg);
int8_t GFX_getFontAscent(Adafruit_GFX *gfx);
int8_t GFX_getFontDescent(Adafruit_GFX *gfx);
int16_t GFX_drawGlyph(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t e);
int16_t GFX_drawStr(Adafruit_GFX *gfx, int16_t x, int16_t y, const char *s);
int16_t GFX_drawUTF8(Adafruit_GFX *gfx, int16_t x, int16_t y, const char *str);
int16_t GFX_getUTF8Width(Adafruit_GFX *gfx, const char *str);
size_t GFX_print(Adafruit_GFX *gfx, const char c);
size_t GFX_write(Adafruit_GFX *gfx, const char *buffer, size_t size);
size_t GFX_printf(Adafruit_GFX *gfx, const char* format, ...);

#endif // _ADAFRUIT_GFX_H
