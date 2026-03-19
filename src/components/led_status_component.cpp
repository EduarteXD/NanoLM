#include "led_status_component.h"

#include <FastLED.h>

#include "../settings.h"

namespace LedStatusComponent
{

  static Stage sStage = STAGE_BOOTING;
  static bool sApMode = false;
  static uint8_t sSensorState = SENSOR_STATE_PENDING;
  static CRGB sLeds[SystemResources::STATUS_LED_COUNT];
  static uint32_t sLastFrameMs = 0;
  static uint8_t sLastR = 255;
  static uint8_t sLastG = 255;
  static uint8_t sLastB = 255;

  static void applyColor(uint8_t r, uint8_t g, uint8_t b)
  {
    if (r == sLastR && g == sLastG && b == sLastB)
    {
      return;
    }
    sLastR = r;
    sLastG = g;
    sLastB = b;
    sLeds[0] = CRGB(r, g, b);
    FastLED.show();
  }

  static uint8_t breatheLevel(uint32_t now, uint16_t periodMs, uint8_t minV,
                              uint8_t maxV)
  {
    if (periodMs < 2U || maxV <= minV)
    {
      return maxV;
    }
    const uint32_t phase = now % periodMs;
    const uint32_t half = periodMs / 2U;
    if (half == 0U)
    {
      return maxV;
    }
    const uint32_t tri = (phase <= half) ? phase : (periodMs - phase);
    const uint16_t span = static_cast<uint16_t>(maxV - minV);
    return static_cast<uint8_t>(minV + (span * tri) / half);
  }

  static void applyBreathingColor(uint8_t baseR, uint8_t baseG, uint8_t baseB,
                                  uint32_t now, uint16_t periodMs, uint8_t minV,
                                  uint8_t maxV)
  {
    const uint8_t lvl = breatheLevel(now, periodMs, minV, maxV);
    const uint8_t r = static_cast<uint8_t>((uint16_t(baseR) * lvl) / 255U);
    const uint8_t g = static_cast<uint8_t>((uint16_t(baseG) * lvl) / 255U);
    const uint8_t b = static_cast<uint8_t>((uint16_t(baseB) * lvl) / 255U);
    applyColor(r, g, b);
  }

  void begin()
  {
    FastLED.addLeds<STATUS_LED_TYPE, SystemResources::PIN_STATUS_LED,
                    STATUS_LED_COLOR_ORDER>(sLeds, SystemResources::STATUS_LED_COUNT);
    FastLED.setBrightness(SystemResources::STATUS_LED_BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 300);
    FastLED.clear(true); // 强制清空，避免上电残留白灯
    sLastFrameMs = 0;
    applyColor(0, 0, 0);
  }

  void setStage(Stage stage)
  {
    sStage = stage;
  }

  void setNetworkState(bool apMode)
  {
    sApMode = apMode;
  }

  void setSensorState(uint8_t sensorState)
  {
    sSensorState = sensorState;
  }

  void update()
  {
    const uint32_t now = millis();
    if ((now - sLastFrameMs) < 20U)
    {
      return;
    }
    sLastFrameMs = now;

    if (sStage == STAGE_BOOTING)
    {
      // 启动：蓝色呼吸
      applyBreathingColor(0, 0, 255, now, 1800U, 18U, 255U);
      return;
    }

    if (sStage == STAGE_WIFI_CONNECTING)
    {
      // 连网：青色呼吸
      applyBreathingColor(0, 255, 220, now, 900U, 20U, 255U);
      return;
    }

    if (sSensorState == SENSOR_STATE_ERROR)
    {
      // 异常：红色呼吸（用户关注的重点）
      applyBreathingColor(255, 0, 0, now, 1000U, 16U, 255U);
      return;
    }
    if (sSensorState == SENSOR_STATE_SATURATED)
    {
      // 过曝：琥珀色呼吸
      applyBreathingColor(255, 150, 0, now, 1100U, 16U, 255U);
      return;
    }
    if (sSensorState == SENSOR_STATE_PENDING)
    {
      // 等待数据：紫色呼吸
      applyBreathingColor(170, 0, 255, now, 1400U, 14U, 255U);
      return;
    }

    if (sApMode)
    {
      // 正常 AP：蓝青常亮
      applyColor(0, 85, 130);
    }
    else
    {
      // 网络未知：紫色呼吸
      applyBreathingColor(170, 0, 255, now, 1500U, 10U, 220U);
    }
  }

}
