#include "CST9217.h"
#include "DEV_Config.h"

CST9217_Struct CST9217 = {};

static bool readReg16(uint16_t reg, uint8_t *dst, size_t len) {
  Wire1.beginTransmission(CST9217_I2C_ADDR);
  Wire1.write((uint8_t)(reg >> 8));
  Wire1.write((uint8_t)reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(CST9217_I2C_ADDR, (uint8_t)len) != (int)len) return false;
  for (size_t i = 0; i < len; ++i) dst[i] = Wire1.read();
  return true;
}
static bool writeReg16(uint16_t reg, const uint8_t *src, size_t len) {
  Wire1.beginTransmission(CST9217_I2C_ADDR);
  Wire1.write((uint8_t)(reg >> 8)); Wire1.write((uint8_t)reg);
  for (size_t i = 0; i < len; ++i) Wire1.write(src[i]);
  return Wire1.endTransmission() == 0;
}

void CST9217_Reset(void) {
  gpio_put(TOUCH_RST_PIN, 0); delay(10); gpio_put(TOUCH_RST_PIN, 1); delay(50);
}

bool CST9217_Read_Config(void) {
  uint8_t mode[2] = {0xD1, 0x01};
  if (!writeReg16(CST9217_CMD_MODE_REG, mode, 2)) return false;
  delay(10);
  uint8_t data[4] = {};
  if (!readReg16(CST9217_CHECKCODE_REG, data, 4)) return false;
  if (!readReg16(CST9217_PROJECT_ID_REG, data, 4)) return false;
  uint16_t chipType = (uint16_t(data[3]) << 8) | data[2];
  return chipType == CST9217_CHIP_ID;
}

void CST9217_Init(void) {
  CST9217_Reset();
  Serial.println(CST9217_Read_Config() ? "CST9217 OK" : "CST9217 not detected");
}

bool CST9217_Read_Data(void) {
  uint8_t data[CST9217_DATA_LENGTH] = {};
  if (!readReg16(CST9217_DATA_REG, data, sizeof(data))) return false;
  CST9217.points = data[5] & 0x7F;
  if (CST9217.points > CST9217_MAX_TOUCH_POINTS) CST9217.points = CST9217_MAX_TOUCH_POINTS;
  for (uint8_t i = 0; i < CST9217_MAX_TOUCH_POINTS; ++i) CST9217.data[i].valid = false;
  for (uint8_t i = 0; i < CST9217.points; ++i) {
    uint8_t *p = &data[i * 5 + (i ? 2 : 0)];
    if ((p[0] & 0x0F) != 0x06) continue;
    CST9217.data[i].x = (uint16_t)((p[1] << 4) | (p[3] >> 4));
    CST9217.data[i].y = (uint16_t)((p[2] << 4) | (p[3] & 0x0F));
    CST9217.data[i].x = 466 - CST9217.data[i].x;
    CST9217.data[i].y = 466 - CST9217.data[i].y;
    CST9217.data[i].valid = true;
  }
  return data[6] == CST9217_ACK_VALUE;
}
