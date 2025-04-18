#include <LoRa.h>

#define SS 9
#define RST 14
#define DIO0 4         // DIO0 ไปที่ GPIO4 เพื่อ Wakeup
#define RELAY_PIN 16

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

  // เริ่ม LoRa ก่อน parse
  LoRa.setPins(SS, RST, DIO0);
  LoRa.begin(433E6);

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String msg = "";
      while (LoRa.available()) {
        msg += (char)LoRa.read();
      }
      Serial.println("Received: " + msg);

      if (msg == "ON") {
        digitalWrite(RELAY_PIN, HIGH);
        delay(5000);
      }
    } else {
      Serial.println("No LoRa packet received.");
    }
  }

  // ตั้ง wakeup จาก DIO0
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0);

  Serial.println("Going to sleep...");
  delay(100);
  esp_deep_sleep_start();
}

void loop() {
  // ไม่ใช้งาน
}
