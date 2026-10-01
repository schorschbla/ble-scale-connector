#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "0000fff0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000fff2-0000-1000-8000-00805f9b34fb"


BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

void startCustomAdvertising() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
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

const uint8_t calibrationValue[] = { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF };

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        uint8_t* pData = pChar->getData();
        size_t length = pChar->getLength();

        if (length >= 11) {
            if (memcmp(pData + 7, calibrationValue, sizeof(calibrationValue))) {
              int32_t value = decodeLcdSegmentCodeValue(pData + 7, 5);
              Serial.printf("%d\t%0.1f g\n", millis(), value / 10.0);
            } else {
              Serial.println("Kalibriere...\n");
            }
        }
    }
};


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("\n-> [CONNECT] Master hat die Verbindung aufgebaut!");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("\n-> [DISCONNECT] Master hat die Verbindung getrennt.");
      
      delay(500);
      startCustomAdvertising();
      Serial.println("-> [ADV] Sende wieder Custom Raw Advertisements... Warte auf Master.");
    }
};

void setup() {
  Serial.begin(115200);

  uint8_t base_mac[] = {0xF8, 0x8F, 0xC8, 0x9E, 0xF2, 0xCE};
  
  esp_err_t err = esp_base_mac_addr_set(base_mac);
  if (err == ESP_OK) {
    Serial.println("[MAC] Basis-MAC-Adresse erfolgreich im RAM geändert.");
  } else {
    Serial.printf("[MAC] FEHLER beim Ändern der MAC: 0x%X\n", err);
  }


  BLEDevice::init("NerdCoff");
  Serial.print("[MAC] Aktive emulierte BLE-Adresse: ");
  Serial.println(BLEDevice::getAddress().toString().c_str()); 

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
  Serial.println("[ADV] Custom Raw Advertisements aktiv! Warte auf einkommende Verbindung vom Master...");
}

void loop() {
  delay(1000);
}