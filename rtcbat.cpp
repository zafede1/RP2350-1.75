#include "rtcbat.h"
#include "pin_config.h"
#include <Wire.h>
#include <time.h>

static constexpr uint8_t RTC_ADDR = 0x51;
static constexpr uint8_t PMU_ADDR = 0x34;
static bool rtcOk = false;
static bool pmuOk = false;

static uint8_t bcdToDec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t decToBcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool rtcRead(uint8_t reg, uint8_t *data, size_t n) {
  Wire1.beginTransmission(RTC_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(RTC_ADDR, (uint8_t)n) != (int)n) return false;
  for (size_t i = 0; i < n; ++i) data[i] = Wire1.read();
  return true;
}

static bool rtcWrite(uint8_t reg, const uint8_t *data, size_t n) {
  Wire1.beginTransmission(RTC_ADDR);
  Wire1.write(reg);
  for (size_t i = 0; i < n; ++i) Wire1.write(data[i]);
  return Wire1.endTransmission() == 0;
}

bool rtcBegin() {
  uint8_t v = 0;
  rtcOk = rtcRead(0x02, &v, 1);
  if (!rtcOk) Serial.println("PCF85063 non rilevato");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  uint8_t d[7] = {};
  if (!rtcRead(0x04, d, sizeof(d))) return 0;
  if (d[0] & 0x80) return 0;
  tm t = {};
  t.tm_sec = bcdToDec(d[0] & 0x7F);
  t.tm_min = bcdToDec(d[1] & 0x7F);
  t.tm_hour = bcdToDec(d[2] & 0x3F);
  t.tm_mday = bcdToDec(d[3] & 0x3F);
  t.tm_mon = bcdToDec(d[5] & 0x1F) - 1;
  t.tm_year = 2000 + bcdToDec(d[6]) - 1900;
  time_t e = mktime(&t);
  if (e <= 0 || t.tm_year + 1900 < 2025 || t.tm_year + 1900 > 2120) return 0;
  return (uint32_t)e;
}

void rtcSetEpoch(uint32_t epoch) {
  if (!rtcOk) return;
  time_t tt = (time_t)epoch;
  tm *t = gmtime(&tt);
  if (!t) return;
  uint8_t d[7] = {
    decToBcd(t->tm_sec), decToBcd(t->tm_min), decToBcd(t->tm_hour),
    decToBcd((t->tm_wday == 0) ? 7 : t->tm_wday), decToBcd(t->tm_mday),
    decToBcd(t->tm_mon + 1), decToBcd((t->tm_year + 1900) - 2000)
  };
  rtcWrite(0x04, d, sizeof(d));
}

static uint8_t axpRead(uint8_t reg) {
  Wire1.beginTransmission(PMU_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return 0;
  if (Wire1.requestFrom(PMU_ADDR, (uint8_t)1) != 1) return 0;
  return Wire1.read();
}

static bool axpWrite(uint8_t reg, uint8_t v) {
  Wire1.beginTransmission(PMU_ADDR);
  Wire1.write(reg);
  Wire1.write(v);
  return Wire1.endTransmission() == 0;
}

static bool axpSetBit(uint8_t reg, uint8_t bit) {
  return axpWrite(reg, axpRead(reg) | (uint8_t)(1u << bit));
}

static bool axpClrBit(uint8_t reg, uint8_t bit) {
  return axpWrite(reg, axpRead(reg) & (uint8_t)~(1u << bit));
}

bool batBegin() {
  pmuOk = (axpRead(0x03) == 0x4A);
  if (!pmuOk) {
    Serial.println("AXP2101 non rilevato");
    return false;
  }

  // ADC: batteria, VBUS, VSYS e temperatura, come nell'esempio Waveshare.
  axpClrBit(0x30, 1); // disable TS pin measurement
  axpSetBit(0x30, 4); // temperature
  axpSetBit(0x30, 2); // VBUS
  axpSetBit(0x30, 0); // battery
  axpSetBit(0x30, 3); // system voltage
  axpSetBit(0x68, 0); // battery detection
  // 4.2V target (driver Waveshare usa option 3).
  uint8_t cv = axpRead(0x64);
  axpWrite(0x64, (uint8_t)((cv & 0xF8) | 3));

  // Pulizia degli IRQ pendenti e enable del power-key short press.
  axpWrite(0x48, 0xFF);
  axpWrite(0x49, 0xFF);
  axpWrite(0x4A, 0xFF);
  axpSetBit(0x41, 3);
  return true;
}

void pmuEnablePanel() {
  // BLDO1: 0.5V + 28*100mV = 3.3V; enable = bit4 di LDO_ONOFF_CTRL0.
  uint8_t v = axpRead(0x96);
  axpWrite(0x96, (uint8_t)((v & 0xE0) | 28));
  axpSetBit(0x90, 4);
}

static uint32_t powerCacheT = 0;
static int cachedPct = -1, cachedMv = 0;
static bool cachedCharging = false, cachedUsb = false;

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < 2000) return;
  powerCacheT = now ? now : 1;
  if (!pmuOk) {
    cachedPct = -1;
    cachedMv = 0;
    cachedCharging = false;
    cachedUsb = false;
    return;
  }

  uint8_t s1 = axpRead(0x00), s2 = axpRead(0x01);
  cachedCharging = ((s2 >> 5) & 0x03) == 1;
  bool vbusGood = (s1 & (1u << 5)) != 0;
  bool vbusIn = ((s2 & (1u << 3)) == 0) && vbusGood;
  cachedUsb = vbusIn;

  bool batt = (s1 & (1u << 3)) != 0;
  cachedPct = batt ? axpRead(0xA4) : -1;
  uint16_t raw = ((uint16_t)axpRead(0x34) << 8) | axpRead(0x35);
  raw &= 0x0FFF;
  cachedMv = batt ? (int)(raw * 1.7f) : 0;
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
int batMillivolts() { refreshPower(); return cachedMv; }
bool usbPresent() { refreshPower(); return cachedUsb; }

void pwrSetup() {
  if (!pmuOk) return;
  // Short press IRQ + hardware long-press shutdown after 4s.
  uint8_t r27 = axpRead(0x27);
  r27 &= (uint8_t)~0x0C; // OFFLEVEL=00 -> 4s
  axpWrite(0x27, r27);
  axpClrBit(0x40, 0); // start from a known IRQ mask for gauge channel
  axpSetBit(0x41, 3); // PKEY short press IRQ
  axpSetBit(0x42, 7); // watchdog IRQ harmless; keep default safety path
  axpWrite(0x48, 0xFF);
  axpWrite(0x49, 0xFF);
  axpWrite(0x4A, 0xFF);
}

bool pwrShortPressed() {
  if (!pmuOk) return false;
  uint8_t stat = axpRead(0x49);
  if ((stat & (1u << 3)) == 0) return false;
  // RW1C: write a one to clear the short-press flag.
  axpWrite(0x49, (uint8_t)(1u << 3));
  return true;
}
