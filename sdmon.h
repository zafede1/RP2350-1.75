#pragma once
#include <Arduino.h>

// Sprite animado TPK1 (formato heredado, camino de respaldo). El proyecto usa
// PMD/TPK2 (PmdMon) para todo; esta ruta queda inactiva si no hay NNN.bin en la SD.
// RP2350 no necesita PSRAM: se impone un presupuesto comun de SRAM para sprites.
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
  const uint8_t *data = nullptr;
};

// sprite PMD multi-accion cargado de la SD a SRAM.
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  uint32_t blobSize = 0;
  PmdAct acts[PMD_NACTS];

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// Miniaturas de la galeria: solo se conserva la tabla de offsets y un unico
// buffer de trabajo. get() devuelve un puntero valido hasta la siguiente llamada.
// Asi la galeria no consume decenas de KB junto al framebuffer completo de 466x466.
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
