#pragma once

// CDC Badge Hardware Configuration
// All pin definitions in one place

#include "driver/gpio.h"

// I2C0 (Charging IC + Expander)
#define I2C0_SDA_PIN GPIO_NUM_17
#define I2C0_SCL_PIN GPIO_NUM_18

// I2C1 (Expansion header)
#define I2C1_SDA_PIN GPIO_NUM_47
#define I2C1_SCL_PIN GPIO_NUM_48

// IO Expander Interrupt
#define EXP_IRQ_PIN GPIO_NUM_1

// Flash/BOOT Button (Power Off Trigger)
#define FLASH_BTN_PIN GPIO_NUM_0

// EPD Backlight
#define EPD_LED_PIN GPIO_NUM_8

// EPD Control Pins (defined in CalEPD Kconfig, repeated here for reference)
#define EPD_CS_PIN GPIO_NUM_41
#define EPD_DC_PIN GPIO_NUM_45
#define EPD_RST_PIN GPIO_NUM_46
#define EPD_BUSY_PIN GPIO_NUM_42

// I2C Addresses
#define BQ25895_ADDR 0x6A
#define EXPANDER_ADDR 0x20

// BQ25895 Pins
#define CHG_DSEL_PIN GPIO_NUM_21
#define CHG_IRQ_PIN GPIO_NUM_39

// SPI Bus Pins
#define SPI_SCLK_PIN GPIO_NUM_12
#define SPI_MISO_PIN GPIO_NUM_11
#define SPI_MOSI_PIN GPIO_NUM_13

// TROPIC01 Secure Element
#define TR01_CS_PIN GPIO_NUM_10
