#pragma once
#include <Arduino.h>

// Sprite animado TPK1 (formato heredado, camino de respaldo). El proyecto usa
// PMD/TPK2 (PmdMon) para todo; esta ruta queda inactiva si no hay NNN.bin en la SD.
// Los datos viven en SRAM solo mientras hacen falta para una animacion.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;
  uint32_t dataSize = 0;

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// acciones de los sprites PMD (formato TPK2)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;
  uint16_t ms[24];
  uint32_t dataOffset = 0; // offset del primo pixel del primo frame nel file
};

// Sprite PMD multi-accion: il file resta sulla SD; in SRAM si tiene solo
// la palette + metadati + un buffer per il frame che si sta disegnando.
// Questo permette di usare i file originali da 80-400+ KB anche senza PSRAM.
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  PmdAct acts[PMD_NACTS];

  uint8_t *frameBuf = nullptr;
  uint32_t frameBufSize = 0;
  uint8_t cachedAct = 0xFF;
  uint8_t cachedFrame = 0xFF;
  char filePath[32] = {};

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
  const uint8_t *frameData(uint8_t actId, uint8_t frame);
};

// Miniaturas della galleria: si conserva solo la tabella degli offset e un unico
// buffer di lavoro. get() restituisce un puntatore valido fino alla prossima call.
// Le 151 miniatures ocupan ~169 KB en SD, ma non vengono caricate tutte in SRAM.
struct SdThumbs {
  bool loaded = false;
  uint16_t count = 0;
  uint32_t size = 0;
  uint32_t offsets[151] = {};
  mutable uint8_t scratch[4096] = {};
  mutable int16_t cachedDex = -1;

  bool load();
  void unload();
  const uint8_t *get(int16_t dex) const;
};
extern SdThumbs thumbs;

bool sdBegin();
bool sdSerialCommand(const String &line);
void sdInvalidateMount();
extern bool sdReady;
extern bool sdDirty;
