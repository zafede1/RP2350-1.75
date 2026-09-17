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

// Audio ES8311 della board Waveshare: BCLK=4, LRCLK=5, DOUT=1.
// Il codec viene configurato come slave con clock derivato dal BCLK, quindi
// non serve generare MCLK dal core Arduino-Pico durante la riproduzione.
#define I2S_BCK_IO 4
#define I2S_WS_IO 5
#define I2S_DO_IO 1
#define I2S_DI_IO 2
#define I2S_MCK_IO 3
#define PA 0

// SDIO 1-bit: il core Arduino-Pico usa questa overload per il controller PIO.
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41

#ifndef RGB565_BLACK
#define RGB565_BLACK 0x0000
#endif

#ifdef ARDUINO_ARCH_RP2040
// Compatibilità API: TamaPoke usa begin(sda,scl), mentre il core Arduino-Pico
// richiede setSDA/setSCL + begin(). La board Waveshare usa I2C1 sui GPIO 6/7.
class RP2350WireCompat {
public:
  explicit RP2350WireCompat(TwoWire &impl) : impl_(impl) {}
  void begin(int sda, int scl) { impl_.setSDA(sda); impl_.setSCL(scl); impl_.begin(); }
  void setTimeOut(uint32_t) {}
  operator TwoWire &() { return impl_; }
private:
  TwoWire &impl_;
};
static RP2350WireCompat rp2350Wire(::Wire1);
#define Wire rp2350Wire

// Compatibilità Serial: i metodi ESP32 setRxBufferSize/setTxTimeoutMs non esistono
// su Arduino-Pico. Li rendiamo no-op mantenendo intatto il codice del gioco.
class RP2350SerialCompat {
public:
  explicit RP2350SerialCompat(decltype(::Serial) &impl) : impl_(impl) {}
  void begin(unsigned long baud) { impl_.begin(baud); }
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
  void setTimeout(unsigned long ms) { impl_.setTimeout(ms); }
  operator bool() const { return (bool)impl_; }
  template <typename T> size_t print(const T &v) { return impl_.print(v); }
  template <typename T> size_t println(const T &v) { return impl_.println(v); }
  size_t println() { return impl_.println(); }
  template <typename... Args> int printf(const char *fmt, Args... args) {
    char buf[384]; int n = snprintf(buf, sizeof(buf), fmt, args...); if (n <= 0) return n; return (int)impl_.print(buf);
  }
  size_t write(uint8_t b) { return impl_.write(b); }
  size_t write(const uint8_t *b, size_t n) { return impl_.write(b, n); }
  int available() { return impl_.available(); }
  int read() { return impl_.read(); }
  size_t readBytes(char *b, size_t n) { return impl_.readBytes(b, n); }
  size_t readBytes(uint8_t *b, size_t n) { return impl_.readBytes(b, n); }
private:
  decltype(::Serial) &impl_;
};
static RP2350SerialCompat rp2350Serial(::Serial);
#define Serial rp2350Serial
#endif
