#include "meter_component.h"

#include <cmath>

#include "../settings.h"

namespace MeterComponent
{

  static ExposureResult invalidExposureResult()
  {
    ExposureResult result = {false, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
    return result;
  }

  static const float kApertureStops[] = {
      1.0f, 1.1f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.2f, 2.5f, 2.8f,
      3.2f, 3.5f, 4.0f, 4.5f, 5.0f, 5.6f, 6.3f, 7.1f, 8.0f, 9.0f,
      10.0f, 11.0f, 13.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f};

  static const float kShutterStops[] = {
      1.0f / 8000.0f, 1.0f / 6400.0f, 1.0f / 5000.0f, 1.0f / 4000.0f,
      1.0f / 3200.0f, 1.0f / 2500.0f, 1.0f / 2000.0f, 1.0f / 1600.0f,
      1.0f / 1250.0f, 1.0f / 1000.0f, 1.0f / 800.0f, 1.0f / 640.0f,
      1.0f / 500.0f, 1.0f / 400.0f, 1.0f / 320.0f, 1.0f / 250.0f,
      1.0f / 200.0f, 1.0f / 160.0f, 1.0f / 125.0f, 1.0f / 100.0f,
      1.0f / 80.0f, 1.0f / 60.0f, 1.0f / 50.0f, 1.0f / 40.0f,
      1.0f / 30.0f, 1.0f / 25.0f, 1.0f / 20.0f, 1.0f / 15.0f,
      1.0f / 13.0f, 1.0f / 10.0f, 1.0f / 8.0f, 1.0f / 6.0f,
      1.0f / 5.0f, 1.0f / 4.0f, 0.3f, 0.4f,
      0.5f, 0.6f, 0.8f, 1.0f,
      1.3f, 1.6f, 2.0f, 2.5f,
      3.2f, 4.0f, 5.0f, 6.0f,
      8.0f, 10.0f, 13.0f, 15.0f,
      20.0f, 25.0f, 30.0f};

  static bool isFiniteNumber(float value)
  {
    return !std::isnan(value) && !std::isinf(value);
  }

  static float safeLog2(float x)
  {
    if (x <= 0.0f)
    {
      return NAN;
    }
    return logf(x) / logf(2.0f);
  }

  bool parseShutterSeconds(const String &text, float &secondsOut)
  {
    String s = text;
    s.trim();
    if (s.length() == 0)
    {
      return false;
    }

    int slash = s.indexOf('/');
    if (slash > 0)
    {
      const String numStr = s.substring(0, slash);
      const String denStr = s.substring(slash + 1);
      const float num = numStr.toFloat();
      const float den = denStr.toFloat();
      if (num > 0.0f && den > 0.0f)
      {
        secondsOut = num / den;
        return true;
      }
      return false;
    }

    const float sec = s.toFloat();
    if (sec > 0.0f)
    {
      secondsOut = sec;
      return true;
    }
    return false;
  }

  static float nearestStep(const float *steps, size_t count, float value)
  {
    if (count == 0 || !isFiniteNumber(value) || value <= 0.0f)
    {
      return NAN;
    }

    float best = steps[0];
    float bestDiff = fabsf(best - value);
    for (size_t i = 1; i < count; ++i)
    {
      const float diff = fabsf(steps[i] - value);
      if (diff < bestDiff)
      {
        bestDiff = diff;
        best = steps[i];
      }
    }
    return best;
  }

  String formatAperture(float aperture)
  {
    if (!isFiniteNumber(aperture) || aperture <= 0.0f)
    {
      return "--";
    }
    const float rounded = roundf(aperture * 10.0f) / 10.0f;
    return String(rounded, 1);
  }

  String formatShutter(float seconds)
  {
    if (!isFiniteNumber(seconds) || seconds <= 0.0f)
    {
      return "--";
    }
    if (seconds >= 1.0f)
    {
      if (seconds >= 10.0f)
      {
        return String(seconds, 0) + "s";
      }
      return String(seconds, 1) + "s";
    }
    if (seconds >= 0.3f)
    {
      return String(seconds, 2) + "s";
    }
    float denom = roundf(1.0f / seconds);
    if (denom < 1.0f)
    {
      denom = 1.0f;
    }
    return "1/" + String((int)denom);
  }

  static float ev100FromLux(float lux, float calibC)
  {
    if (!isFiniteNumber(lux) || lux <= 0.0f || !isFiniteNumber(calibC) ||
        calibC <= 0.0f)
    {
      return NAN;
    }
    return safeLog2((lux * 100.0f) / calibC);
  }

  static float evAtIso(float ev100, uint16_t iso)
  {
    if (!isFiniteNumber(ev100) || iso == 0)
    {
      return NAN;
    }
    return ev100 + safeLog2(static_cast<float>(iso) / 100.0f);
  }

  static float shutterFromEv(float ev, float aperture)
  {
    if (!isFiniteNumber(ev) || !isFiniteNumber(aperture) || aperture <= 0.0f)
    {
      return NAN;
    }
    return (aperture * aperture) / powf(2.0f, ev);
  }

  static float apertureFromEv(float ev, float shutterSec)
  {
    if (!isFiniteNumber(ev) || !isFiniteNumber(shutterSec) || shutterSec <= 0.0f)
    {
      return NAN;
    }
    const float n2 = shutterSec * powf(2.0f, ev);
    if (n2 <= 0.0f)
    {
      return NAN;
    }
    return sqrtf(n2);
  }

  ExposureResult calculateExposure(float lux, const MeterConfig &cfg)
  {
    ExposureResult r = invalidExposureResult();

    const float ev100 = ev100FromLux(lux, cfg.calibC);
    if (!isFiniteNumber(ev100))
    {
      return r;
    }
    const float sceneEvIso = evAtIso(ev100, cfg.iso);
    if (!isFiniteNumber(sceneEvIso))
    {
      return r;
    }

    const float targetEv = sceneEvIso - cfg.expComp;
    const float sh = shutterFromEv(targetEv, cfg.aperture);
    const float ap = apertureFromEv(targetEv, cfg.shutterSec);

    r.valid = isFiniteNumber(sh) && isFiniteNumber(ap);
    r.ev100 = ev100;
    r.evIso = sceneEvIso;
    r.targetEv = targetEv;
    r.shutterFromAperture = sh;
    r.apertureFromShutter = ap;
    r.shutterSuggested =
        nearestStep(kShutterStops, sizeof(kShutterStops) / sizeof(kShutterStops[0]), sh);
    r.apertureSuggested =
        nearestStep(kApertureStops, sizeof(kApertureStops) / sizeof(kApertureStops[0]), ap);
    return r;
  }

  ExposureResult calculateExposure(const SensorData &sensorData,
                                   const MeterConfig &cfg)
  {
    if (sensorData.state != SENSOR_STATE_OK)
    {
      return invalidExposureResult();
    }
    return calculateExposure(sensorData.lux, cfg);
  }

}
