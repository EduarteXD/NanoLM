#include "ble_protocol.h"

#include <cmath>
#include <cstring>
#include <limits>

#include "../core/math_utils.h"
#include "../settings.h"

namespace BleProtocol {
namespace {

constexpr uint8_t kMeterFlagValid = 0x01;

void writeU16LE(uint8_t *buffer, size_t offset, uint16_t value) {
  buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
  buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void writeI16LE(uint8_t *buffer, size_t offset, int16_t value) {
  writeU16LE(buffer, offset, static_cast<uint16_t>(value));
}

void writeU32LE(uint8_t *buffer, size_t offset, uint32_t value) {
  buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
  buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
  buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
  buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

uint16_t readU16LE(const uint8_t *buffer, size_t offset) {
  return static_cast<uint16_t>(buffer[offset]) |
         (static_cast<uint16_t>(buffer[offset + 1]) << 8);
}

int16_t readI16LE(const uint8_t *buffer, size_t offset) {
  return static_cast<int16_t>(readU16LE(buffer, offset));
}

uint32_t readU32LE(const uint8_t *buffer, size_t offset) {
  return static_cast<uint32_t>(buffer[offset]) |
         (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
         (static_cast<uint32_t>(buffer[offset + 2]) << 16) |
         (static_cast<uint32_t>(buffer[offset + 3]) << 24);
}

uint16_t apertureToTenths(float aperture) {
  if (!isFiniteNumber(aperture) || aperture <= 0.0f) {
    return 0;
  }

  const float clamped = clampf(aperture, 0.7f, 32.0f);
  return static_cast<uint16_t>(lroundf(clamped * 10.0f));
}

float tenthsToAperture(uint16_t tenths) {
  return static_cast<float>(tenths) / 10.0f;
}

int16_t expCompToTenths(float expComp) {
  const float clamped = clampf(expComp, -5.0f, 5.0f);
  return static_cast<int16_t>(lroundf(clamped * 10.0f));
}

float tenthsToExpComp(int16_t tenths) {
  return static_cast<float>(tenths) / 10.0f;
}

uint32_t secondsToMicros(float seconds) {
  if (!isFiniteNumber(seconds) || seconds <= 0.0f) {
    return 0;
  }

  const float clamped = clampf(seconds, 1.0f / 16000.0f, 120.0f);
  return static_cast<uint32_t>(lroundf(clamped * 1000000.0f));
}

float microsToSeconds(uint32_t micros) {
  return static_cast<float>(micros) / 1000000.0f;
}

uint32_t luxToMilli(float lux) {
  if (!isFiniteNumber(lux) || lux <= 0.0f) {
    return 0;
  }

  const float scaled = lux * 1000.0f;
  const float maxValue =
      static_cast<float>(std::numeric_limits<uint32_t>::max());
  if (scaled >= maxValue) {
    return std::numeric_limits<uint32_t>::max();
  }
  return static_cast<uint32_t>(lroundf(scaled));
}

int16_t evToCenti(float value) {
  if (!isFiniteNumber(value)) {
    return 0;
  }

  float scaled = value * 100.0f;
  if (scaled > 32767.0f) {
    scaled = 32767.0f;
  } else if (scaled < -32768.0f) {
    scaled = -32768.0f;
  }
  return static_cast<int16_t>(lroundf(scaled));
}

}  // namespace

const char kServiceUuid[] = "7f29e500-3ad2-4f7e-9d58-9b66b528c000";
const char kDeviceInfoCharacteristicUuid[] =
    "7f29e500-3ad2-4f7e-9d58-9b66b528c001";
const char kSensorCharacteristicUuid[] =
    "7f29e500-3ad2-4f7e-9d58-9b66b528c002";
const char kMeterCharacteristicUuid[] =
    "7f29e500-3ad2-4f7e-9d58-9b66b528c003";
const char kConfigCharacteristicUuid[] =
    "7f29e500-3ad2-4f7e-9d58-9b66b528c004";

bool encodeSensorPacket(const SensorData &sensorData, uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size != kSensorPacketSize) {
    return false;
  }

  memset(buffer, 0, kSensorPacketSize);
  buffer[0] = BLE_PROTOCOL_VERSION;
  buffer[1] = sensorData.state;
  writeU16LE(buffer, 2, sensorData.gainX);
  writeU16LE(buffer, 4, sensorData.integrationMs);
  writeU16LE(buffer, 6, sensorData.full);
  writeU16LE(buffer, 8, sensorData.ir);
  writeU16LE(buffer, 10, sensorData.visible);
  writeU32LE(buffer, 12, luxToMilli(sensorData.lux));
  writeU32LE(buffer, 16, sensorData.sampleMs);
  return true;
}

bool encodeMeterPacket(const MeterConfig &config, const ExposureResult &exposure,
                       uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size != kMeterPacketSize) {
    return false;
  }

  memset(buffer, 0, kMeterPacketSize);
  buffer[0] = BLE_PROTOCOL_VERSION;
  buffer[1] = exposure.valid ? kMeterFlagValid : 0;
  buffer[2] = config.mode;
  buffer[3] = 0;
  writeI16LE(buffer, 4, evToCenti(exposure.ev100));
  writeI16LE(buffer, 6, evToCenti(exposure.evIso));
  writeI16LE(buffer, 8, evToCenti(exposure.targetEv));
  writeU32LE(buffer, 10, secondsToMicros(exposure.shutterFromAperture));
  writeU32LE(buffer, 14, secondsToMicros(exposure.shutterSuggested));
  writeU16LE(buffer, 18, apertureToTenths(exposure.apertureSuggested));
  return true;
}

bool encodeConfigPacket(const MeterConfig &config, uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size != kConfigPacketSize) {
    return false;
  }

  memset(buffer, 0, kConfigPacketSize);
  buffer[0] = BLE_PROTOCOL_VERSION;
  buffer[1] = config.mode;
  writeU16LE(buffer, 2, config.iso);
  writeU16LE(buffer, 4, apertureToTenths(config.aperture));
  writeU32LE(buffer, 6, secondsToMicros(config.shutterSec));
  writeI16LE(buffer, 10, expCompToTenths(config.expComp));
  writeU16LE(buffer, 12, static_cast<uint16_t>(lroundf(config.calibC)));
  return true;
}

bool decodeConfigPacket(const uint8_t *buffer, size_t size, MeterConfig &out) {
  if (buffer == nullptr || size != kConfigPacketSize) {
    return false;
  }
  if (buffer[0] != BLE_PROTOCOL_VERSION) {
    return false;
  }

  const uint8_t mode = buffer[1];
  const uint16_t iso = readU16LE(buffer, 2);
  const uint16_t apertureTenths = readU16LE(buffer, 4);
  const uint32_t shutterMicros = readU32LE(buffer, 6);
  const int16_t expCompTenths = readI16LE(buffer, 10);
  const uint16_t calibC = readU16LE(buffer, 12);

  if (mode != METER_MODE_APERTURE_PRIORITY &&
      mode != METER_MODE_SHUTTER_PRIORITY) {
    return false;
  }
  if (iso < 25 || iso > 25600) {
    return false;
  }
  if (apertureTenths < 7 || apertureTenths > 320) {
    return false;
  }
  if (shutterMicros < 63 || shutterMicros > 120000000UL) {
    return false;
  }
  if (expCompTenths < -50 || expCompTenths > 50) {
    return false;
  }
  if (calibC < 100 || calibC > 500) {
    return false;
  }

  out.mode = mode;
  out.iso = iso;
  out.aperture = tenthsToAperture(apertureTenths);
  out.shutterSec = microsToSeconds(shutterMicros);
  out.expComp = tenthsToExpComp(expCompTenths);
  out.calibC = static_cast<float>(calibC);
  return true;
}

std::string buildDeviceInfoValue(const std::string &deviceName,
                                 const std::string &deviceId) {
  std::string value = "name=";
  value += deviceName;
  value += ";device_id=";
  value += deviceId;
  value += ";proto=";
  value += std::to_string(BLE_PROTOCOL_VERSION);
  value += ";service=";
  value += kServiceUuid;
  value += ";sensor=20;meter=20;config=14";
  return value;
}

}  // namespace BleProtocol
