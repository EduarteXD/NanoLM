#pragma once

#include <Arduino.h>

namespace LedStatusComponent
{

  enum Stage : uint8_t
  {
    STAGE_BOOTING = 0,
    STAGE_WIFI_CONNECTING = 1,
    STAGE_RUNNING = 2,
  };

  void begin();
  void setStage(Stage stage);
  void setNetworkState(bool apMode);
  void setSensorState(uint8_t sensorState);
  void update();

}
