#include "web_component.h"

#include <WiFi.h>

#include "../app/app_runtime.h"
#include "../core/math_utils.h"
#include "../web_ui_zh.h"
#include "../core/state_store.h"
#include "../settings.h"
#include "meter_component.h"

namespace WebComponent
{

  static bool sRoutesBound = false;

  static String currentIpString()
  {
    return WiFi.softAPIP().toString();
  }

  static const char *sensorStateText(uint8_t state)
  {
    if (state == SENSOR_STATE_OK)
    {
      return "ok";
    }
    if (state == SENSOR_STATE_SATURATED)
    {
      return "saturated";
    }
    if (state == SENSOR_STATE_ERROR)
    {
      return "error";
    }
    return "pending";
  }

  static const char *meterModeText(uint8_t mode)
  {
    if (mode == METER_MODE_SHUTTER_PRIORITY)
    {
      return "shutter";
    }
    return "aperture";
  }

  static void addJsonFloat(String &json, const char *key, float value, int digits)
  {
    json += "\"";
    json += key;
    json += "\":";
    if (isFiniteNumber(value))
    {
      json += String(value, digits);
    }
    else
    {
      json += "null";
    }
  }

  static String configToJson(const MeterConfig &cfg)
  {
    String json;
    json.reserve(180);
    json += "{";
    json += "\"mode\":\"";
    json += meterModeText(cfg.mode);
    json += "\",\"iso\":";
    json += cfg.iso;
    json += ",\"aperture\":";
    json += String(cfg.aperture, 1);
    json += ",\"shutter_s\":";
    json += String(cfg.shutterSec, 6);
    json += ",\"shutter_text\":\"";
    json += MeterComponent::formatShutter(cfg.shutterSec);
    json += "\",\"exp_comp\":";
    json += String(cfg.expComp, 1);
    json += ",\"calib_c\":";
    json += String(cfg.calibC, 0);
    json += "}";
    return json;
  }

  static void handleRoot()
  {
    AppRuntime::server().sendHeader("Cache-Control", "no-store");
    AppRuntime::server().send_P(200, "text/html; charset=utf-8", WEB_UI_ZH);
  }

  static void handleApiConfig()
  {
    MeterConfig cfg;
    if (!loadMeterConfig(cfg))
    {
      AppRuntime::server().send(503, "application/json",
                                "{\"error\":\"config unavailable\"}");
      return;
    }

    bool changed = false;

    if (AppRuntime::server().hasArg("mode"))
    {
      String modeArg = AppRuntime::server().arg("mode");
      modeArg.toLowerCase();
      if (modeArg == "aperture")
      {
        cfg.mode = METER_MODE_APERTURE_PRIORITY;
        changed = true;
      }
      else if (modeArg == "shutter")
      {
        cfg.mode = METER_MODE_SHUTTER_PRIORITY;
        changed = true;
      }
    }

    if (AppRuntime::server().hasArg("iso"))
    {
      const long iso = AppRuntime::server().arg("iso").toInt();
      if (iso >= 25 && iso <= 25600)
      {
        cfg.iso = static_cast<uint16_t>(iso);
        changed = true;
      }
    }

    if (AppRuntime::server().hasArg("aperture"))
    {
      const float ap = AppRuntime::server().arg("aperture").toFloat();
      if (ap >= 0.7f && ap <= 32.0f)
      {
        cfg.aperture = ap;
        changed = true;
      }
    }

    if (AppRuntime::server().hasArg("shutter"))
    {
      float sec = 0.0f;
      if (MeterComponent::parseShutterSeconds(AppRuntime::server().arg("shutter"),
                                              sec))
      {
        cfg.shutterSec = clampf(sec, 1.0f / 16000.0f, 120.0f);
        changed = true;
      }
    }

    if (AppRuntime::server().hasArg("exp_comp"))
    {
      cfg.expComp =
          clampf(AppRuntime::server().arg("exp_comp").toFloat(), -5.0f, 5.0f);
      changed = true;
    }

    if (AppRuntime::server().hasArg("calib_c"))
    {
      const float c = AppRuntime::server().arg("calib_c").toFloat();
      if (c >= 100.0f && c <= 500.0f)
      {
        cfg.calibC = c;
        changed = true;
      }
    }

    if (changed)
    {
      saveMeterConfig(cfg);
    }

    AppRuntime::server().sendHeader("Cache-Control", "no-store");
    AppRuntime::server().send(200, "application/json", configToJson(cfg));
  }

  static void handleApiData()
  {
    SensorData sd;
    MeterConfig cfg;
    if (!loadSensorData(sd) || !loadMeterConfig(cfg))
    {
      AppRuntime::server().send(503, "application/json",
                                "{\"error\":\"data unavailable\"}");
      return;
    }

    const ExposureResult ex = MeterComponent::calculateExposure(sd, cfg);
    const char *wifiMode = AppRuntime::wifiApMode() ? "AP" : "DOWN";

    String json;
    json.reserve(760);
    json += "{";
    json += "\"full\":";
    json += sd.full;
    json += ",\"ir\":";
    json += sd.ir;
    json += ",\"visible\":";
    json += sd.visible;
    json += ",\"lux\":";
    if (sd.state == SENSOR_STATE_OK)
    {
      json += String(sd.lux, 2);
    }
    else
    {
      json += "null";
    }
    json += ",\"sensor_state\":\"";
    json += sensorStateText(sd.state);
    json += "\",\"gain_x\":";
    json += sd.gainX;
    json += ",\"integration_ms\":";
    json += sd.integrationMs;
    json += ",\"auto_range\":";
    json += (AUTO_RANGE_ENABLED ? "true" : "false");
    json += ",\"sample_age_ms\":";
    json += (millis() - sd.sampleMs);
    json += ",\"wifi_mode\":\"";
    json += wifiMode;
    json += "\",\"wifi_target_mode\":\"";
    json += "AP_ONLY";
    json += "\",\"ip\":\"";
    json += currentIpString();
    json += "\",\"mode\":\"";
    json += meterModeText(cfg.mode);
    json += "\",\"iso\":";
    json += cfg.iso;
    json += ",\"aperture\":";
    json += String(cfg.aperture, 1);
    json += ",\"aperture_set\":";
    json += String(cfg.aperture, 1);
    json += ",\"shutter_s\":";
    json += String(cfg.shutterSec, 6);
    json += ",\"shutter_set_s\":";
    json += String(cfg.shutterSec, 6);
    json += ",\"shutter_text\":\"";
    json += MeterComponent::formatShutter(cfg.shutterSec);
    json += "\"";
    json += ",\"shutter_set_text\":\"";
    json += MeterComponent::formatShutter(cfg.shutterSec);
    json += "\"";
    json += ",\"exp_comp\":";
    json += String(cfg.expComp, 1);
    json += ",\"calib_c\":";
    json += String(cfg.calibC, 0);
    json += ",";
    addJsonFloat(json, "ev100", ex.ev100, 2);
    json += ",";
    addJsonFloat(json, "ev_iso", ex.evIso, 2);
    json += ",";
    addJsonFloat(json, "target_ev", ex.targetEv, 2);
    json += ",";
    addJsonFloat(json, "shutter_calc_s", ex.shutterFromAperture, 6);
    json += ",";
    addJsonFloat(json, "aperture_calc", ex.apertureFromShutter, 2);
    json += ",";
    addJsonFloat(json, "shutter_suggest_s", ex.shutterSuggested, 6);
    json += ",\"shutter_suggest_text\":\"";
    json += MeterComponent::formatShutter(ex.shutterSuggested);
    json += "\",";
    addJsonFloat(json, "aperture_suggest", ex.apertureSuggested, 1);
    json += ",\"aperture_suggest_text\":\"";
    json += MeterComponent::formatAperture(ex.apertureSuggested);
    json += "\"}";

    AppRuntime::server().sendHeader("Cache-Control", "no-store");
    AppRuntime::server().send(200, "application/json", json);
  }

  static void handleNotFound()
  {
    AppRuntime::server().send(404, "text/plain", "Not Found");
  }

  void beginServer()
  {
    if (!sRoutesBound)
    {
      AppRuntime::server().on("/", HTTP_GET, handleRoot);
      AppRuntime::server().on("/api/data", HTTP_GET, handleApiData);
      AppRuntime::server().on("/api/config", HTTP_GET, handleApiConfig);
      AppRuntime::server().on("/api/config", HTTP_POST, handleApiConfig);
      AppRuntime::server().onNotFound(handleNotFound);
      sRoutesBound = true;
    }
    AppRuntime::server().begin();
    Serial.println("HTTP 服务已启动");
  }

}
