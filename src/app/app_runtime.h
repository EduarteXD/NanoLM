#pragma once

#include <WebServer.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "../app_types.h"

namespace AppRuntime {

bool initialize();

SensorData &sensorData();
MeterConfig &meterConfig();

WebServer &server();

SemaphoreHandle_t sensorMutex();
SemaphoreHandle_t configMutex();

TaskHandle_t sensorTaskHandle();
void setSensorTaskHandle(TaskHandle_t handle);

bool wifiApMode();
void setWifiApMode(bool apMode);

}  // namespace AppRuntime
