#include "math_utils.h"

#include <Arduino.h>

bool isFiniteNumber(float value) {
  return !isnan(value) && !isinf(value);
}

float safeLog2(float x) {
  if (x <= 0.0f) {
    return NAN;
  }
  return logf(x) / logf(2.0f);
}

float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}
