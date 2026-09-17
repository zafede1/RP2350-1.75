#pragma once
#include <Wire.h>

// Waveshare RP2350 Touch AMOLED 1.75" — official pinout.
#define XPOWERS_CHIP_AXP2101
#define LCD_SDIO0 12
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_SCLK 11
#define LCD_CS 10
#define LCD_RESET 16
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define IIC_SDA 6
#define IIC_SCL 7
#define TP_INT 22
#define TP_RESET 23

// Waveshare ES8311 audio pins.
#define I2S_BCK_IO 4
#define I2S_WS_IO 5
#define I2S_DO_IO 1
#define I2S_DI_IO 2
#define I2S_MCK_IO 3
#define PA 0

// SDIO PIO pins.
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41

#ifndef RGB565_BLACK
#define RGB565_BLACK 0x0000
#endif

#ifdef ARDUINO_ARCH_RP2040
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#endif
