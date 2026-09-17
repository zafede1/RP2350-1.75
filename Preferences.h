#pragma once
#ifdef ARDUINO_ARCH_RP2040
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

class Preferences {
  static constexpr uint16_t EEPROM_SIZE = 4096;
  static constexpr uint8_t SLOT_COUNT = 63;
  static constexpr uint8_t SLOT_SIZE = 64;
  static constexpr uint8_t KEY_SIZE = 16;
  static constexpr uint8_t DATA_SIZE = 44;
public:
  bool begin(const char *ns, bool readOnly = false, const char *partition = "") {
    (void)ns; (void)partition; readOnly_ = readOnly;
    EEPROM.begin(EEPROM_SIZE);
    if (EEPROM.read(0) != 'T' || EEPROM.read(1) != 'P' || EEPROM.read(2) != 'R' || EEPROM.read(3) != '1') {
      for (uint16_t i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i, 0xFF);
      EEPROM.write(0, 'T'); EEPROM.write(1, 'P'); EEPROM.write(2, 'R'); EEPROM.write(3, '1');
      EEPROM.commit();
    }
    return true;
  }
  void end() { if (!readOnly_) EEPROM.commit(); }
  void clear() { if (readOnly_) return; for (uint16_t i = 4; i < EEPROM_SIZE; ++i) EEPROM.write(i, 0xFF); EEPROM.commit(); }
  bool isKey(const char *key) const { return findSlot(key) >= 0; }
  size_t putBytes(const char *key, const void *value, size_t len) { return putRaw(key, value, min(len, (size_t)DATA_SIZE), 1); }
  size_t getBytes(const char *key, void *value, size_t len) const { return getRaw(key, value, len); }
  size_t putString(const char *key, const String &value) { return putRaw(key, value.c_str(), min((size_t)value.length(), (size_t)(DATA_SIZE - 1)), 2); }
  size_t putString(const char *key, const char *value) { size_t n = putString(key, String(value)); if (!readOnly_) EEPROM.commit(); return n; }
  String getString(const char *key, const String &def = String()) const {
    char buf[DATA_SIZE + 1]; memset(buf, 0, sizeof(buf));
    size_t n = getRaw(key, buf, DATA_SIZE); if (!n) return def; buf[min(n, (size_t)DATA_SIZE)] = 0; return String(buf);
  }
  size_t getString(const char *key, char *value, size_t maxLen) const {
    if (!value || maxLen == 0) return 0;
    size_t n = getRaw(key, value, maxLen - 1); value[n] = 0; return n;
  }
  uint8_t putUChar(const char *key, uint8_t v) { return (uint8_t)putNum(key, v, 3); }
  uint8_t getUChar(const char *key, uint8_t def = 0) const { uint8_t v; return getNum(key, v, def); }
  bool putBool(const char *key, bool v) { return putNum(key, (uint8_t)v, 4) == 1; }
  bool getBool(const char *key, bool def = false) const { uint8_t v; return getNum(key, v, (uint8_t)def) != 0; }
  uint32_t putUInt(const char *key, uint32_t v) { return putNum32(key, v, 5); }
  uint32_t getUInt(const char *key, uint32_t def = 0) const { return getNum32(key, def); }
  int16_t putShort(const char *key, int16_t v) { putNum32(key, (uint16_t)v, 6); return v; }
  int16_t getShort(const char *key, int16_t def = 0) const { return (int16_t)getNum32(key, (uint16_t)def); }
  uint16_t putUShort(const char *key, uint16_t v) { putNum32(key, v, 7); return v; }
  uint16_t getUShort(const char *key, uint16_t def = 0) const { return (uint16_t)getNum32(key, def); }
  int8_t putChar(const char *key, int8_t v) { putNum(key, (uint8_t)v, 8); return v; }
  int8_t getChar(const char *key, int8_t def = 0) const { uint8_t v; return (int8_t)getNum(key, v, (uint8_t)def); }
  bool commit() { return EEPROM.commit(); }
private:
  bool readOnly_ = false;
  int slotOffset(uint8_t s) const { return 4 + s * SLOT_SIZE; }
  int findSlot(const char *key) const {
    for (uint8_t s = 0; s < SLOT_COUNT; ++s) {
      int off = slotOffset(s); if (EEPROM.read(off) != 0xA5) continue;
      char k[KEY_SIZE + 1] = {};
      for (uint8_t i = 0; i < KEY_SIZE; ++i) k[i] = (char)EEPROM.read(off + 4 + i);
      if (strncmp(k, key, KEY_SIZE) == 0) return off;
    }
    return -1;
  }
  int allocSlot(const char *key) {
    int found = findSlot(key); if (found >= 0) return found;
    for (uint8_t s = 0; s < SLOT_COUNT; ++s) { int off = slotOffset(s); if (EEPROM.read(off) == 0xFF) return off; }
    return -1;
  }
  size_t putRaw(const char *key, const void *value, size_t len, uint8_t type) {
    if (readOnly_) return 0; int off = allocSlot(key); if (off < 0 || len > DATA_SIZE) return 0;
    EEPROM.write(off, 0xA5); EEPROM.write(off + 1, type); EEPROM.write(off + 2, (uint8_t)len); EEPROM.write(off + 3, 0);
    for (uint8_t i = 0; i < KEY_SIZE; ++i) EEPROM.write(off + 4 + i, key[i] ? key[i] : 0);
    const uint8_t *p = (const uint8_t *)value;
    for (uint8_t i = 0; i < DATA_SIZE; ++i) EEPROM.write(off + 20 + i, i < len ? p[i] : 0);
    return len;
  }
  size_t getRaw(const char *key, void *value, size_t len) const {
    int off = findSlot(key); if (off < 0) return 0; size_t n = min((size_t)EEPROM.read(off + 2), len);
    uint8_t *p = (uint8_t *)value; for (size_t i = 0; i < n; ++i) p[i] = EEPROM.read(off + 20 + i); return n;
  }
  template <typename T> size_t putNum(const char *key, T v, uint8_t type) { return putRaw(key, &v, sizeof(v), type); }
  template <typename T> T getNum(const char *key, T tmp, T def) const { (void)tmp; T v; return getRaw(key, &v, sizeof(v)) == sizeof(v) ? v : def; }
  size_t putNum32(const char *key, uint32_t v, uint8_t type) { return putRaw(key, &v, sizeof(v), type); }
  uint32_t getNum32(const char *key, uint32_t def) const { uint32_t v; return getRaw(key, &v, sizeof(v)) == sizeof(v) ? v : def; }
};
#else
#include_next <Preferences.h>
#endif
