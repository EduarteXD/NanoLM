#pragma once

#include <Arduino.h>
#include "system_resources.h"

#define STATUS_LED_TYPE WS2812B
#define STATUS_LED_COLOR_ORDER GRB

// -------------------- 功能开关 --------------------
static constexpr bool ENABLE_EPD_SELF_TEST_ON_BOOT = true;
static constexpr bool ENABLE_BLE = true;

// -------------------- WiFi 配置 --------------------
// 仅 AP 模式。
static const char *WIFI_AP_SSID = "NanoLm";
static const char *WIFI_AP_PASS = "12345678";

// -------------------- BLE 配置 --------------------
static const char *BLE_DEVICE_NAME = "NanoLm";
static constexpr uint8_t BLE_PROTOCOL_VERSION = 1;

// -------------------- TSL2591 寄存器 --------------------
static constexpr uint8_t TSL2591_CMD = 0xA0; // 命令位 + 普通寄存器访问
static constexpr uint8_t REG_ENABLE = 0x00;
static constexpr uint8_t REG_CONTROL = 0x01;
static constexpr uint8_t REG_ID = 0x12;
static constexpr uint8_t REG_STATUS = 0x13;
static constexpr uint8_t REG_C0DATAL = 0x14;
static constexpr uint8_t REG_C1DATAL = 0x16;

static constexpr uint8_t ENABLE_PON = 0x01;
static constexpr uint8_t ENABLE_AEN = 0x02;

// 增益档（CONTROL[5:4]）
static constexpr uint8_t GAIN_LOW_1X = 0x00;
static constexpr uint8_t GAIN_MED_25X = 0x01;
static constexpr uint8_t GAIN_HIGH_428X = 0x02;
static constexpr uint8_t GAIN_MAX_9876X = 0x03;

// 积分时间档（CONTROL[2:0]）
static constexpr uint8_t INT_100MS = 0x00;
static constexpr uint8_t INT_200MS = 0x01;
static constexpr uint8_t INT_300MS = 0x02;
static constexpr uint8_t INT_400MS = 0x03;
static constexpr uint8_t INT_500MS = 0x04;
static constexpr uint8_t INT_600MS = 0x05;

// Lux 计算常数（TSL2591 官方/通用库常用参数）
// https://github.com/adafruit/Adafruit_TSL2591_Library/blob/master/Adafruit_TSL2591.h
static constexpr float LUX_DF = 408.0f;
static constexpr float LUX_COEFB = 1.64f;
static constexpr float LUX_COEFC = 0.59f;
static constexpr float LUX_COEFD = 0.86f;

// -------------------- 测光默认参数 --------------------
// EV100 = log2((lux * 100) / C)
static constexpr float DEFAULT_CALIB_C = 250.0f;
static constexpr uint16_t DEFAULT_ISO = 100;
static constexpr float DEFAULT_APERTURE = 2.8f;
static constexpr float DEFAULT_SHUTTER_SEC = 1.0f / 125.0f;
static constexpr float DEFAULT_EXP_COMP = 0.0f;

// 传感器状态枚举
static constexpr uint8_t SENSOR_STATE_PENDING = 0;
static constexpr uint8_t SENSOR_STATE_OK = 1;
static constexpr uint8_t SENSOR_STATE_SATURATED = 2;
static constexpr uint8_t SENSOR_STATE_ERROR = 3;

// 测光模式枚举
static constexpr uint8_t METER_MODE_APERTURE_PRIORITY = 0;
static constexpr uint8_t METER_MODE_SHUTTER_PRIORITY = 1;

// 自动量程开关
static constexpr bool AUTO_RANGE_ENABLED = true;
