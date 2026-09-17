#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"

#define PLL_SYS_KHZ (200 * 1000)
#define DEV_SDA_PIN 6
#define DEV_SCL_PIN 7
#define TOUCH_RST_PIN 23
#define TOUCH_INT_PIN 22
#define SYS_OUT 9
#define I2C_PORT i2c1

extern uint dma_tx;
extern dma_channel_config c;
void DEV_Digital_Write(uint16_t pin, uint8_t value);
uint8_t DEV_Digital_Read(uint16_t pin);
void DEV_GPIO_Mode(uint16_t pin, uint16_t mode);
void DEV_KEY_Config(uint16_t pin);
void DEV_Delay_ms(uint32_t ms);
void DEV_Delay_us(uint32_t us);
void DEV_I2C_Write_Byte(uint8_t addr, uint8_t reg, uint8_t value);
uint8_t DEV_I2C_Read_Byte(uint8_t addr, uint8_t reg);
void DEV_I2C_Read_nByte(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len);
void DEV_IRQ_SET(uint gpio, uint32_t events, gpio_irq_callback_t callback);
uint8_t DEV_Module_Init(void);
void DEV_Module_Exit(void);
