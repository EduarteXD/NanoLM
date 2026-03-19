#include "wifi_component.h"

#include <WiFi.h>

#include "../app/app_runtime.h"
#include "../settings.h"
#include "led_status_component.h"

bool connectWifi()
{
  LedStatusComponent::setStage(LedStatusComponent::STAGE_WIFI_CONNECTING);
  LedStatusComponent::setNetworkState(false);
  LedStatusComponent::update();

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS))
  {
    Serial.println("AP 启动失败");
    AppRuntime::setWifiApMode(false);
    LedStatusComponent::setNetworkState(false);
    LedStatusComponent::setSensorState(SENSOR_STATE_ERROR);
    LedStatusComponent::setStage(LedStatusComponent::STAGE_RUNNING);
    LedStatusComponent::update();
    return false;
  }

  AppRuntime::setWifiApMode(true);
  LedStatusComponent::setNetworkState(true);
  LedStatusComponent::setStage(LedStatusComponent::STAGE_RUNNING);
  LedStatusComponent::update();
  Serial.print("AP 已启动，SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  return true;
}
