#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "0000fff0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000fff2-0000-1000-8000-00805f9b34fb"

const uint8_t mac_address[] = { 0xF8, 0x8F, 0xC8, 0x9E, 0xF2, 0xD0 };

const uint8_t calibration_sequence[] = { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF };

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

void startCustomAdvertising() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  Serial.println("Start advertising...");
  BLEDevice::startAdvertising();
}

int32_t decodeLcdSegmentCodeValue(const uint8_t *data, size_t length) {
    int32_t value = 0;
    for (int i = (int)length - 1; i >= 0; i--) {
        int digit = 0;
        switch (data[i] & 0x7F) {
            case 0x3F: digit = 0; break;
            case 0x06: digit = 1; break;
            case 0x5B: digit = 2; break;
            case 0x4F: digit = 3; break;
            case 0x66: digit = 4; break;
            case 0x6D: digit = 5; break;
            case 0x7D: digit = 6; break;
            case 0x07: digit = 7; break;
            case 0x7F: digit = 8; break;
            case 0x6F: digit = 9; break;
            default: continue;
        }
        value = (value * 10) + digit;
    }
    return value;
}

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        uint8_t* pData = pChar->getData();
        size_t length = pChar->getLength();
        if (length >= 11) {
            if (memcmp(pData + 7, calibration_sequence, sizeof(calibration_sequence))) {
              int32_t value = decodeLcdSegmentCodeValue(pData + 7, 5);
              Serial.printf("%d\t%0.1f g\n", millis(), value / 10.0);
            } else {
              Serial.println("Calibrating...");
            }
        }
    }
};


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
    }

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Disconnected");
      startCustomAdvertising();
    }
};

void setup() {
  Serial.begin(115200);

  uint8_t base_mac[] = { mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5] - 2 };
  
  esp_err_t err = esp_base_mac_addr_set(base_mac);
  if (err != ESP_OK) {
    Serial.printf("ERROR: esp_base_mac_addr_set failed with 0x%X\n", err);
  }

  BLEDevice::init("NerdCoff");

  Serial.printf("Using mac address %s\n", BLEDevice::getAddress().toString().c_str());

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ  |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pService->start();

  startCustomAdvertising();
}

void loop() {
  delay(1000);
}