#include "system_diagnostics.h"

#include <Wire.h>

#include "../system_resources.h"
// #include "../components/epd_component.h"

void printIoTable()
{
  Serial.println("系统资源表(IO):");
  for (size_t i = 0; i < SystemResources::IO_TABLE_SIZE; ++i)
  {
    const SystemResources::IoEntry &entry = SystemResources::IO_TABLE[i];
    Serial.print(" - ");
    Serial.print(entry.name);
    Serial.print(" : GPIO");
    Serial.print(entry.pin);
    Serial.print(" [");
    Serial.print(entry.mode);
    Serial.print("] ");
    Serial.println(entry.description);
  }
}

static void scanSensorI2cBus()
{
  Serial.print("I2C0 扫描开始 (Wire SDA=GPIO");
  Serial.print(SystemResources::PIN_I2C_SDA);
  Serial.print(", SCL=GPIO");
  Serial.print(SystemResources::PIN_I2C_SCL);
  Serial.println(")");

  uint8_t found = 0;
  for (int addr = 0x03; addr <= 0x77; ++addr)
  {
    Wire.beginTransmission(static_cast<uint8_t>(addr));
    if (Wire.endTransmission() != 0)
    {
      continue;
    }

    ++found;
    Serial.print(" - 发现设备: 0x");
    if (addr < 16)
    {
      Serial.print('0');
    }
    Serial.print(addr, HEX);
    if (addr == SystemResources::I2C_ADDR_TSL2591)
    {
      Serial.print(" (TSL2591)");
    }
    Serial.println();
  }

  if (found == 0)
  {
    Serial.println("I2C0 扫描完成: 未发现设备");
    return;
  }

  Serial.print("I2C0 扫描完成: 共发现 ");
  Serial.print(found);
  Serial.println(" 个设备");
}

// static void scanEpdI2cBus()
// {
//   Serial.print("I2C1 扫描开始 (SoftWire SDA=GPIO");
//   Serial.print(SystemResources::PIN_EPD_I2C_SDA);
//   Serial.print(", SCL=GPIO");
//   Serial.print(SystemResources::PIN_EPD_I2C_SCL);
//   Serial.println(")");

//   if (!EpdComponent::initPcfI2cBus())
//   {
//     Serial.println("I2C1 扫描失败: SoftWire 初始化失败");
//     return;
//   }

//   uint8_t found = 0;
//   for (int addr = 0x03; addr <= 0x77; ++addr)
//   {
//     if (!EpdComponent::probeAddress(static_cast<uint8_t>(addr)))
//     {
//       continue;
//     }

//     ++found;
//     Serial.print(" - 发现设备: 0x");
//     if (addr < 16)
//     {
//       Serial.print('0');
//     }
//     Serial.print(addr, HEX);
//     if (addr == SystemResources::I2C_ADDR_EPD_PCF8574)
//     {
//       Serial.print(" (PCF8574)");
//     }
//     Serial.println();
//   }

//   if (found == 0)
//   {
//     Serial.println("I2C1 扫描完成: 未发现设备");
//     return;
//   }

//   Serial.print("I2C1 扫描完成: 共发现 ");
//   Serial.print(found);
//   Serial.println(" 个设备");
// }

void scanI2cBuses()
{
  scanSensorI2cBus();
  // scanEpdI2cBus();
}
