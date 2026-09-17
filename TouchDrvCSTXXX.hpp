#pragma once
#ifdef ARDUINO_ARCH_RP2040
#include "CST9217.h"

class TouchDrvCST92xx {
public:
  void setPins(int8_t resetPin, int8_t intPin) { reset_ = resetPin; int_ = intPin; }
  bool begin(TwoWire &, uint8_t = CST9217_I2C_ADDR) { CST9217_Init(); return true; }
  void reset() { CST9217_Reset(); }
  void setMaxCoordinates(uint16_t, uint16_t) {}
  void setMirrorXY(bool mx, bool my) { mirrorX_ = mx; mirrorY_ = my; }
  int getPoint(int16_t *x, int16_t *y, uint8_t = 1) {
    if (!CST9217_Read_Data() || !CST9217.points || !CST9217.data[0].valid) return 0;
    int16_t px = (int16_t)CST9217.data[0].x;
    int16_t py = (int16_t)CST9217.data[0].y;
    if (mirrorX_) px = 465 - px;
    if (mirrorY_) py = 465 - py;
    *x = px; *y = py;
    return 1;
  }
private:
  int8_t reset_ = -1, int_ = -1;
  bool mirrorX_ = false, mirrorY_ = false;
};
#else
#include_next <TouchDrvCSTXXX.hpp>
#endif
