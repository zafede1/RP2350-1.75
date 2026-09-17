#include "qspi_pio.h"
#include "pico/stdlib.h"

pio_qspi_t qspi = {
  .pio = pio0,
  .sm = 0,
  .sm_4wire = 0,
  .sm_1wire = 1,
  .pin_cs = PIN_CS,
  .pin_sclk = PIN_SCLK,
  .pin_dio0 = PIN_DIO0,
  .pin_dio1 = PIN_DIO1,
  .pin_dio2 = PIN_DIO2,
  .pin_dio3 = PIN_DIO3,
  .pin_pwr_en = 0,
  .pin_rst = PIN_RST
};

void QSPI_GPIO_Init(pio_qspi_t q) {
  gpio_init(q.pin_cs); gpio_pull_down(q.pin_cs); gpio_set_dir(q.pin_cs, GPIO_OUT); gpio_put(q.pin_cs, 1);
  gpio_init(q.pin_rst); gpio_set_dir(q.pin_rst, GPIO_OUT); gpio_put(q.pin_rst, 1);
}
void QSPI_Select(pio_qspi_t q) { gpio_put(q.pin_cs, 0); }
void QSPI_Deselect(pio_qspi_t q) { gpio_put(q.pin_cs, 1); }

void QSPI_PIO_Init(pio_qspi_t q) {
  uint offset = pio_add_program(q.pio, &qspi_4wire_data_program);
  qspi_4wire_data_program_init(q.pio, q.sm_4wire, offset, PIN_SCLK, PIN_DIO0, 4);
  pio_sm_set_enabled(q.pio, q.sm_4wire, false);
  pio_sm_set_enabled(q.pio, q.sm_1wire, false);
}
void QSPI_1Wrie_Mode(pio_qspi_t *q) {
  pio_sm_set_enabled(q->pio, q->sm_4wire, false);
  pio_sm_set_enabled(q->pio, q->sm_1wire, true);
  q->sm = q->sm_1wire;
}
void QSPI_4Wrie_Mode(pio_qspi_t *q) {
  pio_sm_set_enabled(q->pio, q->sm_4wire, true);
  pio_sm_set_enabled(q->pio, q->sm_1wire, false);
  q->sm = q->sm_4wire;
}

static void qspiWrite(pio_qspi_t q, uint32_t val) { pio_sm_put_blocking(q.pio, q.sm, val << 24); }

static void qspiPacked(pio_qspi_t q, uint32_t val) {
  uint8_t b[4];
  for (int i = 0; i < 4; ++i) {
    uint8_t bit1 = (val & (1u << (2 * i))) ? 1 : 0;
    uint8_t bit2 = (val & (1u << (2 * i + 1))) ? 1 : 0;
    b[3 - i] = bit1 | (bit2 << 4);
  }
  for (int i = 0; i < 4; ++i) qspiWrite(q, b[i]);
}
void QSPI_DATA_Write(pio_qspi_t q, uint32_t val) { qspiPacked(q, val); }
void QSPI_REGISTER_Write(pio_qspi_t q, uint32_t addr) {
  qspiPacked(q, 0x02); qspiPacked(q, 0x00); qspiPacked(q, addr); qspiPacked(q, 0x00);
}
void QSPI_Pixel_Write(pio_qspi_t q, uint32_t addr) {
  qspiPacked(q, 0x32); qspiPacked(q, 0x00); qspiPacked(q, addr); qspiPacked(q, 0x00);
}
