#include <LoRa.h>

#define SS 5
#define RST 22
#define DIO0 4
#define RELAY_PIN 32
#define WAKE_PIN GPIO_NUM_4

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("EXT0 Wakeup (LoRa DIO0)"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("TIMER"); break;
    default : Serial.println("Other"); break;
  }

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed.");
    while (true);
  }
  LoRa.receive();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Checking for LoRa packet...");
    int packetSize = LoRa.parsePacket();
    Serial.print("Packet size: ");
    Serial.println(packetSize);

    if (packetSize) {
      String msg = "";
      while (LoRa.available()) {
        msg += (char)LoRa.read();
      }
      Serial.println("Received: " + msg);

      if (msg == "ON") {
        digitalWrite(RELAY_PIN, HIGH);
        delay(5000);
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Relay activated.");
      }
    } else {
      Serial.println("No LoRa packet received.");
    }
  }

  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0);

  Serial.println("Going to sleep...");
  delay(100);
  esp_deep_sleep_start();
}

void loop() {
  // ไม่ใช้งาน
}
