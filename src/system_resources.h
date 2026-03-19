#pragma once

#include <Arduino.h>

// 系统资源表：集中维护板级 IO 与用途，便于后续扩展和移植。
namespace SystemResources
{

  // I2C 总线 0（TSL2591）
  static constexpr uint8_t PIN_I2C_SDA = 4;
  static constexpr uint8_t PIN_I2C_SCL = 5;
  static constexpr uint8_t I2C_ADDR_TSL2591 = 0x29;

  // EPD 接口（4-wire SPI, 8-bit 帧 + I2C 总线 1 控制 PCF8574）
  static constexpr uint8_t PIN_EPD_SPI_SCLK = 0;
  static constexpr uint8_t PIN_EPD_SPI_MOSI = 1;
  static constexpr uint8_t PIN_EPD_SPI_CS = 3;
  static constexpr uint8_t PIN_EPD_I2C_SDA = 7;
  static constexpr uint8_t PIN_EPD_I2C_SCL = 8;
  // PCF8574 7-bit 地址（默认 A2/A1/A0=0 -> 0x20）。
  static constexpr uint8_t I2C_ADDR_EPD_PCF8574 = 0x20;

  // 状态灯（外接 SK6805 单灯珠，数据脚）
  static constexpr uint8_t PIN_STATUS_LED = 21;
  static constexpr uint16_t STATUS_LED_COUNT = 1;
  // 状态灯亮度（0-255）。若仍偏暗可继续上调。
  static constexpr uint8_t STATUS_LED_BRIGHTNESS = 150;

  // 模式切换按键（低电平触发，建议上拉输入）
  static constexpr uint8_t PIN_MODE_SWITCH = 9;
  static constexpr uint16_t MODE_SWITCH_DEBOUNCE_MS = 35;

  // 串口波特率
  static constexpr uint32_t SERIAL_BAUD = 115200;

  // 资源条目定义（用于文档化输出）
  struct IoEntry
  {
    const char *name;
    uint8_t pin;
    const char *mode;
    const char *description;
  };

  // 当前系统 IO 映射表
  static constexpr IoEntry IO_TABLE[] = {
      {"I2C_SDA", PIN_I2C_SDA, "I2C", "TSL2591 数据线"},
      {"I2C_SCL", PIN_I2C_SCL, "I2C", "TSL2591 时钟线"},
      {"EPD_SPI_SCLK", PIN_EPD_SPI_SCLK, "SPI", "EPD 4-wire SPI 时钟"},
      {"EPD_SPI_MOSI", PIN_EPD_SPI_MOSI, "SPI", "EPD 4-wire SPI 数据"},
      {"EPD_SPI_CS", PIN_EPD_SPI_CS, "SPI", "EPD 片选"},
      {"EPD_I2C_SDA", PIN_EPD_I2C_SDA, "SOFT-I2C", "GPIO7 -> PCF8574 SDA"},
      {"EPD_I2C_SCL", PIN_EPD_I2C_SCL, "SOFT-I2C", "GPIO8 -> PCF8574 SCL"},
      {"STATUS_LED", PIN_STATUS_LED, "ONEWIRE", "SK6805 DIN"},
      {"MODE_SWITCH", PIN_MODE_SWITCH, "INPUT_PULLUP", "低电平触发网络模式切换（AP <-> STA） 暂时弃用"},
  };

  static constexpr size_t IO_TABLE_SIZE = sizeof(IO_TABLE) / sizeof(IO_TABLE[0]);

}
