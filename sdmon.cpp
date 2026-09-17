#include "sdmon.h"
#include "pin_config.h"
#include <SD.h>
#include <string.h>

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

static bool readExact(File &f, void *dst, size_t len) {
  return f.read((uint8_t *)dst, len) == (int)len;
}

bool PmdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;
  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SD.open(path, FILE_READ);
  if (!f && shiny) {
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dexNum);
    f = SD.open(path, FILE_READ);
  }
  if (!f) return false;
  uint32_t size = f.size();
  // RP2350 has no PSRAM; leave enough SRAM for the 466x466 framebuffer.
  if (size < 11 || size > 60UL * 1024UL) { f.close(); return false; }
  blob = (uint8_t *)malloc(size);
  if (!blob || f.read(blob, size) != (int)size || memcmp(blob, "TPK2", 4) != 0) {
    f.close(); if (blob) { free(blob); blob = nullptr; } return false;
  }
  f.close();
  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);
  const uint8_t *p = blob + 7 + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t i = 0; i < nActs && p + 4 <= end; ++i) {
    uint8_t id = p[0], w = p[1], h = p[2], nf = p[3]; p += 4;
    if (id >= PMD_NACTS || nf == 0 || nf > 24 || w == 0 || h == 0) { unload(); return false; }
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if ((size_t)(end - p) < bytes) { unload(); return false; }
    PmdAct &a = acts[id]; a.w = w; a.h = h; a.frames = nf;
    for (uint8_t k = 0; k < nf; ++k) { a.ms[k] = p[0] | (p[1] << 8); if (!a.ms[k]) a.ms[k] = 100; p += 2; }
    a.data = p; p += (uint32_t)w * h * nf;
    uint8_t base = 1;
    for (uint8_t fr = 0; fr < nf; ++fr) {
      const uint8_t *frame = a.data + (uint32_t)fr * w * h;
      for (int r = h - 1; r >= 0; --r) {
        bool any = false;
        for (uint8_t col = 0; col < w; ++col) if (frame[r * w + col] != 0xFF) { any = true; break; }
        if (any) { if ((uint8_t)(r + 1) > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  loaded = true;
  return true;
}

void PmdMon::unload() {
  if (blob) free(blob);
  blob = nullptr; loaded = false; palCount = 0;
  for (uint8_t i = 0; i < PMD_NACTS; ++i) acts[i] = PmdAct{};
}

bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;
  char path[24];
  snprintf(path, sizeof(path), "/mons/%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SD.open(path, FILE_READ);
  if (!f && shiny) { snprintf(path, sizeof(path), "/mons/%03u.bin", dexNum); f = SD.open(path, FILE_READ); }
  if (!f) return false;
  char magic[4]; uint16_t header[4] = {};
  if (!readExact(f, magic, 4) || memcmp(magic, "TPK1", 4) != 0 || !readExact(f, header, 8)) { f.close(); return false; }
  w = header[0]; h = header[1]; frames = header[2]; frameMs = header[3];
  if (!readExact(f, &palCount, 2) || palCount > 256 || w == 0 || w > 128 || h == 0 || h > 128 || frames == 0 || frames > 32) { f.close(); return false; }
  if (!readExact(f, pal, palCount * 2)) { f.close(); return false; }
  uint32_t size = (uint32_t)w * h * frames;
  if (size > 60UL * 1024UL) { f.close(); return false; }
  data = (uint8_t *)malloc(size);
  if (!data || f.read(data, size) != (int)size) { f.close(); unload(); return false; }
  f.close();
  scale = (uint8_t)(200 / h); if (scale < 1) scale = 1; if (scale > 5) scale = 5;
  loaded = true;
  return true;
}
void SdMon::unload() { if (data) free(data); data = nullptr; loaded = false; }

void SdThumbs::unload() { if (data) free(data); data = nullptr; loaded = false; count = 0; size = 0; }
bool SdThumbs::load() {
  unload(); if (!sdReady) return false;
  File f = SD.open("/mons/thumbs.bin", FILE_READ); if (!f) return false;
  uint32_t sz = f.size(); if (sz < 10 || sz > 64UL * 1024UL) { f.close(); return false; }
  data = (uint8_t *)malloc(sz); if (!data || f.read(data, sz) != (int)sz || memcmp(data, "TPTH", 4) != 0) { f.close(); unload(); return false; }
  f.close(); memcpy(&count, data + 4, 2); if ((uint32_t)6 + 4UL * count > sz) { unload(); return false; }
  size = sz; loaded = true; return true;
}
const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  uint32_t off; memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  if (off > size - 3) return nullptr;
  uint32_t need = 3 + (uint32_t)data[off + 2] * 2 + (uint32_t)data[off] * data[off + 1];
  if (off > size - need) return nullptr;
  return data + off;
}

bool sdBegin() {
  // Arduino-Pico's SD library maps this overload to its PIO SDIO driver.
  sdReady = SD.begin(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  if (sdReady) SD.mkdir("/mons");
  return sdReady;
}

bool sdSerialCommand(const String &line) {
  if (!sdReady) { Serial.println("ERR"); return true; }
  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' '); if (sp <= 4) { Serial.println("ERR"); return true; }
    String path = line.substring(4, sp); uint32_t size = line.substring(sp + 1).toInt();
    if (size == 0 || size > 60UL * 1024UL || !path.startsWith("mons/") || path.indexOf("..") >= 0) { Serial.println("ERR"); return true; }
    if (!path.startsWith("/")) path = "/" + path;
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE); if (!f) { Serial.println("ERR"); return true; }
    Serial.println("OK");
    static uint8_t buf[1024]; uint32_t left = size; Serial.setTimeout(5000);
    while (left) {
      size_t want = left > sizeof(buf) ? sizeof(buf) : left; size_t n = Serial.readBytes(buf, want);
      if (!n || f.write(buf, n) != n) { left = 1; break; }
      left -= n; Serial.println("#");
    }
    f.close(); Serial.setTimeout(1000); sdDirty = (left == 0); Serial.println(sdDirty ? "DONE" : "ERR"); return true;
  }
  if (line == "SDINFO") { Serial.printf("sd=1 size=%llu\n", SD.size64()); Serial.println("DONE"); return true; }
  if (line == "LS") {
    File dir = SD.open("/mons");
    if (dir) { File e; while ((e = dir.openNextFile())) { Serial.printf("%s %u\n", e.name(), (uint32_t)e.size()); e.close(); } dir.close(); }
    Serial.println("DONE"); return true;
  }
  return false;
}
