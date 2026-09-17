#include "tama_gfx.h"
#include "qspi_pio.h"

TamaGFX tamaGfx;

TamaGFX::TamaGFX() : Adafruit_GFX(W, H) {}

bool TamaGFX::begin(uint32_t) {
  QSPI_GPIO_Init(qspi);
  QSPI_PIO_Init(qspi);
  QSPI_4Wrie_Mode(&qspi);
  AMOLED_1IN75_Init();
  fillScreen(0x0000);
  return true;
}

void TamaGFX::flush() { AMOLED_1IN75_Display(framebuffer); }

void TamaGFX::setBrightness(uint8_t brightness255) {
  uint8_t p = (uint16_t)brightness255 * 100 / 255;
  AMOLED_1IN75_SetBrightness(p);
}

void TamaGFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  framebuffer[(size_t)y * W + x] = color;
}

void TamaGFX::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (y < 0 || y >= _height || w <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > _width) w = _width - x;
  if (w <= 0) return;
  uint16_t *p = &framebuffer[(size_t)y * W + x];
  for (int16_t i = 0; i < w; ++i) p[i] = color;
}

void TamaGFX::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (x < 0 || x >= _width || h <= 0) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > _height) h = _height - y;
  if (h <= 0) return;
  for (int16_t i = 0; i < h; ++i) framebuffer[(size_t)(y + i) * W + x] = color;
}

void TamaGFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _width) w = _width - x;
  if (y + h > _height) h = _height - y;
  if (w <= 0 || h <= 0) return;
  for (int16_t yy = 0; yy < h; ++yy) {
    uint16_t *p = &framebuffer[(size_t)(y + yy) * W + x];
    for (int16_t xx = 0; xx < w; ++xx) p[xx] = color;
  }
}

void TamaGFX::fillScreen(uint16_t color) { fillRect(0, 0, W, H, color); }
