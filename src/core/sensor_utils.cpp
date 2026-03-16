#include "sensor_utils.h"

#include "../settings.h"

uint16_t gainToX(uint8_t gain) {
  switch (gain) {
    case GAIN_LOW_1X:
      return 1;
    case GAIN_MED_25X:
      return 25;
    case GAIN_HIGH_428X:
      return 428;
    case GAIN_MAX_9876X:
      return 9876;
    default:
      return 1;
  }
}

float gainToFloat(uint8_t gain) {
  return static_cast<float>(gainToX(gain));
}

uint16_t integToMs(uint8_t integ) {
  return static_cast<uint16_t>((integ + 1U) * 100U);
}

uint16_t maxCountByInteg(uint8_t integ) {
  return (integ == INT_100MS) ? 36863U : 65535U;
}
