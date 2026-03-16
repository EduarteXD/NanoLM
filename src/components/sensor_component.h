#pragma once

#include <Arduino.h>

#include "../app_types.h"

namespace SensorComponent {

bool init();
SensorData sampleOnce();
uint16_t currentIntegrationMs();
uint16_t recommendedDelayMs();

}  // namespace SensorComponent
