#pragma once
#ifdef ARDUINO_ARCH_RP2040
#include "tama_gfx.h"

class Arduino_DataBus {
public:
  virtual ~Arduino_DataBus() = default;
};
class Arduino_ESP32QSPI : public Arduino_DataBus {
public:
  Arduino_ESP32QSPI(int, int, int, int, int, int) {}
};
class Arduino_CO5300 {
public:
  Arduino_CO5300(Arduino_DataBus *, int, int, int, int, int, int, int, int) {}
  void setBrightness(uint8_t value) { tamaGfx.setBrightness(value); }
};
class Arduino_Canvas : public TamaGFX {
public:
  Arduino_Canvas(int16_t w, int16_t h, Arduino_CO5300 *panel) : TamaGFX(w, h, panel) {}
};

struct RP2350ESPCompat {
  uint32_t getFreeHeap() const { return 520 * 1024u; }
  uint32_t getMinFreeHeap() const { return 520 * 1024u; }
  void restart() const { NVIC_SystemReset(); }
};
static RP2350ESPCompat ESP;
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#else
#include_next <Arduino_GFX_Library.h>
#endif
