/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Adafruit_GFX.h"

#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef SWAP
#define SWAP(a, b, T) do { T t = a; a = b; b = t; } while (0)
#endif
#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) (type *)((char *)ptr - offsetof(type, member))
#endif

static void GFX_u8g2_draw_hv_line(u8g2_font_t *u8g2, int16_t x, int16_t y,
                                  int16_t len, uint8_t dir, uint16_t color)
{
  Adafruit_GFX *gfx = CONTAINER_OF(u8g2, Adafruit_GFX, u8g2);
  switch(dir) {
    case 0:
      GFX_drawFastHLine(gfx, x, y, len, color);
      break;
    case 1:
      GFX_drawFastVLine(gfx, x, y, len, color);
      break;
    case 2:
      GFX_drawFastHLine(gfx, x - len + 1, y, len, color);
      break;
    case 3:
      GFX_drawFastVLine(gfx, x, y - len + 1, len, color);
      break;
  }
}

/**************************************************************************/
/*!
   @brief    Instatiate a GFX context for graphics
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
   @param    buffer_height Page buffer height
*/
/**************************************************************************/
void GFX_begin(Adafruit_GFX *gfx, int16_t w, int16_t h, int16_t buffer_height) {
  memset(gfx, 0, sizeof(Adafruit_GFX));
  memset(&gfx->u8g2, 0, sizeof(gfx->u8g2));
  gfx->WIDTH = gfx->_width = w;
  gfx->HEIGHT = gfx->_height = h;
  gfx->u8g2.draw_hv_line = GFX_u8g2_draw_hv_line;
  gfx->buffer = malloc(((gfx->WIDTH + 7) / 8) * buffer_height);
  gfx->page_height = buffer_height;
  gfx->total_pages = (gfx->HEIGHT / gfx->page_height) + (gfx->HEIGHT % gfx->page_height > 0);
}

/**************************************************************************/
/*!
   @brief    Instatiate a 3-color GFX context for graphics
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
   @param    buffer_height Page buffer height, should be multiple of 2
*/
/**************************************************************************/
void GFX_begin_3c(Adafruit_GFX *gfx, int16_t w, int16_t h, int16_t buffer_height) {
  GFX_begin(gfx, w, h, buffer_height);
  gfx->page_height = buffer_height / 2;
  gfx->color = gfx->buffer + ((gfx->WIDTH + 7) / 8) * gfx->page_height;
  gfx->total_pages = (gfx->HEIGHT / gfx->page_height) + (gfx->HEIGHT % gfx->page_height > 0);
}

void GFX_end(Adafruit_GFX *gfx) {
  if (gfx->buffer) free(gfx->buffer);
}

void GFX_firstPage(Adafruit_GFX *gfx) {
  GFX_fillScreen(gfx, GFX_WHITE);
  gfx->current_page = 0;
}

bool GFX_nextPage(Adafruit_GFX *gfx, buffer_callback callback) {
  int16_t page_y = gfx->current_page * gfx->page_height;
  int16_t height = MIN(gfx->page_height, gfx->HEIGHT - page_y);
  if (callback)
    callback(gfx->buffer, gfx->color, 0, page_y, gfx->WIDTH, height);

  gfx->current_page++;
  GFX_fillScreen(gfx, GFX_WHITE);

  return gfx->current_page < gfx->total_pages;
}

/**************************************************************************/
/*!
    @brief      Set rotation setting for display
    @param  r   0 thru 3 corresponding to 4 cardinal rotations
*/
/**************************************************************************/
void GFX_setRotation(Adafruit_GFX *gfx, GFX_Rotate r) {
  gfx->rotation = r;
  switch (gfx->rotation) {
  case GFX_ROTATE_0:
  case GFX_ROTATE_180:
    gfx->_width = gfx->WIDTH;
    gfx->_height = gfx->HEIGHT;
    break;
  case GFX_ROTATE_90:
  case GFX_ROTATE_270:
    gfx->_width = gfx->HEIGHT;
    gfx->_height = gfx->WIDTH;
    break;
  }
}


/**************************************************************************/
/*!
   @brief    Draw a pixel
    @param   x   x coordinate
    @param   y   y coordinate
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_drawPixel(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= gfx->_width || y < 0 || y >= gfx->_height) return;
  
  switch (gfx->rotation) {
    case GFX_ROTATE_0:
      break;
    case GFX_ROTATE_90:
      SWAP(x, y, int16_t);
      x = gfx->WIDTH - x - 1;
      break;
    case GFX_ROTATE_180:
      x = gfx->WIDTH - x - 1;
      y = gfx->HEIGHT - y - 1;
      break;
    case GFX_ROTATE_270:
      SWAP(x, y, int16_t);
      y = gfx->HEIGHT - y - 1;
      break;
  }

  y -= gfx->current_page * gfx->page_height;
  if (y < 0 || y >= gfx->page_height) return;

  uint16_t i = x / 8 + y * (gfx->WIDTH / 8);
  if (gfx->color != NULL) {
    gfx->buffer[i] |= 0x80 >> (x & 7); // white
    gfx->color[i] |= 0x80 >> (x & 7);
    if (color == GFX_BLACK)
      gfx->buffer[i] &= ~(0x80 >> (x & 7));
    else if (color == GFX_RED)
      gfx->color[i] &= ~(0x80 >> (x & 7));
  } else {
    if (color == GFX_WHITE)
      gfx->buffer[i] |= 0x80 >> (x & 7);
    else
      gfx->buffer[i] &= ~(0x80 >> (x & 7));
  }
}

/**************************************************************************/
/*!
   @brief    Draw a line.  Bresenham's algorithm - thx wikpedia
    @param    x0  Start point x coordinate
    @param    y0  Start point y coordinate
    @param    x1  End point x coordinate
    @param    y1  End point y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawLine(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color) {
  int16_t steep = ABS(y1 - y0) > ABS(x1 - x0);
  if (steep) {
    SWAP(x0, y0, int16_t);
    SWAP(x1, y1, int16_t);
  }

  if (x0 > x1) {
    SWAP(x0, x1, int16_t);
    SWAP(y0, y1, int16_t);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = ABS(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      GFX_drawPixel(gfx, y0, x0, color);
    } else {
      GFX_drawPixel(gfx, x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}
                                  
/**************************************************************************/
/*!
   @brief    Draw a perfectly vertical line
    @param    x   Top-most x coordinate
    @param    y   Top-most y coordinate
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_drawFastVLine(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t h,
                       uint16_t color) {
  GFX_drawLine(gfx, x, y, x, y + h - 1, color);
}

/**************************************************************************/
/*!
   @brief    Draw a perfectly horizontal line
    @param    x   Left-most x coordinate
    @param    y   Left-most y coordinate
    @param    w   Width in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_drawFastHLine(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w,
                       uint16_t color) {
  GFX_drawLine(gfx, x, y, x + w - 1, y, color);
}

/**************************************************************************/
/*!
   @brief    Fill a rectangle completely with one color.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_fillRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t color) {
  for (int16_t i = x; i < x + w; i++) {
    GFX_drawFastVLine(gfx, i, y, h, color);
  }
}

/**************************************************************************/
/*!
   @brief    Fill the screen completely with one color.
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_fillScreen(Adafruit_GFX *gfx, uint16_t color) {
  uint32_t size = ((gfx->WIDTH + 7) / 8) * gfx->page_height;
  memset(gfx->buffer, color == GFX_WHITE ? 0xFF : 0x00, size);
  if (gfx->color != NULL)
    memset(gfx->color, color == GFX_RED ? 0x00 : 0xFF, size);
}

/**************************************************************************/
/*!
   @brief    Draw a circle outline
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawCircle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r,
                    uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  GFX_drawPixel(gfx, x0, y0 + r, color);
  GFX_drawPixel(gfx, x0, y0 - r, color);
  GFX_drawPixel(gfx, x0 + r, y0, color);
  GFX_drawPixel(gfx, x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    GFX_drawPixel(gfx, x0 + x, y0 + y, color);
    GFX_drawPixel(gfx, x0 - x, y0 + y, color);
    GFX_drawPixel(gfx, x0 + x, y0 - y, color);
    GFX_drawPixel(gfx, x0 - x, y0 - y, color);
    GFX_drawPixel(gfx, x0 + y, y0 + x, color);
    GFX_drawPixel(gfx, x0 - y, y0 + x, color);
    GFX_drawPixel(gfx, x0 + y, y0 - x, color);
    GFX_drawPixel(gfx, x0 - y, y0 - x, color);
  }
}

/**************************************************************************/
/*!
    @brief    Quarter-circle drawer, used to do circles and roundrects
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    cornername  Mask bit #1 or bit #2 to indicate which quarters of
   the circle we're doing
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawCircleHelper(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r,
                          uint8_t cornername, uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (cornername & 0x4) {
      GFX_drawPixel(gfx, x0 + x, y0 + y, color);
      GFX_drawPixel(gfx, x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      GFX_drawPixel(gfx, x0 + x, y0 - y, color);
      GFX_drawPixel(gfx, x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      GFX_drawPixel(gfx, x0 - y, y0 + x, color);
      GFX_drawPixel(gfx, x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      GFX_drawPixel(gfx, x0 - y, y0 - x, color);
      GFX_drawPixel(gfx, x0 - x, y0 - y, color);
    }
  }
}

/**************************************************************************/
/*!
   @brief    Draw a circle with filled color
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_fillCircle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r,
                    uint16_t color) {
  GFX_drawFastVLine(gfx, x0, y0 - r, 2 * r + 1, color);
  GFX_fillCircleHelper(gfx, x0, y0, r, 3, 0, color);
}

/**************************************************************************/
/*!
    @brief  Quarter-circle drawer with fill, used for circles and roundrects
    @param  x0       Center-point x coordinate
    @param  y0       Center-point y coordinate
    @param  r        Radius of circle
    @param  corners  Mask bits indicating which quarters we're doing
    @param  delta    Offset from center-point, used for round-rects
    @param  color    16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX_fillCircleHelper(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t r,
                          uint8_t corners, int16_t delta, uint16_t color) {

  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;
  int16_t px = x;
  int16_t py = y;

  delta++; // Avoid some +1's in the loop

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    // These checks avoid double-drawing certain lines, important
    // for the SSD1306 library which has an INVERT drawing mode.
    if (x < (y + 1)) {
      if (corners & 1)
        GFX_drawFastVLine(gfx, x0 + x, y0 - y, 2 * y + delta, color);
      if (corners & 2)
        GFX_drawFastVLine(gfx, x0 - x, y0 - y, 2 * y + delta, color);
    }
    if (y != py) {
      if (corners & 1)
        GFX_drawFastVLine(gfx, x0 + py, y0 - px, 2 * px + delta, color);
      if (corners & 2)
        GFX_drawFastVLine(gfx, x0 - py, y0 - px, 2 * px + delta, color);
      py = y;
    }
    px = x;
  }
}

/**************************************************************************/
/*!
   @brief   Draw a rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t color) {
  GFX_drawFastHLine(gfx, x, y, w, color);
  GFX_drawFastHLine(gfx, x, y + h - 1, w, color);
  GFX_drawFastVLine(gfx, x, y, h, color);
  GFX_drawFastVLine(gfx, x + w - 1, y, h, color);
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawRoundRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w,
                       int16_t h, int16_t r, uint16_t color) {
  int16_t max_radius = ((w < h) ? w : h) / 2; // 1/2 minor axis
  if (r > max_radius)
    r = max_radius;
  // smarter version
  GFX_drawFastHLine(gfx, x + r, y, w - 2 * r, color);         // Top
  GFX_drawFastHLine(gfx, x + r, y + h - 1, w - 2 * r, color); // Bottom
  GFX_drawFastVLine(gfx, x, y + r, h - 2 * r, color);         // Left
  GFX_drawFastVLine(gfx, x + w - 1, y + r, h - 2 * r, color); // Right
  // draw four corners
  GFX_drawCircleHelper(gfx, x + r, y + r, r, 1, color);
  GFX_drawCircleHelper(gfx, x + w - r - 1, y + r, r, 2, color);
  GFX_drawCircleHelper(gfx, x + w - r - 1, y + h - r - 1, r, 4, color);
  GFX_drawCircleHelper(gfx, x + r, y + h - r - 1, r, 8, color);
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw/fill with
*/
/**************************************************************************/
void GFX_fillRoundRect(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w,
                       int16_t h, int16_t r, uint16_t color) {
  int16_t max_radius = ((w < h) ? w : h) / 2; // 1/2 minor axis
  if (r > max_radius)
    r = max_radius;
  // smarter version
  GFX_fillRect(gfx, x + r, y, w - 2 * r, h, color);
  // draw four corners
  GFX_fillCircleHelper(gfx, x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
  GFX_fillCircleHelper(gfx, x + r, y + r, r, 2, h - 2 * r - 1, color);
}

/**************************************************************************/
/*!
   @brief   Draw a triangle with no fill color
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX_drawTriangle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1,
                      int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  GFX_drawLine(gfx, x0, y0, x1, y1, color);
  GFX_drawLine(gfx, x1, y1, x2, y2, color);
  GFX_drawLine(gfx, x2, y2, x0, y0, color);
}

/**************************************************************************/
/*!
   @brief     Draw a triangle with color-fill
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to fill/draw with
*/
/**************************************************************************/
void GFX_fillTriangle(Adafruit_GFX *gfx, int16_t x0, int16_t y0, int16_t x1,
                      int16_t y1, int16_t x2, int16_t y2, uint16_t color) {

  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    SWAP(y0, y1, int16_t);
    SWAP(x0, x1, int16_t);
  }
  if (y1 > y2) {
    SWAP(y2, y1, int16_t);
    SWAP(x2, x1, int16_t);
  }
  if (y0 > y1) {
    SWAP(y0, y1, int16_t);
    SWAP(x0, x1, int16_t);
  }

  if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
    GFX_drawFastHLine(gfx, a, y0, b - a + 1, color);
    return;
  }

  int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
          dx12 = x2 - x1, dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1; // Include y1 scanline
  else
    last = y1 - 1; // Skip it

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      SWAP(a, b, int16_t);
    GFX_drawFastHLine(gfx, a, y, b - a + 1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      SWAP(a, b, int16_t);
    GFX_drawFastHLine(gfx, a, y, b - a + 1, color);
  }
}

/**************************************************************************/
/*!
   @brief      Draw a RAM-resident 1-bit image at the specified (x,y) position,
   using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
    @param    invert When true, will invert the bitmap
*/
/**************************************************************************/
void GFX_drawBitmap(Adafruit_GFX *gfx, int16_t x, int16_t y, const uint8_t bitmap[],
                    int16_t w, int16_t h, uint16_t color, bool invert) {

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];
      if (((byte & 0x80) == 0x80) ^ invert)
        GFX_drawPixel(gfx, x + i, y + j, color);
    }
  }
}

/*

  U8g2_for_Adafruit_GFX.cpp
  
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

void GFX_setCursor(Adafruit_GFX *gfx, int16_t x, int16_t y) {
  gfx->tx = x;
  gfx->ty = y;
  gfx->utf8_state = 0;
}

void GFX_setFont(Adafruit_GFX *gfx, const uint8_t *font) {
  u8g2_SetFont(&gfx->u8g2, font);
}

void GFX_setFontMode(Adafruit_GFX *gfx, uint8_t is_transparent) {
  u8g2_SetFontMode(&gfx->u8g2, is_transparent);
}

void GFX_setFontDirection(Adafruit_GFX *gfx, GFX_Rotate d) {
  u8g2_SetFontDirection(&gfx->u8g2, (uint8_t)d);
}

void GFX_setTextColor(Adafruit_GFX *gfx, uint16_t fg, uint16_t bg) {
  u8g2_SetForegroundColor(&gfx->u8g2, fg);
  u8g2_SetBackgroundColor(&gfx->u8g2, bg);
}

int8_t GFX_getFontAscent(Adafruit_GFX *gfx) {
  return gfx->u8g2.font_info.ascent_A;
}

int8_t GFX_getFontDescent(Adafruit_GFX *gfx) {
  return gfx->u8g2.font_info.descent_g;
}

int16_t GFX_drawGlyph(Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t e) {
  return u8g2_DrawGlyph(&gfx->u8g2, x, y, e);
}

int16_t GFX_drawStr(Adafruit_GFX *gfx, int16_t x, int16_t y, const char *s) {
  return u8g2_DrawStr(&gfx->u8g2, x, y, s);
}

static uint16_t utf8_next(Adafruit_GFX *gfx, uint8_t b)
{
  if ( b == 0 )  /* '\n' terminates the string to support the string list procedures */
    return 0x0ffff; /* end of string detected, pending UTF8 is discarded */
  if ( gfx->utf8_state == 0 )
  {
    if ( b >= 0xfc )  /* 6 byte sequence */
    {
      gfx->utf8_state = 5;
      b &= 1;
    }
    else if ( b >= 0xf8 )
    {
      gfx->utf8_state = 4;
      b &= 3;
    }
    else if ( b >= 0xf0 )
    {
      gfx->utf8_state = 3;
      b &= 7;      
    }
    else if ( b >= 0xe0 )
    {
      gfx->utf8_state = 2;
      b &= 15;
    }
    else if ( b >= 0xc0 )
    {
      gfx->utf8_state = 1;
      b &= 0x01f;
    }
    else
    {
      /* do nothing, just use the value as encoding */
      return b;
    }
    gfx->encoding = b;
    return 0x0fffe;
  }
  else
  {
    gfx->utf8_state--;
    /* The case b < 0x080 (an illegal UTF8 encoding) is not checked here. */
    gfx->encoding<<=6;
    b &= 0x03f;
    gfx->encoding |= b;
    if ( gfx->utf8_state != 0 )
      return 0x0fffe; /* nothing to do yet */
  }
  return gfx->encoding;
}

int16_t GFX_drawUTF8(Adafruit_GFX *gfx, int16_t x, int16_t y, const char *str)
{
  uint16_t e;
  int16_t delta, sum;
  
  gfx->utf8_state = 0;
  sum = 0;
  for(;;)
  {
    e = utf8_next(gfx, (uint8_t)*str);
    if ( e == 0x0ffff )
      break;
    str++;
    if ( e != 0x0fffe )
    {
      delta = u8g2_DrawGlyph(&gfx->u8g2, x, y, e);
    
      switch(gfx->u8g2.font_decode.dir)
      {
        case 0:
          x += delta;
          break;
        case 1:
          y += delta;
          break;
        case 2:
          x -= delta;
          break;
        case 3:
          y -= delta;
          break;
      }

      sum += delta;    
    }
  }
  return sum;
}

int16_t GFX_getUTF8Width(Adafruit_GFX *gfx, const char *str)
{
  uint16_t e;
  int16_t dx, w;
  
  gfx->u8g2.font_decode.glyph_width = 0;
  gfx->utf8_state = 0;
  w = 0;
  dx = 0;
  for(;;)
  {
    e = utf8_next(gfx, (uint8_t)*str);
    if ( e == 0x0ffff )
      break;
    str++;
    if ( e != 0x0fffe )
    {
      dx = u8g2_GetGlyphWidth(&gfx->u8g2, e);
      w += dx;
    }
  }
  /* adjust the last glyph, check for issue #16: do not adjust if width is 0 */
  if ( gfx->u8g2.font_decode.glyph_width != 0 )
  {
    w -= dx;
    w += gfx->u8g2.font_decode.glyph_width;  /* the real pixel width of the glyph, sideeffect of GetGlyphWidth */
    /* issue #46: we have to add the x offset also */
    w += gfx->u8g2.glyph_x_offset;  /* this value is set as a side effect of u8g2_GetGlyphWidth() */
  }
  
  return w;
}

size_t GFX_print(Adafruit_GFX *gfx, const char c) {
  int16_t delta;
  uint16_t e = utf8_next(gfx, (uint8_t)c);
  if ( e == '\n' )
  {
    gfx->tx = 0;
    gfx->ty += gfx->u8g2.font_info.ascent_para - gfx->u8g2.font_info.descent_para;
  }
  else if ( e == '\r' )
  {
    gfx->tx = 0;
  }
  else if ( e < 0x0fffe )
  {
    delta = u8g2_DrawGlyph(&gfx->u8g2, gfx->tx, gfx->ty, e);
    switch(gfx->u8g2.font_decode.dir)
    {
      case 0:
        gfx->tx += delta;
        break;
      case 1:
        gfx->ty += delta;
        break;
      case 2:
        gfx->tx -= delta;
        break;
      case 3:
        gfx->ty -= delta;
        break;
    }
  }
  return 1;
}

size_t GFX_write(Adafruit_GFX *gfx, const char *buffer, size_t size) {
  size_t cnt = 0;
  while( size > 0 ) {
    cnt += GFX_print(gfx, *buffer++); 
    size--;
  }
  return cnt;
}

size_t GFX_printf(Adafruit_GFX *gfx, const char* format, ...) {
  va_list va;
  char tmp[64] = {0};
  char *buf = tmp;
  size_t len;
 
  va_start(va, format);
  len = vsnprintf(tmp, sizeof(tmp), format, va);
  va_end(va);

  if (len > sizeof(tmp) - 1)
  {
    buf = malloc(len + 1);
    if (buf == NULL)
      return 0;
    va_start(va, format);
    vsnprintf(buf, len + 1, format, va);
    va_end(va);
  }
 
  return GFX_write(gfx, buf, len);
}
