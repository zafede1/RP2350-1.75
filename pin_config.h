#pragma once
#include <Wire.h>

// Waveshare RP2350 Touch AMOLED 1.75" — pinout ufficiale.
#define XPOWERS_CHIP_AXP2101
#define LCD_SDIO0 12
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_SCLK 11
#define LCD_CS 10
#define LCD_RESET 16
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define IIC_SDA 6
#define IIC_SCL 7
#define TP_INT 22
#define TP_RESET 23
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41
#ifndef RGB565_BLACK
#define RGB565_BLACK 0x0000
#endif

#ifdef ARDUINO_ARCH_RP2040
// Il core Arduino-Pico espone Wire.begin() + setSDA/setSCL, mentre TamaPoke
// arriva dall'API ESP32 Wire.begin(sda,scl) / setTimeOut().
class RP2350WireCompat {
public:
  explicit RP2350WireCompat(TwoWire &impl) : impl_(impl) {}
  void begin(int sda, int scl) { impl_.setSDA(sda); impl_.setSCL(scl); impl_.begin(); }
  void setTimeOut(uint32_t) {}
  operator TwoWire &() { return impl_; }
private:
  TwoWire &impl_;
};
static RP2350WireCompat rp2350Wire(::Wire);
#define Wire rp2350Wire
#endif
