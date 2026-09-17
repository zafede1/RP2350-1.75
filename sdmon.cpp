#include "sdmon.h"

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

bool SdMon::load(uint8_t, bool) { unload(); return false; }
void SdMon::unload() { loaded = false; data = nullptr; }
bool PmdMon::load(uint8_t, bool) { unload(); return false; }
void PmdMon::unload() { loaded = false; blob = nullptr; for (uint8_t i = 0; i < PMD_NACTS; ++i) acts[i] = PmdAct{}; }
bool SdThumbs::load() { unload(); return false; }
void SdThumbs::unload() { loaded = false; data = nullptr; count = 0; size = 0; }
const uint8_t *SdThumbs::get(int16_t) const { return nullptr; }

bool sdBegin() { sdReady = false; return false; }
bool sdSerialCommand(const String &) { return false; }
