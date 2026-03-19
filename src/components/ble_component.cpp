#include "ble_component.h"

#include <Arduino.h>
#include <Esp.h>

#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../core/state_store.h"
#include "../protocol/ble_protocol.h"
#include "../settings.h"
#include "meter_component.h"

namespace BleComponent
{
  namespace
  {

    BLEServer *sServer = nullptr;
    BLECharacteristic *sDeviceInfoCharacteristic = nullptr;
    BLECharacteristic *sSensorCharacteristic = nullptr;
    BLECharacteristic *sMeterCharacteristic = nullptr;
    BLECharacteristic *sConfigCharacteristic = nullptr;
    BLE2902 *sSensorCccd = nullptr;
    BLE2902 *sMeterCccd = nullptr;
    BLE2902 *sConfigCccd = nullptr;

    bool sInitialized = false;
    bool sConnected = false;
    bool sForceConfigSync = false;
    std::string sDeviceName;
    std::string sDeviceId;

    uint8_t sLastSensorPacket[BleProtocol::kSensorPacketSize] = {0};
    uint8_t sLastMeterPacket[BleProtocol::kMeterPacketSize] = {0};
    uint8_t sLastConfigPacket[BleProtocol::kConfigPacketSize] = {0};
    bool sHasSensorPacket = false;
    bool sHasMeterPacket = false;
    bool sHasConfigPacket = false;

    std::string formatDeviceId(uint64_t efuseMac)
    {
      char buffer[13] = {0};
      snprintf(buffer, sizeof(buffer), "%012llX",
               static_cast<unsigned long long>(efuseMac & 0xFFFFFFFFFFFFULL));
      return std::string(buffer);
    }

    void prepareDeviceIdentity()
    {
      if (!sDeviceName.empty() && !sDeviceId.empty())
      {
        return;
      }

      sDeviceId = formatDeviceId(ESP.getEfuseMac());
      sDeviceName = BLE_DEVICE_NAME;
      sDeviceName += "-";
      sDeviceName += sDeviceId.substr(8);
    }

    bool notificationsEnabled(BLE2902 *cccd)
    {
      return sConnected && cccd != nullptr && cccd->getNotifications();
    }

    void setCharacteristicValue(BLECharacteristic *characteristic,
                                const uint8_t *data, size_t size)
    {
      if (characteristic == nullptr)
      {
        return;
      }

      characteristic->setValue(const_cast<uint8_t *>(data), size);
    }

    bool buildSensorPacket(uint8_t *buffer)
    {
      SensorData sensorData;
      if (!loadSensorData(sensorData))
      {
        return false;
      }

      return BleProtocol::encodeSensorPacket(sensorData, buffer,
                                             BleProtocol::kSensorPacketSize);
    }

    bool buildMeterPacket(uint8_t *buffer)
    {
      SensorData sensorData;
      MeterConfig config;
      if (!loadSensorData(sensorData) || !loadMeterConfig(config))
      {
        return false;
      }

      const ExposureResult exposure = MeterComponent::calculateExposure(sensorData, config);
      return BleProtocol::encodeMeterPacket(config, exposure, buffer,
                                            BleProtocol::kMeterPacketSize);
    }

    bool buildConfigPacket(uint8_t *buffer)
    {
      MeterConfig config;
      if (!loadMeterConfig(config))
      {
        return false;
      }

      return BleProtocol::encodeConfigPacket(config, buffer,
                                             BleProtocol::kConfigPacketSize);
    }

    void refreshSensorCharacteristic(bool notify)
    {
      if (sSensorCharacteristic == nullptr)
      {
        return;
      }

      uint8_t packet[BleProtocol::kSensorPacketSize];
      if (!buildSensorPacket(packet))
      {
        return;
      }

      const bool changed =
          !sHasSensorPacket ||
          memcmp(packet, sLastSensorPacket, BleProtocol::kSensorPacketSize) != 0;
      if (!changed && !notify)
      {
        return;
      }

      memcpy(sLastSensorPacket, packet, BleProtocol::kSensorPacketSize);
      sHasSensorPacket = true;
      setCharacteristicValue(sSensorCharacteristic, packet,
                             BleProtocol::kSensorPacketSize);
      if ((changed || notify) && notificationsEnabled(sSensorCccd))
      {
        sSensorCharacteristic->notify();
      }
    }

    void refreshMeterCharacteristic(bool notify)
    {
      if (sMeterCharacteristic == nullptr)
      {
        return;
      }

      uint8_t packet[BleProtocol::kMeterPacketSize];
      if (!buildMeterPacket(packet))
      {
        return;
      }

      const bool changed =
          !sHasMeterPacket ||
          memcmp(packet, sLastMeterPacket, BleProtocol::kMeterPacketSize) != 0;
      if (!changed && !notify)
      {
        return;
      }

      memcpy(sLastMeterPacket, packet, BleProtocol::kMeterPacketSize);
      sHasMeterPacket = true;
      setCharacteristicValue(sMeterCharacteristic, packet,
                             BleProtocol::kMeterPacketSize);
      if ((changed || notify) && notificationsEnabled(sMeterCccd))
      {
        sMeterCharacteristic->notify();
      }
    }

    void refreshConfigCharacteristic(bool notify)
    {
      if (sConfigCharacteristic == nullptr)
      {
        return;
      }

      uint8_t packet[BleProtocol::kConfigPacketSize];
      if (!buildConfigPacket(packet))
      {
        return;
      }

      const bool changed =
          !sHasConfigPacket ||
          memcmp(packet, sLastConfigPacket, BleProtocol::kConfigPacketSize) != 0;
      if (!changed && !notify)
      {
        return;
      }

      memcpy(sLastConfigPacket, packet, BleProtocol::kConfigPacketSize);
      sHasConfigPacket = true;
      setCharacteristicValue(sConfigCharacteristic, packet,
                             BleProtocol::kConfigPacketSize);
      if ((changed || notify) && notificationsEnabled(sConfigCccd))
      {
        sConfigCharacteristic->notify();
      }
    }

    class AppBleServerCallbacks : public BLEServerCallbacks
    {
    public:
      void onConnect(BLEServer *server) override
      {
        (void)server;
        sConnected = true;
        Serial.println("BLE: 客户端已连接");
      }

      void onDisconnect(BLEServer *server) override
      {
        (void)server;
        sConnected = false;
        Serial.println("BLE: 客户端已断开，重新开始广播");
        BLEDevice::startAdvertising();
      }
    };

    class AppConfigCharacteristicCallbacks : public BLECharacteristicCallbacks
    {
    public:
      void onWrite(BLECharacteristic *characteristic) override
      {
        if (characteristic == nullptr)
        {
          return;
        }

        const std::string value = characteristic->getValue();
        MeterConfig nextConfig;
        if (!BleProtocol::decodeConfigPacket(
                reinterpret_cast<const uint8_t *>(value.data()), value.size(),
                nextConfig))
        {
          Serial.println("BLE: 收到无效配置包，已忽略");
          refreshConfigCharacteristic(false);
          return;
        }

        if (!saveMeterConfig(nextConfig))
        {
          Serial.println("BLE: 配置保存失败");
          refreshConfigCharacteristic(false);
          return;
        }

        sForceConfigSync = true;
        Serial.println("BLE: 配置已更新");
      }
    };

    AppBleServerCallbacks sServerCallbacks;
    AppConfigCharacteristicCallbacks sConfigCallbacks;

    std::string buildDeviceInfoString()
    {
      return BleProtocol::buildDeviceInfoValue(sDeviceName, sDeviceId);
    }

  } // namespace

  bool begin()
  {
    if (sInitialized)
    {
      return true;
    }
    if (!ENABLE_BLE)
    {
      return false;
    }

    prepareDeviceIdentity();
    BLEDevice::init(sDeviceName);
    BLEDevice::setMTU(185);

    sServer = BLEDevice::createServer();
    if (sServer == nullptr)
    {
      Serial.println("BLE: createServer 失败");
      return false;
    }

    sServer->setCallbacks(&sServerCallbacks);

    BLEService *service = sServer->createService(BleProtocol::kServiceUuid);
    if (service == nullptr)
    {
      Serial.println("BLE: createService 失败");
      return false;
    }

    sDeviceInfoCharacteristic = service->createCharacteristic(
        BleProtocol::kDeviceInfoCharacteristicUuid,
        BLECharacteristic::PROPERTY_READ);
    sSensorCharacteristic = service->createCharacteristic(
        BleProtocol::kSensorCharacteristicUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    sMeterCharacteristic = service->createCharacteristic(
        BleProtocol::kMeterCharacteristicUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    sConfigCharacteristic = service->createCharacteristic(
        BleProtocol::kConfigCharacteristicUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY);

    if (sDeviceInfoCharacteristic == nullptr || sSensorCharacteristic == nullptr ||
        sMeterCharacteristic == nullptr || sConfigCharacteristic == nullptr)
    {
      Serial.println("BLE: createCharacteristic 失败");
      return false;
    }

    sSensorCccd = new BLE2902();
    sMeterCccd = new BLE2902();
    sConfigCccd = new BLE2902();

    sSensorCharacteristic->addDescriptor(sSensorCccd);
    sMeterCharacteristic->addDescriptor(sMeterCccd);
    sConfigCharacteristic->addDescriptor(sConfigCccd);
    sConfigCharacteristic->setCallbacks(&sConfigCallbacks);

    const std::string deviceInfo = buildDeviceInfoString();
    sDeviceInfoCharacteristic->setValue(deviceInfo);
    refreshSensorCharacteristic(false);
    refreshMeterCharacteristic(false);
    refreshConfigCharacteristic(false);

    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BleProtocol::kServiceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();

    sInitialized = true;
    return true;
  }

  void poll()
  {
    if (!sInitialized)
    {
      return;
    }

    refreshSensorCharacteristic(false);
    refreshMeterCharacteristic(false);
    refreshConfigCharacteristic(sForceConfigSync);
    sForceConfigSync = false;
  }

}
