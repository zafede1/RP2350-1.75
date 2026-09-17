#include "sdmon.h"
#include "pin_config.h"
#include <SD.h>
#include <string.h>

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

// Il framebuffer AMOLED occupa ~434 KB su 520 KB di SRAM. Tutti gli sprite
// dinamici condividono quindi un budget stretto; il secondo sprite richiesto
// mentre uno e gia residente viene rifiutato e il chiamante usa il fallback.
static constexpr uint32_t SPRITE_RAM_BUDGET = 64UL * 1024UL;
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
  if (size < 11 || size > 60UL * 1024UL || spriteRamHeld + size > SPRITE_RAM_BUDGET) {
    f.close();
    return false;
  }

  blob = (uint8_t *)malloc(size);
  if (!blob) {
    f.close();
    return false;
  }
  blobSize = size;
  spriteRamHeld += size;

  if (f.read(blob, size) != (int)size || memcmp(blob, "TPK2", 4) != 0) {
    f.close();
    unload();
    return false;
  }
  f.close();

  uint8_t nActs = blob[4];
  if (nActs > PMD_NACTS) { unload(); return false; }
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) {
    unload();
    return false;
  }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *p = blob + 7 + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t i = 0; i < nActs && p + 4 <= end; ++i) {
    uint8_t id = p[0], w = p[1], h = p[2], nf = p[3];
    p += 4;
    if (id >= PMD_NACTS || nf == 0 || nf > 24 || w == 0 || h == 0) {
      unload();
      return false;
    }
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if ((size_t)(end - p) < bytes) {
      unload();
      return false;
    }

    PmdAct &a = acts[id];
    a.w = w; a.h = h; a.frames = nf;
    for (uint8_t k = 0; k < nf; ++k) {
      a.ms[k] = p[0] | (p[1] << 8);
      if (!a.ms[k]) a.ms[k] = 100;
      p += 2;
    }
    a.data = p;
    p += (uint32_t)w * h * nf;

    uint8_t base = 1;
    for (uint8_t fr = 0; fr < nf; ++fr) {
      const uint8_t *frame = a.data + (uint32_t)fr * w * h;
      for (int r = h - 1; r >= 0; --r) {
        bool any = false;
        for (uint8_t col = 0; col < w; ++col) {
          if (frame[r * w + col] != 0xFF) { any = true; break; }
        }
        if (any) { if ((uint8_t)(r + 1) > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }

  loaded = true;
  return true;
}

void PmdMon::unload() {
  if (blob) {
    free(blob);
    if (blobSize <= spriteRamHeld) spriteRamHeld -= blobSize;
  }
  blob = nullptr;
  blobSize = 0;
  loaded = false;
  palCount = 0;
  for (uint8_t i = 0; i < PMD_NACTS; ++i) acts[i] = PmdAct{};
}

bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!ensureSdMounted()) return false;

  char path[24];
  snprintf(path, sizeof(path), "/mons/%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SD.open(path, FILE_READ);
  if (!f && shiny) {
    snprintf(path, sizeof(path), "/mons/%03u.bin", dexNum);
    f = SD.open(path, FILE_READ);
  }
  if (!f) return false;

  char magic[4];
  uint16_t header[4] = {};
  if (!readExact(f, magic, 4) || memcmp(magic, "TPK1", 4) != 0 || !readExact(f, header, 8)) {
    f.close();
    return false;
  }
  w = header[0]; h = header[1]; frames = header[2]; frameMs = header[3];
  if (!readExact(f, &palCount, 2) || palCount > 256 || w == 0 || w > 128 || h == 0 || h > 128 || frames == 0 || frames > 32) {
    f.close();
    return false;
  }
  if (!readExact(f, pal, palCount * 2)) {
    f.close();
    return false;
  }

  uint32_t size = (uint32_t)w * h * frames;
  if (size == 0 || size > 60UL * 1024UL || spriteRamHeld + size > SPRITE_RAM_BUDGET) {
    f.close();
    return false;
  }
  data = (uint8_t *)malloc(size);
  if (!data) {
    f.close();
    return false;
  }
  dataSize = size;
  spriteRamHeld += size;
  if (f.read(data, size) != (int)size) {
    f.close();
    unload();
    return false;
  }
  f.close();

  scale = (uint8_t)(200 / h);
  if (scale < 1) scale = 1;
  if (scale > 5) scale = 5;
  loaded = true;
  return true;
}

void SdMon::unload() {
  if (data) {
    free(data);
    if (dataSize <= spriteRamHeld) spriteRamHeld -= dataSize;
  }
  data = nullptr;
  dataSize = 0;
  loaded = false;
}

void SdThumbs::unload() {
  loaded = false;
  count = 0;
  size = 0;
  memset(offsets, 0, sizeof(offsets));
  memset(scratch, 0, sizeof(scratch));
  cachedDex = -1;
}

bool SdThumbs::load() {
  unload();
  if (!ensureSdMounted()) return false;

  File f = SD.open("/mons/thumbs.bin", FILE_READ);
  if (!f) return false;
  uint32_t sz = f.size();
  if (sz < 10 || sz > 64UL * 1024UL) {
    f.close();
    return false;
  }

  uint8_t hdr[6] = {};
  if (!readExact(f, hdr, sizeof(hdr)) || memcmp(hdr, "TPTH", 4) != 0) {
    f.close();
    return false;
  }
  count = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
  if (count == 0 || count > 151 || (uint32_t)6 + 4UL * count > sz) {
    f.close();
    unload();
    return false;
  }
  if (!readExact(f, offsets, 4UL * count)) {
    f.close();
    unload();
    return false;
  }
  f.close();
  size = sz;
  loaded = true;
  cachedDex = -1;
  return true;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > (int16_t)count) return nullptr;
  if (dex == cachedDex) return scratch;

  uint32_t off = offsets[dex - 1];
  if (off >= size || size - off < 3) return nullptr;

  if (!ensureSdMounted()) return nullptr;
  File f = SD.open("/mons/thumbs.bin", FILE_READ);
  if (!f) return nullptr;
  if (!f.seek(off, SeekSet)) {
    f.close();
    return nullptr;
  }

  uint8_t head[3] = {};
  if (!readExact(f, head, sizeof(head))) {
    f.close();
    return nullptr;
  }
  uint32_t palBytes = (uint32_t)head[2] * 2;
  uint32_t pixelBytes = (uint32_t)head[0] * head[1];
  uint32_t need = 3 + palBytes + pixelBytes;
  if (head[0] == 0 || head[1] == 0 || head[2] > 255 || need > sizeof(scratch) || need > size - off) {
    f.close();
    return nullptr;
  }

  memcpy(scratch, head, 3);
  if (f.read(scratch + 3, need - 3) != (int)(need - 3)) {
    f.close();
    return nullptr;
  }
  f.close();
  cachedDex = dex;
  return scratch;
}

void sdInvalidateMount() {
  sdMounted = false;
  sdReady = false;
}

bool sdBegin() {
  sdReady = SD.begin(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  sdMounted = sdReady;
  if (sdReady) SD.mkdir("/mons");
  return sdReady;
}

bool sdSerialCommand(const String &line) {
  if (!ensureSdMounted()) {
    Serial.println("ERR");
    return true;
  }

  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' ');
    if (sp <= 4) { Serial.println("ERR"); return true; }
    String path = line.substring(4, sp);
    uint32_t size = line.substring(sp + 1).toInt();
    if (size == 0 || size > 60UL * 1024UL || !path.startsWith("mons/") || path.indexOf("..") >= 0) {
      Serial.println("ERR");
      return true;
    }
    if (!path.startsWith("/")) path = "/" + path;
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) { Serial.println("ERR"); return true; }

    Serial.println("OK");
    static uint8_t buf[1024];
    uint32_t left = size;
    Serial.setTimeout(5000);
    while (left) {
      size_t want = left > sizeof(buf) ? sizeof(buf) : left;
      size_t n = Serial.readBytes(buf, want);
      if (!n || f.write(buf, n) != n) { left = 1; break; }
      left -= n;
      Serial.println("#");
    }
    f.close();
    Serial.setTimeout(1000);
    sdDirty = (left == 0);
    Serial.println(sdDirty ? "DONE" : "ERR");
    return true;
  }

  if (line == "SDINFO") {
    FSInfo info{};
    if (SD.info(info)) {
      Serial.printf("sd=1 size=%llu used=%llu\n",
                    (unsigned long long)info.totalBytes,
                    (unsigned long long)info.usedBytes);
      Serial.println("DONE");
    } else {
      Serial.println("ERR");
    }
    return true;
  }

  if (line == "LS") {
    File dir = SD.open("/mons");
    if (dir) {
      File e;
      while ((e = dir.openNextFile())) {
        Serial.printf("%s %u\n", e.name(), (uint32_t)e.size());
        e.close();
      }
      dir.close();
    }
    Serial.println("DONE");
    return true;
  }

  return false;
}
