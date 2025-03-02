#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

const long frequencies[] = {915E6, 868E6, 433E6}; // Liste des fréquences disponibles
int currentFrequencyIndex = 0; // Index de la fréquence actuelle

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Receiver");

  if (!LoRa.begin(frequencies[currentFrequencyIndex])) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("Démarré avec succès");

  // Configuration LoRa
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
}

void changeFrequency(int newFrequencyIndex) {
  LoRa.end();
  if (!LoRa.begin(frequencies[newFrequencyIndex])) {
    Serial.println("Changement de fréquence échoué !");
    while (1);
  }
  currentFrequencyIndex = newFrequencyIndex;
  Serial.print("Changement de fréquence vers : ");
  Serial.println(frequencies[currentFrequencyIndex]);
}

void informEmetteursOfFrequencyChange() {
  StaticJsonDocument<100> doc;
  doc["type"] = "frequency_change";
  doc["new_frequency"] = frequencies[currentFrequencyIndex];

  char buffer[100];
  serializeJson(doc, buffer);

  LoRa.beginPacket();
  LoRa.print(buffer);
  LoRa.endPacket();

  Serial.println("Message de changement de fréquence envoyé aux émetteurs.");
}

bool waitForConfirmation() {
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) { // Attendre 10 secondes
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String receivedData = "";
      while (LoRa.available()) {
        char c = LoRa.read();
        if (isPrintable(c)) {
          receivedData += c;
        }
      }

      StaticJsonDocument<100> doc;
      DeserializationError error = deserializeJson(doc, receivedData);

      if (!error && doc["type"] == "confirmation") {
        Serial.println("Confirmation reçue d'un émetteur.");
        return true;
      }
    }
  }
  Serial.println("Aucune confirmation reçue.");
  return false;
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
      char c = LoRa.read();
      if (isPrintable(c)) {
        receivedData += c;
      }
    }

    // Vérification du format UTF-8
    bool isUTF8 = true;
    for (int i = 0; i < receivedData.length(); i++) {
      if ((receivedData[i] & 0xC0) == 0x80 || (receivedData[i] & 0xF8) == 0xF8) {
        isUTF8 = false;
        break;
      }
    }

    if (isUTF8) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, receivedData);

      if (!error) {
        int emetteur = doc["emetteur"];
        int capteur1 = doc["capteur1"];
        int capteur2 = doc["capteur2"];
        int capteur3 = doc["capteur3"];
        int capteur4 = doc["capteur4"];

        Serial.print("Paquet reçu de l'émetteur ");
        Serial.print(emetteur);
        Serial.print(": ");
        Serial.print(capteur1);
        Serial.print(", ");
        Serial.print(capteur2);
        Serial.print(", ");
        Serial.print(capteur3);
        Serial.print(", ");
        Serial.println(capteur4);

        // Vérification du RSSI
        int rssi = -abs(LoRa.packetRssi());
        Serial.print("RSSI: ");
        Serial.print(rssi);
        Serial.println(" dBm");

        if (rssi < -120) {
          Serial.println("RSSI trop faible, changement de fréquence...");
          int newFrequencyIndex = (currentFrequencyIndex + 1) % (sizeof(frequencies) / sizeof(frequencies[0]));
          changeFrequency(newFrequencyIndex);
          informEmetteursOfFrequencyChange();

          if (!waitForConfirmation()) {
            Serial.println("Aucune confirmation, tentative de changement de fréquence supplémentaire...");
            newFrequencyIndex = (newFrequencyIndex + 1) % (sizeof(frequencies) / sizeof(frequencies[0]));
            changeFrequency(newFrequencyIndex);
            informEmetteursOfFrequencyChange();

            if (!waitForConfirmation()) {
              Serial.println("Échec du changement de fréquence. Retour à la fréquence initiale.");
              changeFrequency(0); // Retour à la fréquence initiale
            }
          }
        }
      } else {
        Serial.println("Erreur de désérialisation JSON");
      }
    } else {
      Serial.println("Message non UTF-8 reçu, ignoré");
    }
  }
}