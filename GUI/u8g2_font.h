/*

  U8g2_for_Adafruit_GFX.h
  
  Add unicode support and U8g2 fonts to Adafruit GFX libraries.
  
  U8g2 for Adafruit GFX Lib (https://github.com/olikraus/U8g2_for_Adafruit_GFX)

  Copyright (c) 2018, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list 
    of conditions and the following disclaimer.
    
  * Redistributions in binary form must reproduce the above copyright notice, this 
    list of conditions and the following disclaimer in the documentation and/or other 
    materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  

*/
#ifndef __U8G2_H
#define __U8G2_H

#include <stdint.h>

#ifdef __GNUC__
#  define U8X8_NOINLINE __attribute__((noinline))
#  define U8X8_SECTION(name) __attribute__ ((section (name)))
#  define U8X8_UNUSED __attribute__((unused))
#else
#  define U8X8_SECTION(name)
#  define U8X8_NOINLINE
#  define U8X8_UNUSED
#endif

#if defined(__GNUC__) && defined(__AVR__)
#  define U8X8_FONT_SECTION(name) U8X8_SECTION(".progmem." name)
#  define u8x8_pgm_read(adr) pgm_read_byte_near(adr)
#  define U8X8_PROGMEM PROGMEM
#endif

#ifndef U8X8_FONT_SECTION
#  define U8X8_FONT_SECTION(name) 
#endif

#ifndef u8x8_pgm_read
#  define u8x8_pgm_read(adr) (*(const uint8_t *)(adr)) 
#endif

#ifndef U8X8_PROGMEM
#  define U8X8_PROGMEM
#endif

#define U8G2_FONT_SECTION(name) U8X8_FONT_SECTION(name) 

/* the macro U8G2_USE_LARGE_FONTS enables large fonts (>32K) */
/* it can be enabled for those uC supporting larger arrays */
#if defined(unix) || defined(__arm__) || defined(__arc__) || defined(ESP8266) || defined(ESP_PLATFORM)
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif 
#endif

typedef struct _u8g2_font_info_t
{
    /* offset 0 */
    uint8_t glyph_cnt;
    uint8_t bbx_mode;
    uint8_t bits_per_0;
    uint8_t bits_per_1;

    /* offset 4 */
    uint8_t bits_per_char_width;
    uint8_t bits_per_char_height;    
    uint8_t bits_per_char_x;
    uint8_t bits_per_char_y;
    uint8_t bits_per_delta_x;

    /* offset 9 */
    int8_t max_char_width;
    int8_t max_char_height; /* overall height, NOT ascent. Instead ascent = max_char_height + y_offset */
    int8_t x_offset;
    int8_t y_offset;

    /* offset 13 */
    int8_t  ascent_A;
    int8_t  descent_g;  /* usually a negative value */
    int8_t  ascent_para;
    int8_t  descent_para;

    /* offset 17 */
    uint16_t start_pos_upper_A;
    uint16_t start_pos_lower_a; 

    /* offset 21 */
    uint16_t start_pos_unicode;
} u8g2_font_info_t;

typedef struct _u8g2_font_decode_t
{
    const uint8_t *decode_ptr;      /* pointer to the compressed data */
    
    int16_t target_x;
    int16_t target_y;
    uint16_t fg_color;
    uint16_t bg_color;
    
    int8_t x;           /* local coordinates, (0,0) is upper left */
    int8_t y;
    int8_t glyph_width; 
    int8_t glyph_height;

    uint8_t decode_bit_pos;     /* bitpos inside a byte of the compressed data */
    uint8_t is_transparent;
    uint8_t dir;        /* direction */
} u8g2_font_decode_t;

typedef struct _u8g2_font_t
{
    const uint8_t *font;             /* current font for all text procedures */

    u8g2_font_decode_t font_decode;  /* new font decode structure */
    u8g2_font_info_t font_info;      /* new font info structure */

    int8_t glyph_x_offset;           /* set by u8g2_GetGlyphWidth as a side effect */

    void (*draw_hv_line)(struct _u8g2_font_t *u8g2, int16_t x, int16_t y,
                         int16_t len, uint8_t dir, uint16_t color);
} u8g2_font_t;

uint8_t u8g2_IsGlyph(u8g2_font_t *u8g2, uint16_t requested_encoding);
int8_t u8g2_GetGlyphWidth(u8g2_font_t *u8g2, uint16_t requested_encoding);
void u8g2_SetFontMode(u8g2_font_t *u8g2, uint8_t is_transparent);
void u8g2_SetFontDirection(u8g2_font_t *u8g2, uint8_t dir);
int16_t u8g2_DrawGlyph(u8g2_font_t *u8g2, int16_t x, int16_t y, uint16_t encoding);
int16_t u8g2_DrawStr(u8g2_font_t *u8g2, int16_t x, int16_t y, const char *s);
void u8g2_SetFont(u8g2_font_t *u8g2, const uint8_t  *font);
void u8g2_SetForegroundColor(u8g2_font_t *u8g2, uint16_t fg);
void u8g2_SetBackgroundColor(u8g2_font_t *u8g2, uint16_t bg);

#endif
