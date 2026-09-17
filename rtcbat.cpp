#include "rtcbat.h"
#include "pin_config.h"
#include <Wire.h>
#include <time.h>

// PCF85063A
static constexpr uint8_t RTC_ADDR = 0x51;
static bool rtcOk = false;
static bool pmuOk = false;

static uint8_t bcdToDec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t decToBcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool rtcRead(uint8_t reg, uint8_t *data, size_t n) {
  Wire1.beginTransmission(RTC_ADDR); Wire1.write(reg); if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(RTC_ADDR, (uint8_t)n) != (int)n) return false;
  for (size_t i = 0; i < n; ++i) data[i] = Wire1.read();
  return true;
}
static bool rtcWrite(uint8_t reg, const uint8_t *data, size_t n) {
  Wire1.beginTransmission(RTC_ADDR); Wire1.write(reg);
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
  time_t tt = (time_t)epoch; tm *t = gmtime(&tt); if (!t) return;
  uint8_t d[7] = {
    decToBcd(t->tm_sec), decToBcd(t->tm_min), decToBcd(t->tm_hour),
    decToBcd((t->tm_wday == 0) ? 7 : t->tm_wday), decToBcd(t->tm_mday),
    decToBcd(t->tm_mon + 1), decToBcd((t->tm_year + 1900) - 2000)
  };
  rtcWrite(0x04, d, sizeof(d));
}

static uint8_t axpRead(uint8_t reg) {
  Wire1.beginTransmission(0x34); Wire1.write(reg); if (Wire1.endTransmission(false) != 0) return 0;
  if (Wire1.requestFrom((uint8_t)0x34, (uint8_t)1) != 1) return 0;
  return Wire1.read();
}
static void axpWrite(uint8_t reg, uint8_t v) {
  Wire1.beginTransmission(0x34); Wire1.write(reg); Wire1.write(v); Wire1.endTransmission();
}

bool batBegin() {
  pmuOk = (axpRead(0x03) == 0x4A);
  if (!pmuOk) Serial.println("AXP2101 non rilevato");
  return pmuOk;
}
void pmuEnablePanel() { /* BLDO1 è già uscito dal reset sulla board; non forziamo registri proprietari. */ }

static uint32_t powerCacheT = 0;
static int cachedPct = -1, cachedMv = 0;
static bool cachedCharging = false, cachedUsb = true;
static void refreshPower() {
  uint32_t now = millis(); if (powerCacheT && now - powerCacheT < 2000) return; powerCacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedMv = 0; cachedCharging = false; cachedUsb = true; return; }
  uint8_t s1 = axpRead(0x00), s2 = axpRead(0x01);
  cachedCharging = ((s2 >> 5) & 0x03) == 1;
  cachedUsb = ((s1 >> 5) & 1) != 0;
  bool batt = ((s1 >> 3) & 1) != 0;
  cachedPct = batt ? axpRead(0xA4) : -1;
  uint16_t raw = ((uint16_t)axpRead(0x34) << 4) | (axpRead(0x35) & 0x0F);
  cachedMv = batt ? (int)((raw * 1.7f)) : 0;
}
int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
int batMillivolts() { refreshPower(); return cachedMv; }
bool usbPresent() { refreshPower(); return cachedUsb; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }
