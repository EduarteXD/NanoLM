#pragma once

#include <Arduino.h>

namespace EpdComponent {

bool initPcfI2cBus();
bool probeAddress(uint8_t addr);
bool runBootSelfTest();

}  // namespace EpdComponent
