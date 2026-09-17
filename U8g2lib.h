#pragma once
#ifdef ARDUINO_ARCH_RP2040
#include <Adafruit_GFX.h>
// Placeholder compatibilità: il port RP2350 usa la font classica Adafruit-GFX.
// Il simbolo mantiene compilabile il ramo CJK del sorgente originale; per ora
// il rendering CJK cade sulla font classica (fallback ASCII/CP437).
static const GFXfont *const u8g2_font_unifont_t_japanese3 = nullptr;
#else
#include_next <U8g2lib.h>
#endif
