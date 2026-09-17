#pragma once
#include <Arduino.h>
#include <Wire.h>
#define CST9217_I2C_ADDR 0x5A
#define CST9217_CHIP_ID 0x9217
#define CST9217_DATA_REG 0xD000
#define CST9217_PROJECT_ID_REG 0xD204
#define CST9217_CMD_MODE_REG 0xD101
#define CST9217_CHECKCODE_REG 0xD1FC
#define CST9217_RESOLUTION_REG 0xD1F8
#define CST9217_ACK_VALUE 0xAB
#define CST9217_MAX_TOUCH_POINTS 2
#define CST9217_DATA_LENGTH (CST9217_MAX_TOUCH_POINTS * 5 + 5)
struct CST9217_Point { uint16_t x; uint16_t y; bool valid; };
struct CST9217_Struct { CST9217_Point data[CST9217_MAX_TOUCH_POINTS]; uint8_t points; };
extern CST9217_Struct CST9217;
void CST9217_Reset(void);
void CST9217_Init(void);
bool CST9217_Read_Config(void);
bool CST9217_Read_Data(void);
