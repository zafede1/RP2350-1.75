#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "AMOLED_1in75.h"

class TamaGFX : public Adafruit_GFX {
public:
  TamaGFX();
  TamaGFX(int16_t w, int16_t h, void *unusedPanel);
  bool begin(uint32_t unused = 0);
  void flush();
  void setBrightness(uint8_t brightness255);
  void setUTF8Print(bool enabled) { (void)enabled; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color) override;
};
extern TamaGFX tamaGfx;
