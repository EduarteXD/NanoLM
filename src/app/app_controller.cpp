#include "app_controller.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../system_resources.h"
#include "../core/state_store.h"
#include "../core/system_diagnostics.h"
#include "../settings.h"
// #include "../components/epd_component.h"
#include "../components/ble_component.h"
#include "../components/led_status_component.h"
#include "../components/sensor_component.h"
#include "../tasks/sensor_sampling_task.h"
#include "../components/web_component.h"
#include "../components/wifi_component.h"
#include "app_runtime.h"

namespace AppController
{
  namespace
  {

    SensorData makeSensorErrorData()
    {
      // {
      //  uint16_t full;
      //  uint16_t ir;
      //  uint16_t visible;
      //  float lux;
      //  uint16_t gainX;
      //  uint16_t integrationMs;
      //  uint8_t state;
      //  uint32_t sampleMs;
      // }
      SensorData data = {0, 0, 0, 0.0f, 1, 100, SENSOR_STATE_ERROR, millis()};
      return data;
    }

    void beginStatusLed()
    {
      LedStatusComponent::begin();
      LedStatusComponent::setStage(LedStatusComponent::STAGE_BOOTING);
      LedStatusComponent::setSensorState(SENSOR_STATE_PENDING);
      LedStatusComponent::update();
    }

    bool beginRuntime()
    {
      if (AppRuntime::initialize())
      {
        return true;
      }

      Serial.println("互斥锁创建失败");
      LedStatusComponent::setSensorState(SENSOR_STATE_ERROR);
      LedStatusComponent::update();
      return false;
    }

    void beginSensorPipeline()
    {
      if (!SensorComponent::init())
      {
        Serial.println("TSL2591 初始化失败");
        storeSensorData(makeSensorErrorData());
        LedStatusComponent::setSensorState(SENSOR_STATE_ERROR);
        return;
      }

      Serial.println("TSL2591 初始化成功");
      LedStatusComponent::setSensorState(SENSOR_STATE_PENDING);

      TaskHandle_t handle = nullptr;
      const BaseType_t ok =
          xTaskCreate(sensorSamplingTask, "sensorSamplingTask", 4096, nullptr, 1,
                      &handle);
      if (ok != pdPASS)
      {
        Serial.println("创建采样任务失败");
        storeSensorData(makeSensorErrorData());
        LedStatusComponent::setSensorState(SENSOR_STATE_ERROR);
        return;
      }

      AppRuntime::setSensorSamplingTaskHandle(handle);
    }

    void beginNetworkPipeline()
    {
      if (!connectWifi())
      {
        Serial.println("WiFi 初始化失败，网页可能不可达");
      }

      WebComponent::beginServer();
      LedStatusComponent::update();

      Serial.print("浏览器打开: http://");
      Serial.println(WiFi.softAPIP());
    }

    void beginBlePipeline()
    {
      if (!ENABLE_BLE)
      {
        Serial.println("BLE 已禁用");
        return;
      }

      if (!BleComponent::begin())
      {
        Serial.println("BLE 初始化失败，客户端暂不可用");
        return;
      }

      Serial.println("BLE 已启动，等待客户端连接");
    }

  } // namespace

  void setup()
  {
    Serial.begin(SystemResources::SERIAL_BAUD);
    delay(2000);
    Serial.println();
    Serial.println("Startup...");

    printIoTable();
    beginStatusLed();

    if (!beginRuntime())
    {
      return;
    }

    Wire.begin(SystemResources::PIN_I2C_SDA, SystemResources::PIN_I2C_SCL);
    scanI2cBuses();

    // if (ENABLE_EPD_SELF_TEST_ON_BOOT)
    // {
    //   if (EpdComponent::runBootSelfTest())
    //   {
    //     Serial.println("EPD 自检: 显示完成");
    //   }
    //   else
    //   {
    //     Serial.println("EPD 自检: 失败(不影响测光表功能)");
    //   }
    // }

    beginSensorPipeline();
    beginNetworkPipeline();
    beginBlePipeline();
  }

  void loop()
  {
    AppRuntime::server().handleClient();
    BleComponent::poll();
    LedStatusComponent::setNetworkState(AppRuntime::wifiApMode());
    LedStatusComponent::update();
    vTaskDelay(pdMS_TO_TICKS(2));
  }

}
