#pragma once
#include <stdint.h>
#include "qspi_pio.h"
#define AMOLED_1IN75_WIDTH 466
#define AMOLED_1IN75_HEIGHT 466
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0

typedef struct { uint16_t WIDTH; uint16_t HEIGHT; uint8_t SCAN_DIR; } AMOLED_1IN75_ATTRIBUTES;
extern AMOLED_1IN75_ATTRIBUTES AMOLED_1IN75;
void AMOLED_1IN75_Init(void);
void AMOLED_1IN75_SetBrightness(uint8_t brightness);
void AMOLED_1IN75_SetWindows(uint32_t Xstart, uint32_t Ystart, uint32_t Xend, uint32_t Yend);
void AMOLED_1IN75_Display(uint16_t *Image);
void AMOLED_1IN75_DisplayWindows(uint32_t Xstart, uint32_t Ystart, uint32_t Xend, uint32_t Yend, uint16_t *Image);
void AMOLED_1IN75_Clear(uint16_t Color);
