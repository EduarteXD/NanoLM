#pragma once

#include <WebServer.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "../app_types.h"

namespace AppRuntime
{

  bool initialize();

  SensorData &sensorData();
  MeterConfig &meterConfig();

  WebServer &server();

  SemaphoreHandle_t sensorMutex();
  SemaphoreHandle_t configMutex();

  TaskHandle_t sensorSamplingTaskHandle();
  void setSensorSamplingTaskHandle(TaskHandle_t handle);

  bool wifiApMode();
  void setWifiApMode(bool apMode);

}
