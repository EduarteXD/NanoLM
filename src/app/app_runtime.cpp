#include "app_runtime.h"

#include "../settings.h"

namespace AppRuntime {
namespace {

SensorData sSensorData = {0, 0, 0, 0.0f, 25, 200, SENSOR_STATE_PENDING, 0};
MeterConfig sMeterConfig = {
    DEFAULT_ISO,
    DEFAULT_APERTURE,
    DEFAULT_SHUTTER_SEC,
    DEFAULT_EXP_COMP,
    DEFAULT_CALIB_C,
    METER_MODE_APERTURE_PRIORITY,
};

SemaphoreHandle_t sSensorMutex = nullptr;
SemaphoreHandle_t sConfigMutex = nullptr;
TaskHandle_t sSensorSamplingTaskHandle = nullptr;
WebServer sServer(80);
bool sWifiApMode = false;

}  // namespace

bool initialize() {
  if (sSensorMutex == nullptr) {
    sSensorMutex = xSemaphoreCreateMutex();
  }
  if (sConfigMutex == nullptr) {
    sConfigMutex = xSemaphoreCreateMutex();
  }
  return sSensorMutex != nullptr && sConfigMutex != nullptr;
}

SensorData &sensorData() {
  return sSensorData;
}

MeterConfig &meterConfig() {
  return sMeterConfig;
}

WebServer &server() {
  return sServer;
}

SemaphoreHandle_t sensorMutex() {
  return sSensorMutex;
}

SemaphoreHandle_t configMutex() {
  return sConfigMutex;
}

TaskHandle_t sensorSamplingTaskHandle() {
  return sSensorSamplingTaskHandle;
}

void setSensorSamplingTaskHandle(TaskHandle_t handle) {
  sSensorSamplingTaskHandle = handle;
}

bool wifiApMode() {
  return sWifiApMode;
}

void setWifiApMode(bool apMode) {
  sWifiApMode = apMode;
}

}  // namespace AppRuntime
