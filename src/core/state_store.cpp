#include "state_store.h"

#include <Arduino.h>

#include "../app/app_runtime.h"

bool storeSensorData(const SensorData &in) {
  SemaphoreHandle_t mutex = AppRuntime::sensorMutex();
  if (mutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  AppRuntime::sensorData() = in;
  AppRuntime::sensorData().sampleMs = millis();
  xSemaphoreGive(mutex);
  return true;
}

bool loadSensorData(SensorData &out) {
  SemaphoreHandle_t mutex = AppRuntime::sensorMutex();
  if (mutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  out = AppRuntime::sensorData();
  xSemaphoreGive(mutex);
  return true;
}

bool loadMeterConfig(MeterConfig &out) {
  SemaphoreHandle_t mutex = AppRuntime::configMutex();
  if (mutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  out = AppRuntime::meterConfig();
  xSemaphoreGive(mutex);
  return true;
}

bool saveMeterConfig(const MeterConfig &in) {
  SemaphoreHandle_t mutex = AppRuntime::configMutex();
  if (mutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  AppRuntime::meterConfig() = in;
  xSemaphoreGive(mutex);
  return true;
}
