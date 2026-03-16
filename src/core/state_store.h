#pragma once

#include "../app_types.h"

bool storeSensorData(const SensorData &in);
bool loadSensorData(SensorData &out);
bool loadMeterConfig(MeterConfig &out);
bool saveMeterConfig(const MeterConfig &in);
