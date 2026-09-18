#include "sdmon.h"
#include "pin_config.h"
#include <SD.h>
#include <SDFS.h>
#include <string.h>

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

static constexpr uint32_t SPRITE_RAM_BUDGET = 64UL * 1024UL;
static constexpr uint32_t SD_UPLOAD_LIMIT = 4UL * 1024UL * 1024UL;
static uint32_t spriteRamHeld = 0;
static bool sdMounted = false;

static bool readExact(File &f, void *dst, size_t len) {
  return f.read((uint8_t *)dst, len) == (int)len;
}

static bool ensureSdMounted() {
  if (sdReady && sdMounted) return true;
  return sdBegin();
}

bool PmdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!ensureSdMounted()) return false;

  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SD.open(path, FILE_READ);
  if (!f && shiny) {
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dexNum);
    f = SD.open(path, FILE_READ);
  }
  if (!f) return false;

  uint32_t size = f.size();
  if (size < 11 || size > SD_UPLOAD_LIMIT) {
    f.close();
    return false;
  }

  uint8_t magic[4] = {};
  uint8_t nActs = 0;
  uint16_t pc = 0;
  if (!readExact(f, magic, 4) || memcmp(magic, "TPK2", 4) != 0 ||
      !readExact(f, &nActs, 1) || !readExact(f, &pc, 2) ||
      nActs == 0 || nActs > PMD_NACTS || pc > 256) {
    f.close();
    return false;
  }

  palCount = pc;
  if ((uint32_t)7 + (uint32_t)palCount * 2 > size ||
      !readExact(f, pal, (size_t)palCount * 2)) {
    f.close();
    unload();
    return false;
  }

  memset(acts, 0, sizeof(acts));
  uint32_t maxFrameBytes = 0;

  // Prima passata: leggi solo i metadati e salva gli offset dei pixel.
  for (uint8_t i = 0; i < nActs; ++i) {
    uint8_t hdr[4] = {};
    if (!readExact(f, hdr, sizeof(hdr))) {
      f.close();
      unload();
      return false;
    }

    uint8_t id = hdr[0], w = hdr[1], h = hdr[2], nf = hdr[3];
    if (id >= PMD_NACTS || nf == 0 || nf > 24 || w == 0 || h == 0) {
      f.close();
      unload();
      return false;
    }

    PmdAct &a = acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    for (uint8_t k = 0; k < nf; ++k) {
      uint8_t ms[2] = {};
      if (!readExact(f, ms, 2)) {
        f.close();
        unload();
        return false;
      }
      a.ms[k] = (uint16_t)ms[0] | ((uint16_t)ms[1] << 8);
      if (!a.ms[k]) a.ms[k] = 100;
    }

    a.dataOffset = (uint32_t)f.position();
    uint32_t bytes = (uint32_t)w * h * nf;
    uint32_t pos = (uint32_t)f.position();
    if (bytes > size - pos) {
      f.close();
      unload();
      return false;
    }
    if ((uint32_t)w * h > maxFrameBytes) maxFrameBytes = (uint32_t)w * h;

    if (!f.seek(pos + bytes, SeekSet)) {
      f.close();
      unload();
      return false;
    }
  }

  // Un frame alla volta in SRAM: non serve PSRAM e non serve tenere il file
  // PMD intero in memoria. Il budget vale solo per il frame piu grande.
  if (!maxFrameBytes || maxFrameBytes > SPRITE_RAM_BUDGET ||
      spriteRamHeld + maxFrameBytes > SPRITE_RAM_BUDGET) {
    f.close();
    unload();
    return false;
  }

  frameBuf = (uint8_t *)malloc(maxFrameBytes);
  if (!frameBuf) {
    f.close();
    unload();
    return false;
  }
  frameBufSize = maxFrameBytes;
  spriteRamHeld += frameBufSize;
  cachedAct = 0xFF;
  cachedFrame = 0xFF;

  strncpy(filePath, path, sizeof(filePath) - 1);
  filePath[sizeof(filePath) - 1] = 0;

  // Seconda passata: calcola la base piu bassa con contenuto, come nel
  // formato originale, senza conservare i pixel di tutte le animazioni.
  for (uint8_t id = 0; id < PMD_NACTS; ++id) {
    PmdAct &a = acts[id];
    if (!a.frames) continue;
    uint8_t base = 1;
    const uint32_t frameBytes = (uint32_t)a.w * a.h;
    for (uint8_t fr = 0; fr < a.frames; ++fr) {
      uint32_t off = a.dataOffset + (uint32_t)fr * frameBytes;
      if (!f.seek(off, SeekSet) || f.read(frameBuf, frameBytes) != (int)frameBytes) {
        f.close();
        unload();
        return false;
      }
      for (int r = a.h - 1; r >= 0; --r) {
        bool any = false;
        for (uint8_t col = 0; col < a.w; ++col) {
          if (frameBuf[r * a.w + col] != 0xFF) {
            any = true;
            break;
          }
        }
        if (any) {
          if ((uint8_t)(r + 1) > base) base = (uint8_t)(r + 1);
          break;
        }
      }
    }
    a.base = base;
  }

  f.close();
  loaded = true;
  return true;
}

void PmdMon::unload() {
  if (frameBuf) {
    free(frameBuf);
    if (frameBufSize <= spriteRamHeld) spriteRamHeld -= frameBufSize;
  }
  frameBuf = nullptr;
  frameBufSize = 0;
  loaded = false;
  palCount = 0;
  cachedAct = 0xFF;
  cachedFrame = 0xFF;
  filePath[0] = 0;
  for (uint8_t i = 0; i < PMD_NACTS; ++i) acts[i] = PmdAct{};
}

const uint8_t *PmdMon::frameData(uint8_t actId, uint8_t frame) {
  if (!loaded || !frameBuf || actId >= PMD_NACTS || !acts[actId].frames ||
      frame >= acts[actId].frames) return nullptr;

  if (cachedAct == actId && cachedFrame == frame) return frameBuf;

  PmdAct &a = acts[actId];
  const uint32_t frameBytes = (uint32_t)a.w * a.h;
  if (frameBytes > frameBufSize) return nullptr;

  if (!ensureSdMounted()) {
    cachedAct = cachedFrame = 0xFF;
    return nullptr;
  }

  File f = SD.open(filePath, FILE_READ);
  if (!f) {
    cachedAct = cachedFrame = 0xFF;
    return nullptr;
  }

  uint32_t off = a.dataOffset + (uint32_t)frame * frameBytes;
  bool ok = f.seek(off, SeekSet) && f.read(frameBuf, frameBytes) == (int)frameBytes;
  f.close();
  if (!ok) {
    cachedAct = cachedFrame = 0xFF;
    return nullptr;
  }

  cachedAct = actId;
  cachedFrame = frame;
  return frameBuf;
}

bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!ensureSdMounted()) return false;
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
  if (size == 0 || size > SPRITE_RAM_BUDGET || spriteRamHeld + size > SPRITE_RAM_BUDGET) { f.close(); return false; }
  data = (uint8_t *)malloc(size);
  if (!data) { f.close(); return false; }
  dataSize = size; spriteRamHeld += size;
  if (f.read(data, size) != (int)size) { f.close(); unload(); return false; }
  f.close();
  scale = (uint8_t)(200 / h); if (scale < 1) scale = 1; if (scale > 5) scale = 5;
  loaded = true; return true;
}

void SdMon::unload() {
  if (data) { free(data); if (dataSize <= spriteRamHeld) spriteRamHeld -= dataSize; }
  data = nullptr; dataSize = 0; loaded = false;
}

void SdThumbs::unload() {
  loaded = false; count = 0; size = 0; memset(offsets, 0, sizeof(offsets)); memset(scratch, 0, sizeof(scratch)); cachedDex = -1;
}

bool SdThumbs::load() {
  unload(); if (!ensureSdMounted()) return false;
  File f = SD.open("/mons/thumbs.bin", FILE_READ); if (!f) return false;
  uint32_t sz = f.size(); if (sz < 10 || sz > 256UL * 1024UL) { f.close(); return false; }
  uint8_t hdr[6] = {};
  if (!readExact(f, hdr, sizeof(hdr)) || memcmp(hdr, "TPTH", 4) != 0) { f.close(); return false; }
  count = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
  if (count == 0 || count > 151 || (uint32_t)6 + 4UL * count > sz) { f.close(); unload(); return false; }
  if (!readExact(f, offsets, 4UL * count)) { f.close(); unload(); return false; }
  f.close(); size = sz; loaded = true; cachedDex = -1; return true;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > (int16_t)count) return nullptr;
  if (dex == cachedDex) return scratch;
  uint32_t off = offsets[dex - 1];
  if (off >= size || size - off < 3) return nullptr;
  if (!ensureSdMounted()) return nullptr;
  File f = SD.open("/mons/thumbs.bin", FILE_READ); if (!f) return nullptr;
  if (!f.seek(off, SeekSet)) { f.close(); return nullptr; }
  uint8_t head[3] = {};
  if (!readExact(f, head, sizeof(head))) { f.close(); return nullptr; }
  uint32_t palBytes = (uint32_t)head[2] * 2;
  uint32_t pixelBytes = (uint32_t)head[0] * head[1];
  uint32_t need = 3 + palBytes + pixelBytes;
  if (head[0] == 0 || head[1] == 0 || head[2] > 255 || need > sizeof(scratch) || need > size - off) { f.close(); return nullptr; }
  memcpy(scratch, head, 3);
  if (f.read(scratch + 3, need - 3) != (int)(need - 3)) { f.close(); return nullptr; }
  f.close(); cachedDex = dex; return scratch;
}

void sdInvalidateMount() { sdMounted = false; sdReady = false; }

bool sdBegin() {
  sdReady = SD.begin(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA); sdMounted = sdReady;
  if (sdReady) SD.mkdir("/mons");
  return sdReady;
}

bool sdSerialCommand(const String &line) {
  if (!ensureSdMounted()) { Serial.println("ERR"); return true; }
  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' ');
    if (sp <= 4) { Serial.println("ERR"); return true; }
    String path = line.substring(4, sp); uint32_t size = line.substring(sp + 1).toInt();
    if (size == 0 || size > SD_UPLOAD_LIMIT || !path.startsWith("mons/") || path.indexOf("..") >= 0) { Serial.println("ERR"); return true; }
    if (!path.startsWith("/")) path = "/" + path;
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE); if (!f) { Serial.println("ERR"); return true; }
    Serial.println("OK");
    static uint8_t buf[2048]; uint32_t left = size; Serial.setTimeout(1000);
    while (left) {
      const size_t want = left > sizeof(buf) ? sizeof(buf) : left;
      size_t got = 0;
      // USB CDC puo spezzare un write in piu letture: l'ACK arriva solo dopo
      // aver raccolto esattamente il blocco richiesto dal protocollo.
      while (got < want) {
        size_t n = Serial.readBytes(buf + got, want - got);
        if (!n) { left = 1; break; }
        got += n;
      }
      if (left != 1 && f.write(buf, want) == want) {
        left -= want;
        Serial.println("#");
      } else {
        left = 1;
      }
    }
    f.close(); Serial.setTimeout(1000); sdDirty = (left == 0); Serial.println(sdDirty ? "DONE" : "ERR"); return true;
  }
  if (line == "SDINFO") {
    FSInfo info{};
    if (SDFS.info(info)) {
      Serial.printf("sd=1 size=%llu used=%llu\n", (unsigned long long)info.totalBytes, (unsigned long long)info.usedBytes);
      Serial.println("DONE");
    } else Serial.println("ERR");
    return true;
  }
  if (line == "LS") {
    File dir = SD.open("/mons");
    if (dir) { File e; while ((e = dir.openNextFile())) { Serial.printf("%s %u\n", e.name(), (uint32_t)e.size()); e.close(); } dir.close(); }
    Serial.println("DONE"); return true;
  }
  return false;
}
