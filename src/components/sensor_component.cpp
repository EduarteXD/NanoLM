#include "sensor_component.h"

#include <Wire.h>

#include "../core/sensor_utils.h"
#include "../settings.h"

namespace SensorComponent
{

  static const RangeProfile kRangeProfiles[] = {
      {GAIN_LOW_1X, INT_100MS},
      {GAIN_LOW_1X, INT_200MS},
      {GAIN_LOW_1X, INT_400MS},
      {GAIN_LOW_1X, INT_600MS},
      {GAIN_MED_25X, INT_100MS},
      {GAIN_MED_25X, INT_200MS},
      {GAIN_MED_25X, INT_400MS},
      {GAIN_MED_25X, INT_600MS},
      {GAIN_HIGH_428X, INT_100MS},
      {GAIN_HIGH_428X, INT_200MS},
      {GAIN_MAX_9876X, INT_100MS},
  };
  static constexpr size_t kRangeProfileCount =
      sizeof(kRangeProfiles) / sizeof(kRangeProfiles[0]);
  static constexpr size_t FAST_RECOVERY_RANGE_INDEX = 0; // 1x/100ms
  static constexpr size_t DEFAULT_RANGE_INDEX = 5;       // 25x/200ms
  static constexpr uint16_t kAutoRangeLowThreshold = 120U;
  static constexpr uint8_t kAutoRangeHighThresholdPercent = 92U;
  static constexpr uint8_t kAutoRangeTargetPercent = 72U;
  static constexpr uint16_t kProfileSwitchSettleExtraMs = 25U;

  static size_t sRangeIndex = DEFAULT_RANGE_INDEX;

  static bool write8(uint8_t reg, uint8_t value)
  {
    Wire.beginTransmission(SystemResources::I2C_ADDR_TSL2591);
    Wire.write(TSL2591_CMD | reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }

  static bool read8(uint8_t reg, uint8_t &value)
  {
    Wire.beginTransmission(SystemResources::I2C_ADDR_TSL2591);
    Wire.write(TSL2591_CMD | reg);
    if (Wire.endTransmission(false) != 0)
    {
      return false;
    }
    if (Wire.requestFrom((int)SystemResources::I2C_ADDR_TSL2591, 1) != 1)
    {
      return false;
    }
    value = Wire.read();
    return true;
  }

  static bool read16(uint8_t regLow, uint16_t &value)
  {
    Wire.beginTransmission(SystemResources::I2C_ADDR_TSL2591);
    Wire.write(TSL2591_CMD | regLow);
    if (Wire.endTransmission(false) != 0)
    {
      return false;
    }
    if (Wire.requestFrom((int)SystemResources::I2C_ADDR_TSL2591, 2) != 2)
    {
      return false;
    }
    const uint8_t low = Wire.read();
    const uint8_t high = Wire.read();
    value = static_cast<uint16_t>(high << 8) | low;
    return true;
  }

  static bool applyProfile(size_t index)
  {
    if (index >= kRangeProfileCount)
    {
      return false;
    }
    const RangeProfile &rp = kRangeProfiles[index];
    const uint8_t control = static_cast<uint8_t>((rp.gain << 4) | rp.integ);
    if (!write8(REG_CONTROL, control))
    {
      return false;
    }
    sRangeIndex = index;
    return true;
  }

  static RangeProfile currentProfile()
  {
    return kRangeProfiles[sRangeIndex];
  }

  static uint16_t peakChannel(uint16_t full, uint16_t ir)
  {
    return (full > ir) ? full : ir;
  }

  static float profileSensitivity(const RangeProfile &profile)
  {
    return gainToFloat(profile.gain) * static_cast<float>(integToMs(profile.integ));
  }

  uint16_t currentIntegrationMs()
  {
    return integToMs(kRangeProfiles[sRangeIndex].integ);
  }

  bool init()
  {
    uint8_t id = 0;
    if (!read8(REG_ID, id))
    {
      Serial.println("TSL2591: 读取 ID 失败");
      return false;
    }
    if (id != 0x50)
    {
      Serial.print("TSL2591: ID 异常 0x");
      Serial.println(id, HEX);
      return false;
    }

    if (!applyProfile(DEFAULT_RANGE_INDEX))
    {
      Serial.println("TSL2591: 设置初始量程失败");
      return false;
    }

    if (!write8(REG_ENABLE, ENABLE_PON | ENABLE_AEN))
    {
      Serial.println("TSL2591: ENABLE 写入失败");
      return false;
    }

    const RangeProfile rp = currentProfile();
    Serial.print("初始量程: ");
    Serial.print(gainToX(rp.gain));
    Serial.print("x/");
    Serial.print(integToMs(rp.integ));
    Serial.println("ms");
    return true;
  }

  static bool readChannels(uint16_t &ch0, uint16_t &ch1)
  {
    uint8_t status = 0;
    if (!read8(REG_STATUS, status))
    {
      return false;
    }

    if ((status & 0x01) == 0)
    {
      return false;
    }

    if (!read16(REG_C0DATAL, ch0))
    {
      return false;
    }
    if (!read16(REG_C1DATAL, ch1))
    {
      return false;
    }
    return true;
  }

  static float calculateLux(uint16_t ch0, uint16_t ch1, uint8_t gain,
                            uint8_t integ)
  {
    const float cpl = (integToMs(integ) * gainToFloat(gain)) / LUX_DF;
    if (cpl <= 0.0f)
    {
      return -1.0f;
    }

    const uint16_t maxCount = maxCountByInteg(integ);
    if (ch0 >= maxCount || ch1 >= maxCount)
    {
      return -2.0f;
    }

    const float lux1 = (static_cast<float>(ch0) - (LUX_COEFB * ch1)) / cpl;
    const float lux2 =
        ((LUX_COEFC * static_cast<float>(ch0)) - (LUX_COEFD * ch1)) / cpl;

    float lux = (lux1 > lux2) ? lux1 : lux2;
    if (lux < 0.0f)
    {
      lux = 0.0f;
    }
    return lux;
  }

  static void assignSample(SensorData &out, uint16_t full, uint16_t ir,
                           const RangeProfile &profile, float rawLux)
  {
    out.full = full;
    out.ir = ir;
    out.visible = (full > ir) ? (full - ir) : 0;
    out.gainX = gainToX(profile.gain);
    out.integrationMs = integToMs(profile.integ);
    out.sampleMs = millis();
    out.lux = rawLux;

    if (rawLux == -2.0f)
    {
      out.state = SENSOR_STATE_SATURATED;
      out.lux = 0.0f;
    }
    else if (rawLux < 0.0f)
    {
      out.state = SENSOR_STATE_ERROR;
      out.lux = 0.0f;
    }
    else
    {
      out.state = SENSOR_STATE_OK;
    }
  }

  static bool readChannelsWithTimeout(uint16_t &ch0, uint16_t &ch1,
                                      uint16_t timeoutMs)
  {
    const uint32_t startMs = millis();
    while ((millis() - startMs) <= timeoutMs)
    {
      if (readChannels(ch0, ch1))
      {
        return true;
      }
      delay(5);
    }
    return false;
  }

  static bool sampleAfterProfileSwitch(size_t index, SensorData &out)
  {
    if (!applyProfile(index))
    {
      return false;
    }

    uint16_t full = 0;
    uint16_t ir = 0;
    const RangeProfile profile = kRangeProfiles[index];
    const uint16_t waitMs =
        integToMs(profile.integ) + kProfileSwitchSettleExtraMs;
    if (!readChannelsWithTimeout(full, ir, waitMs))
    {
      return false;
    }

    const float rawLux = calculateLux(full, ir, profile.gain, profile.integ);
    assignSample(out, full, ir, profile, rawLux);
    return true;
  }

  static bool sampleTooBright(uint16_t full, uint16_t ir, float lux,
                              const RangeProfile &sampleProfile)
  {
    const uint16_t maxCount = maxCountByInteg(sampleProfile.integ);
    const uint16_t highThreshold = static_cast<uint16_t>(
        maxCount * kAutoRangeHighThresholdPercent / 100U);
    return (lux == -2.0f) || (full >= highThreshold) || (ir >= highThreshold);
  }

  static bool sampleTooDark(uint16_t full, uint16_t ir, float lux)
  {
    return (lux >= 0.0f) && (full < kAutoRangeLowThreshold) &&
           (ir < kAutoRangeLowThreshold);
  }

  static size_t estimateRangeIndex(uint16_t full, uint16_t ir,
                                   const RangeProfile &sampleProfile)
  {
    const uint16_t peak = peakChannel(full, ir);
    if (peak == 0U)
    {
      return kRangeProfileCount - 1;
    }

    const float sampleSensitivity = profileSensitivity(sampleProfile);
    if (sampleSensitivity <= 0.0f)
    {
      return sRangeIndex;
    }

    size_t targetIndex = FAST_RECOVERY_RANGE_INDEX;
    for (size_t i = 0; i < kRangeProfileCount; ++i)
    {
      const RangeProfile candidate = kRangeProfiles[i];
      const float predictedPeak =
          static_cast<float>(peak) * profileSensitivity(candidate) /
          sampleSensitivity;
      const float targetPeak =
          static_cast<float>(maxCountByInteg(candidate.integ)) *
          static_cast<float>(kAutoRangeTargetPercent) / 100.0f;
      if (predictedPeak <= targetPeak)
      {
        targetIndex = i;
        continue;
      }
      break;
    }

    return targetIndex;
  }

  static void logRangeSwitch(const char *reason, const RangeProfile &profile)
  {
    Serial.print(reason);
    Serial.print(" -> ");
    Serial.print(gainToX(profile.gain));
    Serial.print("x/");
    Serial.print(integToMs(profile.integ));
    Serial.println("ms");
  }

  static void autoRangeIfNeeded(uint16_t full, uint16_t ir, float lux,
                                const RangeProfile &sampleProfile)
  {
    if (!AUTO_RANGE_ENABLED)
    {
      return;
    }

    const bool tooBright = sampleTooBright(full, ir, lux, sampleProfile);
    const bool tooDark = sampleTooDark(full, ir, lux);
    if (!tooBright && !tooDark)
    {
      return;
    }

    size_t targetIndex = sRangeIndex;
    if (tooBright && (lux == -2.0f))
    {
      targetIndex = FAST_RECOVERY_RANGE_INDEX;
    }
    else
    {
      targetIndex = estimateRangeIndex(full, ir, sampleProfile);
    }

    if (targetIndex == sRangeIndex)
    {
      return;
    }
    if (!applyProfile(targetIndex))
    {
      return;
    }

    logRangeSwitch("自动量程切换", currentProfile());
  }

  static void recoverFromSaturation(SensorData &out)
  {
    if (!AUTO_RANGE_ENABLED || sRangeIndex == FAST_RECOVERY_RANGE_INDEX)
    {
      return;
    }

    Serial.println("自动量程过曝，切到 1x/100ms 快速补测");

    SensorData recovery = {0, 0, 0, 0.0f, 1, 100, SENSOR_STATE_ERROR, millis()};
    if (!sampleAfterProfileSwitch(FAST_RECOVERY_RANGE_INDEX, recovery))
    {
      Serial.println("自动量程快速补测失败");
      return;
    }

    out = recovery;
    if (recovery.state != SENSOR_STATE_OK)
    {
      return;
    }

    const RangeProfile recoveryProfile = kRangeProfiles[FAST_RECOVERY_RANGE_INDEX];
    const size_t targetIndex =
        estimateRangeIndex(recovery.full, recovery.ir, recoveryProfile);
    if (targetIndex == sRangeIndex)
    {
      return;
    }
    if (!applyProfile(targetIndex))
    {
      return;
    }
    logRangeSwitch("自动量程快速补测后预切换", currentProfile());
  }

  SensorData sampleOnce()
  {
    SensorData out = {0, 0, 0, 0.0f, 1, 100, SENSOR_STATE_ERROR, millis()};
    const RangeProfile profile = currentProfile();

    uint16_t full = 0;
    uint16_t ir = 0;
    if (!readChannels(full, ir))
    {
      out.state = SENSOR_STATE_ERROR;
      return out;
    }

    const float rawLux = calculateLux(full, ir, profile.gain, profile.integ);
    assignSample(out, full, ir, profile, rawLux);

    if (rawLux == -2.0f)
    {
      recoverFromSaturation(out);
      return out;
    }

    autoRangeIfNeeded(full, ir, rawLux, profile);
    return out;
  }

  uint16_t recommendedDelayMs()
  {
    uint16_t delayMs = currentIntegrationMs() + 80U;
    if (delayMs < 500U)
    {
      delayMs = 500U;
    }
    return delayMs;
  }

}
