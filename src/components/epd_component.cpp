#include "epd_component.h"

#include <ESP32_SoftWire.h>
#include <SPI.h>
#include <cstring>

#include "../boot_image_data.h"
#include "../settings.h"

namespace EpdComponent
{

  static constexpr uint8_t EPD_SPI_SCLK_PIN = SystemResources::PIN_EPD_SPI_SCLK;
  static constexpr uint8_t EPD_SPI_MOSI_PIN = SystemResources::PIN_EPD_SPI_MOSI;
  static constexpr uint8_t EPD_SPI_CS_PIN = SystemResources::PIN_EPD_SPI_CS;
  static constexpr uint8_t EPD_I2C_SDA_PIN = SystemResources::PIN_EPD_I2C_SDA;
  static constexpr uint8_t EPD_I2C_SCL_PIN = SystemResources::PIN_EPD_I2C_SCL;
  static constexpr uint8_t EPD_PCF8574_ADDR = SystemResources::I2C_ADDR_EPD_PCF8574;
  static constexpr uint32_t EPD_I2C_FREQ_HZ = 50000U;
  static constexpr uint32_t EPD_SPI_FREQ_HZ = 2000000U;

  static constexpr uint16_t EPD_PCF_SETTLE_US = 80U;
  static constexpr uint16_t EPD_DC_SETUP_US = 20U;
  static constexpr uint16_t EPD_CS_SETUP_US = 2U;
  static constexpr uint16_t EPD_CS_HOLD_US = 2U;
  static constexpr uint16_t EPD_CS_IDLE_US = 8U;
  static constexpr uint32_t EPD_BUSY_TIMEOUT_RESET_MS = 25000U;
  static constexpr uint32_t EPD_BUSY_TIMEOUT_UPDATE_MS = 35000U;
  static constexpr uint32_t EPD_BUSY_TIMEOUT_RESET_ATTEMPT_MS = 2000U;
  static constexpr uint8_t EPD_BUSY_STABLE_SAMPLES = 2U;
  static constexpr uint16_t EPD_BUSY_POLL_INTERVAL_MS = 3U;
  static constexpr uint16_t EPD_POWER_STABILIZE_MS = 20U;
  static constexpr bool EPD_BUSY_ACTIVE_HIGH = true;
  static constexpr uint16_t EPD_DC_DEBUG_PULSE_MS = 2U;
  static constexpr bool EPD_DIAG_PULSE_ON_BOOT = false;
  static constexpr uint16_t EPD_DIAG_PULSE_HOLD_MS = 80U;
  static constexpr uint8_t EPD_DIAG_PULSE_REPEAT = 3U;

  static constexpr uint16_t EPD_WIDTH = 400;
  static constexpr uint16_t EPD_HEIGHT = 300;
  static constexpr size_t EPD_ARRAY = (EPD_WIDTH * EPD_HEIGHT) / 8;

  enum PcfPin : uint8_t
  {
    PCF_PIN_DC = 0,
    PCF_PIN_RES = 1,
    PCF_PIN_BUSY = 2,
  };

  static SPISettings sSpiSettings(EPD_SPI_FREQ_HZ, MSBFIRST, SPI_MODE0);
  static uint8_t sPcfShadow = 0xFF; // PCF8574 高电平=释放/输入
  static bool sInitialized = false;
  static bool sPcfBusReady = false;
  static uint8_t sFrame[EPD_ARRAY];
  static SoftWire sPcfI2c;

  struct ResetProfile
  {
    uint16_t lowMs;
    uint16_t highMs;
  };

  static constexpr ResetProfile kResetProfiles[] = {
      {12U, 12U},
      {50U, 50U},
      {120U, 120U},
  };

  static void printHexByte(uint8_t value)
  {
    if (value < 16)
    {
      Serial.print('0');
    }
    Serial.print(value, HEX);
  }

  bool initPcfI2cBus()
  {
    if (sPcfBusReady)
    {
      return true;
    }
    sPcfI2c.begin(EPD_I2C_SDA_PIN, EPD_I2C_SCL_PIN, EPD_I2C_FREQ_HZ);
    sPcfBusReady = true;
    return true;
  }

  bool probeAddress(uint8_t addr)
  {
    if (!initPcfI2cBus())
    {
      return false;
    }
    sPcfI2c.beginTransmission(addr);
    return sPcfI2c.endTransmission() == 0;
  }

  static bool pcfWrite(uint8_t value)
  {
    if (!initPcfI2cBus())
    {
      return false;
    }
    for (uint8_t attempt = 0; attempt < 3; ++attempt)
    {
      sPcfI2c.beginTransmission(EPD_PCF8574_ADDR);
      sPcfI2c.write(value);
      if (sPcfI2c.endTransmission() == 0)
      {
        delayMicroseconds(EPD_PCF_SETTLE_US);
        return true;
      }
      delay(1);
    }
    return false;
  }

  static bool pcfRead(uint8_t &value)
  {
    if (!initPcfI2cBus())
    {
      return false;
    }
    for (uint8_t attempt = 0; attempt < 3; ++attempt)
    {
      const size_t n =
          sPcfI2c.requestFrom(EPD_PCF8574_ADDR, static_cast<size_t>(1));
      if (n == 1)
      {
        value = static_cast<uint8_t>(sPcfI2c.read());
        return true;
      }
      delay(1);
    }
    return false;
  }

  static bool setPcfBit(PcfPin pin, bool high)
  {
    const uint8_t mask = static_cast<uint8_t>(1U << static_cast<uint8_t>(pin));
    const uint8_t next = high ? static_cast<uint8_t>(sPcfShadow | mask)
                              : static_cast<uint8_t>(sPcfShadow & ~mask);
    if (next == sPcfShadow)
    {
      return true;
    }
    if (!pcfWrite(next))
    {
      return false;
    }
    sPcfShadow = next;
    return true;
  }

  static bool readPcfBit(PcfPin pin, bool &high)
  {
    uint8_t value = 0xFF;
    if (!pcfRead(value))
    {
      return false;
    }
    const uint8_t mask = static_cast<uint8_t>(1U << static_cast<uint8_t>(pin));
    high = (value & mask) != 0;
    return true;
  }

  static void logPcfState(const char *tag)
  {
    uint8_t value = 0xFF;
    Serial.print("EPD: ");
    Serial.print(tag);
    Serial.print(" shadow=0x");
    printHexByte(sPcfShadow);
    if (!pcfRead(value))
    {
      Serial.println(" read=ERR");
      return;
    }
    Serial.print(" read=0x");
    printHexByte(value);
    Serial.print(" P0=");
    Serial.print((value >> 0U) & 0x01U);
    Serial.print(" P1=");
    Serial.print((value >> 1U) & 0x01U);
    Serial.print(" P2=");
    Serial.println((value >> 2U) & 0x01U);
  }

  static void runBootPinDiagnostics()
  {
    if (!EPD_DIAG_PULSE_ON_BOOT)
    {
      return;
    }
    Serial.println("EPD: 启动诊断，拉动 DC(P0) 低高脉冲");
    for (uint8_t i = 0; i < EPD_DIAG_PULSE_REPEAT; ++i)
    {
      if (!setPcfBit(PCF_PIN_DC, false))
      {
        Serial.println("EPD: 诊断 DC->LOW 写失败");
        return;
      }
      logPcfState("DC LOW");
      delay(EPD_DIAG_PULSE_HOLD_MS);
      if (!setPcfBit(PCF_PIN_DC, true))
      {
        Serial.println("EPD: 诊断 DC->HIGH 写失败");
        return;
      }
      logPcfState("DC HIGH");
      delay(EPD_DIAG_PULSE_HOLD_MS);
    }

    Serial.println("EPD: 启动诊断，拉动 RES(P1) 低高脉冲");
    if (!setPcfBit(PCF_PIN_RES, false))
    {
      Serial.println("EPD: 诊断 RES->LOW 写失败");
      return;
    }
    logPcfState("RES LOW");
    delay(EPD_DIAG_PULSE_HOLD_MS);
    if (!setPcfBit(PCF_PIN_RES, true))
    {
      Serial.println("EPD: 诊断 RES->HIGH 写失败");
      return;
    }
    logPcfState("RES HIGH");
    delay(EPD_DIAG_PULSE_HOLD_MS);
  }

  static bool waitBusyRelease(const char *phase,
                              uint32_t timeoutMs = EPD_BUSY_TIMEOUT_RESET_MS)
  {
    const uint32_t start = millis();
    uint8_t stableReadySamples = 0;
    bool firstSample = true;
    uint8_t lastRaw = 0xFF;
    bool lastReady = false;
    uint32_t readFailCount = 0;
    while ((millis() - start) < timeoutMs)
    {
      uint8_t raw = 0xFF;
      if (!pcfRead(raw))
      {
        ++readFailCount;
        stableReadySamples = 0;
        delay(EPD_BUSY_POLL_INTERVAL_MS);
        continue;
      }
      const bool busyHigh =
          (raw & static_cast<uint8_t>(1U << static_cast<uint8_t>(PCF_PIN_BUSY))) !=
          0;
      const bool busyActive = EPD_BUSY_ACTIVE_HIGH ? busyHigh : !busyHigh;
      const bool ready = !busyActive;
      if (firstSample || raw != lastRaw || ready != lastReady)
      {
        Serial.print("EPD: ");
        Serial.print(phase);
        Serial.print(" BUSY t=");
        Serial.print(millis() - start);
        Serial.print("ms raw=0x");
        printHexByte(raw);
        Serial.print(" shadow=0x");
        printHexByte(sPcfShadow);
        Serial.print(" busy_bit=");
        Serial.print(busyHigh ? 1 : 0);
        Serial.print(" active=");
        Serial.print(busyActive ? 1 : 0);
        Serial.print(" ready=");
        Serial.println(ready ? 1 : 0);
        firstSample = false;
        lastRaw = raw;
        lastReady = ready;
      }
      if (ready)
      {
        ++stableReadySamples;
        if (stableReadySamples >= EPD_BUSY_STABLE_SAMPLES)
        {
          Serial.print("EPD: ");
          Serial.print(phase);
          Serial.print(" BUSY released in ");
          Serial.print(millis() - start);
          Serial.print("ms, read_fail=");
          Serial.println(readFailCount);
          return true;
        }
      }
      else
      {
        stableReadySamples = 0;
      }
      delay(EPD_BUSY_POLL_INTERVAL_MS);
    }
    Serial.print("EPD: ");
    Serial.print(phase);
    Serial.print(" BUSY wait timeout after ");
    Serial.print(timeoutMs);
    Serial.print("ms, read_fail=");
    Serial.println(readFailCount);
    logPcfState("BUSY timeout snapshot");
    return false;
  }

  static bool tryHardwareResetAndWait(size_t attemptIndex, ResetProfile profile)
  {
    Serial.print("EPD: 硬复位尝试 #");
    Serial.print(attemptIndex + 1U);
    Serial.print(" low=");
    Serial.print(profile.lowMs);
    Serial.print("ms high=");
    Serial.print(profile.highMs);
    Serial.println("ms");

    if (!setPcfBit(PCF_PIN_RES, false))
    {
      Serial.println("EPD: 拉低 RES 失败");
      return false;
    }
    delay(profile.lowMs);
    logPcfState("RES LOW");

    if (!setPcfBit(PCF_PIN_RES, true))
    {
      Serial.println("EPD: 拉高 RES 失败");
      return false;
    }
    delay(profile.highMs);
    logPcfState("RES HIGH");

    if (waitBusyRelease("复位后", EPD_BUSY_TIMEOUT_RESET_ATTEMPT_MS))
    {
      return true;
    }

    Serial.print("EPD: 硬复位尝试 #");
    Serial.print(attemptIndex + 1U);
    Serial.println(" 后 BUSY 仍未释放");
    return false;
  }

  static bool writeCmd(uint8_t cmd)
  {
    digitalWrite(EPD_SPI_CS_PIN, LOW);
    delayMicroseconds(EPD_CS_SETUP_US);
    if (!setPcfBit(PCF_PIN_DC, false))
    {
      digitalWrite(EPD_SPI_CS_PIN, HIGH);
      return false;
    }
    delayMicroseconds(EPD_DC_SETUP_US);
    SPI.transfer(cmd);
    delayMicroseconds(EPD_CS_HOLD_US);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    delayMicroseconds(EPD_CS_IDLE_US);
    return true;
  }

  static bool writeDataByte(uint8_t data)
  {
    digitalWrite(EPD_SPI_CS_PIN, LOW);
    delayMicroseconds(EPD_CS_SETUP_US);
    if (!setPcfBit(PCF_PIN_DC, true))
    {
      digitalWrite(EPD_SPI_CS_PIN, HIGH);
      return false;
    }
    delayMicroseconds(EPD_DC_SETUP_US);
    SPI.transfer(data);
    delayMicroseconds(EPD_CS_HOLD_US);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    delayMicroseconds(EPD_CS_IDLE_US);
    return true;
  }

  static bool writeDataBuffer(const uint8_t *data, size_t len)
  {
    digitalWrite(EPD_SPI_CS_PIN, LOW);
    delayMicroseconds(EPD_CS_SETUP_US);
    if (!setPcfBit(PCF_PIN_DC, true))
    {
      digitalWrite(EPD_SPI_CS_PIN, HIGH);
      return false;
    }
    delayMicroseconds(EPD_DC_SETUP_US);
    for (size_t i = 0; i < len; ++i)
    {
      SPI.transfer(data[i]);
    }
    delayMicroseconds(EPD_CS_HOLD_US);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    delayMicroseconds(EPD_CS_IDLE_US);
    return true;
  }

  static bool powerPinsHigh()
  {
    if (!setPcfBit(PCF_PIN_RES, true))
    {
      return false;
    }
    if (!setPcfBit(PCF_PIN_DC, true))
    {
      return false;
    }
    return true;
  }

  static bool hardwareInitFull()
  {
    if (!powerPinsHigh())
    {
      Serial.println("EPD: 电源控制引脚置高失败");
      return false;
    }
    delay(EPD_POWER_STABILIZE_MS);
    logPcfState("复位前");

    bool resetReleased = false;
    for (size_t i = 0; i < (sizeof(kResetProfiles) / sizeof(kResetProfiles[0]));
         ++i)
    {
      if (tryHardwareResetAndWait(i, kResetProfiles[i]))
      {
        resetReleased = true;
        break;
      }
    }

    if (!resetReleased)
    {
      Serial.println("EPD: 复位后 BUSY 未释放");
      return false;
    }

    if (!setPcfBit(PCF_PIN_DC, false))
    {
      Serial.println("EPD: 预置 DC 低电平失败");
      return false;
    }
    delay(EPD_DC_DEBUG_PULSE_MS);

    SPI.beginTransaction(sSpiSettings);

    bool ok = true;
    ok = ok && writeCmd(0x12); // SWRESET

    SPI.endTransaction();
    if (!ok)
    {
      Serial.println("EPD: SWRESET 命令发送失败");
      return false;
    }
    logPcfState("SWRESET 后");
    if (!waitBusyRelease("SWRESET 后", EPD_BUSY_TIMEOUT_RESET_MS))
    {
      Serial.println("EPD: SWRESET 后 BUSY 未释放");
      return false;
    }

    SPI.beginTransaction(sSpiSettings);
    ok = ok && writeCmd(0x01); // Driver output control
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) % 256U));
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) / 256U));
    ok = ok && writeDataByte(0x00);

    ok = ok && writeCmd(0x21); // Display update control
    ok = ok && writeDataByte(0x40);
    ok = ok && writeDataByte(0x00);

    ok = ok && writeCmd(0x3C); // Border waveform
    ok = ok && writeDataByte(0x05);

    ok = ok && writeCmd(0x11); // Data entry mode
    ok = ok && writeDataByte(0x01);

    ok = ok && writeCmd(0x44); // RAM X start/end
    ok = ok && writeDataByte(0x00);
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_WIDTH / 8U) - 1U));

    ok = ok && writeCmd(0x45); // RAM Y start/end
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) % 256U));
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) / 256U));
    ok = ok && writeDataByte(0x00);
    ok = ok && writeDataByte(0x00);

    ok = ok && writeCmd(0x4E); // RAM X count
    ok = ok && writeDataByte(0x00);

    ok = ok && writeCmd(0x4F); // RAM Y count
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) % 256U));
    ok = ok && writeDataByte(static_cast<uint8_t>((EPD_HEIGHT - 1U) / 256U));

    SPI.endTransaction();
    if (!ok)
    {
      Serial.println("EPD: 初始化寄存器写入失败");
      return false;
    }

    logPcfState("初始化寄存器后");
    if (!waitBusyRelease("初始化寄存器后", EPD_BUSY_TIMEOUT_RESET_MS))
    {
      Serial.println("EPD: 初始化寄存器后 BUSY 未释放");
      return false;
    }
    return true;
  }

  static bool showFrameFull(const uint8_t *frameData)
  {
    SPI.beginTransaction(sSpiSettings);

    bool ok = true;
    ok = ok && writeCmd(0x24);
    ok = ok && writeDataBuffer(frameData, EPD_ARRAY);
    ok = ok && writeCmd(0x26);
    ok = ok && writeDataBuffer(frameData, EPD_ARRAY);

    ok = ok && writeCmd(0x22);
    ok = ok && writeDataByte(0xF7);
    ok = ok && writeCmd(0x20);

    SPI.endTransaction();
    if (!ok)
    {
      return false;
    }

    logPcfState("触发刷新后");
    if (!waitBusyRelease("刷新后", EPD_BUSY_TIMEOUT_UPDATE_MS))
    {
      Serial.println("EPD: 刷新等待 BUSY 释放超时");
      return false;
    }
    return true;
  }

  static void deepSleep()
  {
    SPI.beginTransaction(sSpiSettings);
    const bool ok = writeCmd(0x10) && writeDataByte(0x01);
    SPI.endTransaction();
    delay(100);

    (void)ok;
  }

  static void clearFrame()
  {
    memset(sFrame, 0xFF, sizeof(sFrame));
  }

  static void loadBootImageToFrame()
  {
    clearFrame();
    const size_t srcLen = static_cast<size_t>(get_boot_image_data_len());
    const size_t dstLen = EPD_ARRAY;
    const size_t dstRowBytes = EPD_WIDTH / 8U; // 400px -> 50B/row

    Serial.print("EPD: 测试图字节数=");
    Serial.print(srcLen);
    Serial.print("，面板帧缓冲字节数=");
    Serial.println(dstLen);

    if (srcLen == dstLen)
    {
      memcpy(sFrame, g_boot_image_data, dstLen);
      return;
    }

    // 常见取模数据格式: 每行连续字节。先按高度=300推断。
    if ((srcLen % EPD_HEIGHT) == 0U)
    {
      const size_t srcRowBytes = srcLen / EPD_HEIGHT;
      if (srcRowBytes > 0U)
      {
        const size_t copyColsBytes =
            (srcRowBytes < dstRowBytes) ? srcRowBytes : dstRowBytes;
        for (size_t y = 0; y < EPD_HEIGHT; ++y)
        {
          const size_t srcOff = y * srcRowBytes;
          const size_t dstOff = y * dstRowBytes;
          memcpy(&sFrame[dstOff], &g_boot_image_data[srcOff], copyColsBytes);
        }
        Serial.print("EPD: 按行贴图，推断源尺寸 ");
        Serial.print(srcRowBytes * 8U);
        Serial.print("x");
        Serial.print(EPD_HEIGHT);
        Serial.println("（超出裁剪，不足补白）");
        return;
      }
    }

    // 另一种可能：宽度正好是 400，源高度不足 300。
    if ((srcLen % dstRowBytes) == 0U)
    {
      const size_t srcRows = srcLen / dstRowBytes;
      if (srcRows > 0U)
      {
        const size_t copyRows = (srcRows < EPD_HEIGHT) ? srcRows : EPD_HEIGHT;
        memcpy(sFrame, g_boot_image_data, copyRows * dstRowBytes);
        Serial.print("EPD: 线性按 400 宽贴图，推断源尺寸 400x");
        Serial.println(srcRows);
        return;
      }
    }

    // 兜底：只做截断拷贝，至少不越界。
    const size_t copyLen = (srcLen < dstLen) ? srcLen : dstLen;
    memcpy(sFrame, g_boot_image_data, copyLen);
    Serial.println("EPD: 警告，图片尺寸未知，已按线性截断拷贝");
  }

  static void setPixelBlack(int16_t x, int16_t y)
  {
    if (x < 0 || y < 0 || x >= static_cast<int16_t>(EPD_WIDTH) ||
        y >= static_cast<int16_t>(EPD_HEIGHT))
    {
      return;
    }
    const size_t idx =
        static_cast<size_t>(y) * (EPD_WIDTH / 8U) + static_cast<size_t>(x / 8);
    const uint8_t bit = static_cast<uint8_t>(0x80U >> static_cast<uint8_t>(x % 8));
    sFrame[idx] = static_cast<uint8_t>(sFrame[idx] & ~bit);
  }

  static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled)
  {
    if (w <= 0 || h <= 0)
    {
      return;
    }
    if (filled)
    {
      for (int16_t yy = y; yy < (y + h); ++yy)
      {
        for (int16_t xx = x; xx < (x + w); ++xx)
        {
          setPixelBlack(xx, yy);
        }
      }
      return;
    }
    for (int16_t xx = x; xx < (x + w); ++xx)
    {
      setPixelBlack(xx, y);
      setPixelBlack(xx, y + h - 1);
    }
    for (int16_t yy = y; yy < (y + h); ++yy)
    {
      setPixelBlack(x, yy);
      setPixelBlack(x + w - 1, yy);
    }
  }

  static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
  {
    int16_t dx = abs(x1 - x0);
    const int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    const int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (true)
    {
      setPixelBlack(x0, y0);
      if (x0 == x1 && y0 == y1)
      {
        break;
      }
      const int16_t e2 = static_cast<int16_t>(2 * err);
      if (e2 >= dy)
      {
        err = static_cast<int16_t>(err + dy);
        x0 = static_cast<int16_t>(x0 + sx);
      }
      if (e2 <= dx)
      {
        err = static_cast<int16_t>(err + dx);
        y0 = static_cast<int16_t>(y0 + sy);
      }
    }
  }

  static void drawGlyph5x7(int16_t x, int16_t y, const uint8_t rows[7],
                           uint8_t scale)
  {
    if (scale == 0)
    {
      return;
    }
    for (uint8_t row = 0; row < 7; ++row)
    {
      for (uint8_t col = 0; col < 5; ++col)
      {
        const bool on = (rows[row] & (1U << (4U - col))) != 0;
        if (!on)
        {
          continue;
        }
        const int16_t px = static_cast<int16_t>(x + col * scale);
        const int16_t py = static_cast<int16_t>(y + row * scale);
        drawRect(px, py, scale, scale, true);
      }
    }
  }

  static void drawTextEpdOk(int16_t x, int16_t y, uint8_t scale)
  {
    static const uint8_t kGlyphE[7] = {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t kGlyphP[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t kGlyphD[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t kGlyphO[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t kGlyphK[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};

    drawGlyph5x7(x, y, kGlyphE, scale);
    drawGlyph5x7(static_cast<int16_t>(x + 7 * scale), y, kGlyphP, scale);
    drawGlyph5x7(static_cast<int16_t>(x + 14 * scale), y, kGlyphD, scale);
    drawGlyph5x7(static_cast<int16_t>(x + 24 * scale), y, kGlyphO, scale);
    drawGlyph5x7(static_cast<int16_t>(x + 31 * scale), y, kGlyphK, scale);
  }

  static void buildSelfTestPattern()
  {
    clearFrame();

    drawRect(0, 0, EPD_WIDTH, EPD_HEIGHT, false);
    drawRect(4, 4, EPD_WIDTH - 8, EPD_HEIGHT - 8, false);

    for (int16_t x = 12; x < static_cast<int16_t>(EPD_WIDTH - 12); x += 24)
    {
      drawLine(x, 110, x, 280);
    }
    for (int16_t y = 110; y < 280; y += 24)
    {
      drawLine(12, y, static_cast<int16_t>(EPD_WIDTH - 12), y);
    }

    drawLine(12, 12, static_cast<int16_t>(EPD_WIDTH - 12),
             static_cast<int16_t>(EPD_HEIGHT - 12));
    drawLine(static_cast<int16_t>(EPD_WIDTH - 12), 12, 12,
             static_cast<int16_t>(EPD_HEIGHT - 12));

    drawRect(18, 210, 70, 70, true);
    drawRect(static_cast<int16_t>(EPD_WIDTH - 88), 210, 70, 70, false);
    drawRect(static_cast<int16_t>(EPD_WIDTH - 80), 218, 54, 54, true);

    drawTextEpdOk(26, 28, 6);
  }

  static bool begin()
  {
    if (sInitialized)
    {
      return true;
    }

    if (!initPcfI2cBus())
    {
      Serial.println("EPD: SoftWire I2C 初始化失败");
      return false;
    }
    Serial.print("EPD: PCF8574 使用 SoftWire I2C, SDA=GPIO");
    Serial.print(EPD_I2C_SDA_PIN);
    Serial.print(" SCL=GPIO");
    Serial.println(EPD_I2C_SCL_PIN);
    Serial.print("EPD: BUSY 极性=");
    Serial.println(EPD_BUSY_ACTIVE_HIGH ? "high-active" : "low-active");
    Serial.println("EPD: 接口模式=4-wire SPI(8-bit), DC 由 PCF8574 控制");

    pinMode(EPD_SPI_CS_PIN, OUTPUT);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    SPI.begin(EPD_SPI_SCLK_PIN, -1, EPD_SPI_MOSI_PIN, EPD_SPI_CS_PIN);
    Serial.print("EPD: SPI pin map CS=");
    Serial.print(EPD_SPI_CS_PIN);
    Serial.print(" MOSI=");
    Serial.print(EPD_SPI_MOSI_PIN);
    Serial.print(" SCLK=");
    Serial.println(EPD_SPI_SCLK_PIN);

    // 上电后直接做一次 SPI 引脚脉冲，独立于 BUSY/PCF 逻辑，便于示波确认连线。
    digitalWrite(EPD_SPI_CS_PIN, LOW);
    delay(2);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    delay(2);
    SPI.beginTransaction(sSpiSettings);
    digitalWrite(EPD_SPI_CS_PIN, LOW);
    delayMicroseconds(EPD_CS_SETUP_US);
    SPI.transfer(0xA5);
    delayMicroseconds(EPD_CS_HOLD_US);
    digitalWrite(EPD_SPI_CS_PIN, HIGH);
    SPI.endTransaction();

    sPcfShadow = 0xFF;
    if (!pcfWrite(sPcfShadow))
    {
      Serial.println("EPD: PCF8574 未响应");
      return false;
    }
    logPcfState("PCF 初始态");
    if (!powerPinsHigh())
    {
      Serial.println("EPD: PCF8574 引脚初始化失败");
      return false;
    }
    logPcfState("控制引脚置高后");
    runBootPinDiagnostics();

    sInitialized = true;
    return true;
  }

  bool runBootSelfTest()
  {
    if (!begin())
    {
      return false;
    }
    loadBootImageToFrame();

    if (!hardwareInitFull())
    {
      Serial.println("EPD: 初始化失败(BUSY 超时或总线异常)");
      return false;
    }
    if (!showFrameFull(sFrame))
    {
      Serial.println("EPD: 刷新失败");
      return false;
    }

    deepSleep();
    return true;
  }

}
