#include "audio.h"
#include "pin_config.h"
#include "sdmon.h"
#include <Arduino.h>
#include <I2S.h>
#include "Preferences.h"
#include <Wire.h>

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static I2S i2s(OUTPUT);
static bool gReady = false;
static bool gRunning = false;
static bool gOn = true;
static bool gSleeping = false;

static bool esW(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(ES8311_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

static uint8_t esR(uint8_t reg) {
  Wire1.beginTransmission(ES8311_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return 0;
  if (Wire1.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1) != 1) return 0;
  return Wire1.read();
}

static bool es8311Init() {
  Wire1.beginTransmission(ES8311_ADDR);
  if (Wire1.endTransmission() != 0) return false;

  esW(0x0D, 0xFA);
  esW(0x44, 0x08);
  esW(0x44, 0x08);
  esW(0x01, 0x30);
  esW(0x02, 0x00);
  esW(0x03, 0x10);
  esW(0x16, 0x24);
  esW(0x04, 0x10);
  esW(0x05, 0x00);
  esW(0x0B, 0x00);
  esW(0x0C, 0x00);
  esW(0x10, 0x1F);
  esW(0x11, 0x7F);
  esW(0x00, 0x80);
  esW(0x00, 0x80);
  esW(0x01, 0xBF);
  { uint8_t r = esR(0x06); r &= (uint8_t)~0x20; esW(0x06, r); }
  esW(0x13, 0x10);
  esW(0x1B, 0x0A);
  esW(0x1C, 0x6A);
  esW(0x44, 0x58);

  esW(0x02, 0x18);
  esW(0x05, 0x00);
  esW(0x03, 0x10);
  esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }
  esW(0x09, 0x0C);
  esW(0x0A, 0x0C);

  esW(0x00, 0x80);
  esW(0x01, 0xBF);
  esW(0x09, 0x0C);
  esW(0x0A, 0x0C);
  esW(0x17, 0xBF);
  esW(0x0E, 0x02);
  esW(0x12, 0x00);
  esW(0x14, 0x1A);
  esW(0x0D, 0x01);
  esW(0x15, 0x40);
  esW(0x37, 0x08);
  esW(0x45, 0x00);

  esW(0x32, 0xBF);
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }
  return true;
}

struct Note { uint16_t f, ms; };
static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

static bool startI2S() {
  if (gRunning) return true;

  if (!i2s.setBCLK(I2S_BCK_IO)) return false; // LRCLK = BCLK + 1 = GPIO5
  if (!i2s.setDOUT(I2S_DO_IO)) return false;
  if (!i2s.setMCLK(I2S_MCK_IO)) return false;
  if (!i2s.setMCLKmult(256)) return false;     // ES8311 system clock = 256*Fs
  if (!i2s.setBitsPerSample(16)) return false;
  if (!i2s.setBuffers(4, 128, 0)) return false;
  if (!i2s.setFrequency(SAMPLE_RATE)) return false;
  if (!i2s.begin()) return false;

  gRunning = true;
  return true;
}

static void stopI2S() {
  if (!gRunning) return;
  i2s.flush();
  i2s.end();
  gRunning = false;
  pinMode(I2S_DO_IO, INPUT);
  pinMode(I2S_BCK_IO, INPUT);
  pinMode(I2S_WS_IO, INPUT);
  pinMode(I2S_MCK_IO, INPUT);
  digitalWrite(PA, LOW);
  sdInvalidateMount();
}

static void playTone(uint16_t f, uint16_t ms) {
  if (!gRunning || !ms) return;
  const int total = (SAMPLE_RATE * (int)ms) / 1000;
  const int half = f ? (SAMPLE_RATE / (2 * (int)f)) : 0;
  const int16_t amp = 5000;
  int phase = 0;
  bool high = true;

  for (int i = 0; i < total; ++i) {
    int16_t s = 0;
    if (f) {
      s = high ? amp : (int16_t)-amp;
      if (i < 64) {
        s = (int16_t)((int32_t)s * i / 64);
      } else if (i > total - 96) {
        int left = total - i;
        if (left < 0) left = 0;
        s = (int16_t)((int32_t)s * left / 96);
      }
      if (++phase >= half) {
        phase = 0;
        high = !high;
      }
    }
    i2s.write16(s, s);
  }
}

void audioBegin() {
  gSleeping = false;
  gReady = false;
  gRunning = false;
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  if (!startI2S()) {
    Serial.println("I2S init fallito: audio disattivato");
    return;
  }
  if (!es8311Init()) {
    Serial.println("ES8311 non risponde: audio disattivato");
    stopI2S();
    return;
  }
  gReady = true;
  sfxPlay(SFX_HATCH);
}

void sfxPlay(uint8_t id) {
  if (!gReady || !gOn || gSleeping || id >= SFX_COUNT) return;
  if (!startI2S()) {
    Serial.println("I2S init fallito");
    return;
  }

  digitalWrite(PA, HIGH);
  delay(8);

  const SfxDef &d = SFX[id];
  for (uint8_t i = 0; i < d.len && gOn && !gSleeping; ++i)
    playTone(d.n[i].f, d.n[i].ms);
  stopI2S();
}

void audioSetEnabled(bool on) {
  gOn = on;
  if (!on) stopI2S();
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}

bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  gSleeping = sleeping;
  if (sleeping) stopI2S();
}
