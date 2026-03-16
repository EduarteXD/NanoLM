#pragma once

#include <Arduino.h>

uint16_t gainToX(uint8_t gain);
float gainToFloat(uint8_t gain);
uint16_t integToMs(uint8_t integ);
uint16_t maxCountByInteg(uint8_t integ);
