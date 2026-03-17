#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "../app_types.h"

namespace BleProtocol {

extern const char kServiceUuid[];
extern const char kDeviceInfoCharacteristicUuid[];
extern const char kSensorCharacteristicUuid[];
extern const char kMeterCharacteristicUuid[];
extern const char kConfigCharacteristicUuid[];

static constexpr size_t kSensorPacketSize = 20;
static constexpr size_t kMeterPacketSize = 20;
static constexpr size_t kConfigPacketSize = 14;

bool encodeSensorPacket(const SensorData &sensorData, uint8_t *buffer, size_t size);
bool encodeMeterPacket(const MeterConfig &config, const ExposureResult &exposure,
                       uint8_t *buffer, size_t size);
bool encodeConfigPacket(const MeterConfig &config, uint8_t *buffer, size_t size);
bool decodeConfigPacket(const uint8_t *buffer, size_t size, MeterConfig &out);
std::string buildDeviceInfoValue(const std::string &deviceName,
                                 const std::string &deviceId);

}  // namespace BleProtocol
