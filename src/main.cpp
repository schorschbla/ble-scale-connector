#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// 1. UUIDs definieren
#define SERVICE_UUID        "0000fff0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000fff2-0000-1000-8000-00805f9b34fb"

// Ihr exakter, gewünschter Raw Advertisement Payload (12 Bytes)
uint8_t raw_adv_bytes[] = {
  0x02, 0x01, 0x06,                                      // Flags
  0x08, 0xFF, 0xF8, 0x8F, 0xC8, 0x9E, 0xF2, 0xD0, 0x00   // Manufacturer Data
};

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// Hilfsfunktion für das Custom-Advertising
void startCustomAdvertising() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  
  // Byte-Array in einen Binär-String umwandeln und übergeben
  std::string advDataStr((char*)raw_adv_bytes, sizeof(raw_adv_bytes));
  oAdvertisementData.addData(advDataStr);
  
  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponse(true); // Wichtig, damit der Master die Verbindung einleiten kann
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

// 2. DATEN-CALLBACK: Hier landen die unaufgeforderten Schreibbefehle des Masters
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        // Daten aus der Characteristic auslesen
        uint8_t* pData = pChar->getData();
        size_t length = pChar->getLength();

        if (length >= 11) {
            int32_t value = decodeLcdSegmentCodeValue(pData + 7, 5);
            Serial.printf("%0.1f g\n", value / 10.0);
        }
    }
};

// 3. Callbacks für den Verbindungsstatus
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("\n-> [CONNECT] Master hat die Verbindung aufgebaut!");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("\n-> [DISCONNECT] Master hat die Verbindung getrennt.");
      
      // Sofort das Custom-Advertising neu starten, damit der Master wieder verbinden kann
      delay(500);
      startCustomAdvertising();
      Serial.println("-> [ADV] Sende wieder Custom Raw Advertisements... Warte auf Master.");
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=================================================");
  Serial.println("ESP32 BLE: Custom MAC + Adv -> Direct Connection ");
  Serial.println("=================================================");

  // 1. BLE-Adresse fälschen (Spoofing)
  // Gewünschte BLE-Adresse: F8:8F:C8:9E:F2:D0 (Intern -2 verschoben -> CE)
  uint8_t base_mac[] = {0xF8, 0x8F, 0xC8, 0x9E, 0xF2, 0xCE};
  
  esp_err_t err = esp_base_mac_addr_set(base_mac);
  if (err == ESP_OK) {
    Serial.println("[MAC] Basis-MAC-Adresse erfolgreich im RAM geändert.");
  } else {
    Serial.printf("[MAC] FEHLER beim Ändern der MAC: 0x%X\n", err);
  }

  // 2. BLE-Stack initialisieren (Nimmt die gefälschte Adresse an)
  BLEDevice::init("");
  Serial.print("[MAC] Aktive emulierte BLE-Adresse: ");
  Serial.println(BLEDevice::getAddress().toString().c_str()); 

  // 3. BLE Server erstellen & Callbacks zuweisen
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 4. BLE Service erstellen
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 5. Characteristic (0xfff2) erstellen
  // Wir erlauben dem Master hier Read und Write (mit und ohne Antwort)
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ  |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );

  // Daten-Callback an die Characteristic hängen
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // 6. Service starten
  pService->start();

  // 7. Custom Advertising starten
  startCustomAdvertising();
  Serial.println("[ADV] Custom Raw Advertisements aktiv! Warte auf einkommende Verbindung vom Master...");
}

void loop() {
  // Keine periodischen Aktionen nötig – alles passiert ereignisbasiert in den Callbacks.
  delay(1000);
}