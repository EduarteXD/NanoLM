#pragma once

#include <Arduino.h>

struct RangeProfile {
  uint8_t gain;
  uint8_t integ;
};

struct SensorData {
  uint16_t full;
  uint16_t ir;
  uint16_t visible;
  float lux;
  uint16_t gainX;
  uint16_t integrationMs;
  uint8_t state;
  uint32_t sampleMs;
};

struct MeterConfig {
  uint16_t iso;
  float aperture;
  float shutterSec;
  float expComp;
  float calibC;
  uint8_t mode;
};

struct ExposureResult {
  bool valid;
  float ev100;
  float evIso;
  float targetEv;
  float shutterFromAperture;
  float apertureFromShutter;
  float shutterSuggested;
  float apertureSuggested;
};
