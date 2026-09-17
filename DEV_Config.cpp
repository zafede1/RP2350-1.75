#include "DEV_Config.h"
#include "qspi_pio.h"

uint dma_tx;
dma_channel_config c;

void DEV_Digital_Write(uint16_t pin, uint8_t value) { gpio_put(pin, value); }
uint8_t DEV_Digital_Read(uint16_t pin) { return gpio_get(pin); }
void DEV_GPIO_Mode(uint16_t pin, uint16_t mode) { gpio_init(pin); gpio_set_dir(pin, (mode == GPIO_IN) ? GPIO_IN : GPIO_OUT); }
void DEV_KEY_Config(uint16_t pin) { gpio_init(pin); gpio_pull_up(pin); gpio_set_dir(pin, GPIO_IN); }
void DEV_Delay_ms(uint32_t ms) { delay(ms); }
void DEV_Delay_us(uint32_t us) { delayMicroseconds(us); }

void DEV_I2C_Write_Byte(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(addr); Wire1.write(reg); Wire1.write(value); Wire1.endTransmission();
}
uint8_t DEV_I2C_Read_Byte(uint8_t addr, uint8_t reg) {
  Wire1.beginTransmission(addr); Wire1.write(reg); Wire1.endTransmission(false);
  if (Wire1.requestFrom(addr, (uint8_t)1) != 1) return 0;
  return Wire1.read();
}
void DEV_I2C_Read_nByte(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len) {
  Wire1.beginTransmission(addr); Wire1.write(reg); Wire1.endTransmission(false);
  uint32_t got = Wire1.requestFrom(addr, (uint8_t)len);
  for (uint32_t i = 0; i < got && i < len; ++i) data[i] = Wire1.read();
  for (uint32_t i = got; i < len; ++i) data[i] = 0;
}
void DEV_IRQ_SET(uint gpio, uint32_t events, gpio_irq_callback_t callback) { gpio_set_irq_enabled_with_callback(gpio, events, true, callback); }

uint8_t DEV_Module_Init(void) {
  Serial.begin(115200);
  delay(100);
  set_sys_clock_khz(PLL_SYS_KHZ, true);
  gpio_init(TOUCH_RST_PIN); gpio_set_dir(TOUCH_RST_PIN, GPIO_OUT);

  dma_tx = dma_claim_unused_channel(true);
  c = dma_channel_get_default_config(dma_tx);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  irq_set_enabled(DMA_IRQ_0, false);

  Wire1.setSDA(DEV_SDA_PIN);
  Wire1.setSCL(DEV_SCL_PIN);
  Wire1.setClock(400000);
  Wire1.begin();
  return 0;
}
void DEV_Module_Exit(void) {
  if (dma_tx) dma_channel_abort(dma_tx);
  Wire1.end();
}
