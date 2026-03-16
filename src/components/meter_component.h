#pragma once

#include <Arduino.h>

#include "../app_types.h"

namespace MeterComponent {

bool parseShutterSeconds(const String &text, float &secondsOut);
String formatAperture(float aperture);
String formatShutter(float seconds);
ExposureResult calculateExposure(float lux, const MeterConfig &cfg);

}  // namespace MeterComponent
