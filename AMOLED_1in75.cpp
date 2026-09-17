#include "DEV_Config.h"
#include "AMOLED_1in75.h"

AMOLED_1IN75_ATTRIBUTES AMOLED_1IN75;

void AMOLED_1IN75_SetWindows(uint32_t Xstart, uint32_t Ystart, uint32_t Xend, uint32_t Yend) {
  Xstart += 6; Xend += 6;
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x2a);
  QSPI_DATA_Write(qspi, Xstart >> 8); QSPI_DATA_Write(qspi, Xstart & 0xff);
  QSPI_DATA_Write(qspi, (Xend - 1) >> 8); QSPI_DATA_Write(qspi, (Xend - 1) & 0xff); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x2b);
  QSPI_DATA_Write(qspi, Ystart >> 8); QSPI_DATA_Write(qspi, Ystart & 0xff);
  QSPI_DATA_Write(qspi, (Yend - 1) >> 8); QSPI_DATA_Write(qspi, (Yend - 1) & 0xff); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x2c); QSPI_Deselect(qspi);
}

static void AMOLED_1IN75_InitReg() {
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x11); delay(120); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0xc4); QSPI_DATA_Write(qspi, 0x80); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x44); QSPI_DATA_Write(qspi, 0x01); QSPI_DATA_Write(qspi, 0xD7); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x35); QSPI_DATA_Write(qspi, 0x00); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x53); QSPI_DATA_Write(qspi, 0x20); delay(10); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x29); delay(10); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x51); QSPI_DATA_Write(qspi, 0xA0); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x20); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x36); QSPI_DATA_Write(qspi, 0x00); QSPI_Deselect(qspi);
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x3A); QSPI_DATA_Write(qspi, 0x05); QSPI_Deselect(qspi);
}

static void AMOLED_1IN75_Reset(pio_qspi_t q) {
  gpio_put(q.pin_rst, 1); delay(50); gpio_put(q.pin_rst, 0); delay(50); gpio_put(q.pin_rst, 1); delay(300);
}

void AMOLED_1IN75_Init(void) {
  AMOLED_1IN75_Reset(qspi); AMOLED_1IN75_InitReg();
  AMOLED_1IN75.WIDTH = AMOLED_1IN75_WIDTH; AMOLED_1IN75.HEIGHT = AMOLED_1IN75_HEIGHT;
}

void AMOLED_1IN75_SetBrightness(uint8_t brightness) {
  if (brightness > 100) brightness = 100;
  brightness = (uint16_t)brightness * 255 / 100;
  QSPI_Select(qspi); QSPI_REGISTER_Write(qspi, 0x51); QSPI_DATA_Write(qspi, brightness); QSPI_Deselect(qspi);
}

static void dmaSend(const uint8_t *src, uint32_t len) {
  channel_config_set_dreq(&c, pio_get_dreq(qspi.pio, qspi.sm, true));
  dma_channel_configure(dma_tx, &c, &qspi.pio->txf[qspi.sm], src, len, true);
  while (dma_channel_is_busy(dma_tx)) {}
}

void AMOLED_1IN75_Clear(uint16_t Color) {
  static uint16_t row[AMOLED_1IN75_WIDTH];
  for (size_t i = 0; i < AMOLED_1IN75_WIDTH; ++i) row[i] = (uint16_t)((Color >> 8) | (Color << 8));
  AMOLED_1IN75_SetWindows(0, 0, AMOLED_1IN75_WIDTH, AMOLED_1IN75_HEIGHT);
  QSPI_Select(qspi); QSPI_Pixel_Write(qspi, 0x2c);
  for (uint16_t y = 0; y < AMOLED_1IN75_HEIGHT; ++y) dmaSend((const uint8_t *)row, AMOLED_1IN75_WIDTH * 2);
  QSPI_Deselect(qspi);
}

void AMOLED_1IN75_Display(uint16_t *Image) {
  AMOLED_1IN75_SetWindows(0, 0, AMOLED_1IN75_WIDTH, AMOLED_1IN75_HEIGHT);
  QSPI_Select(qspi); QSPI_Pixel_Write(qspi, 0x2c);
  dmaSend((const uint8_t *)Image, AMOLED_1IN75_WIDTH * AMOLED_1IN75_HEIGHT * 2);
  QSPI_Deselect(qspi);
}

void AMOLED_1IN75_DisplayWindows(uint32_t Xstart, uint32_t Ystart, uint32_t Xend, uint32_t Yend, uint16_t *Image) {
  if (Xend <= Xstart || Yend <= Ystart) return;
  if (Xend > AMOLED_1IN75_WIDTH) Xend = AMOLED_1IN75_WIDTH;
  if (Yend > AMOLED_1IN75_HEIGHT) Yend = AMOLED_1IN75_HEIGHT;
  AMOLED_1IN75_SetWindows(Xstart, Ystart, Xend, Yend);
  QSPI_Select(qspi); QSPI_Pixel_Write(qspi, 0x2c);
  channel_config_set_dreq(&c, pio_get_dreq(qspi.pio, qspi.sm, true));
  for (uint32_t y = Ystart; y < Yend; ++y) {
    const uint8_t *row = (const uint8_t *)Image + (y * AMOLED_1IN75_WIDTH + Xstart) * 2;
    dma_channel_configure(dma_tx, &c, &qspi.pio->txf[qspi.sm], row, (Xend - Xstart) * 2, true);
    while (dma_channel_is_busy(dma_tx)) {}
  }
  QSPI_Deselect(qspi);
}
