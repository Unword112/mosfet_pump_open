#include <LoRa.h>

#define SS 5
#define RST 22
#define DIO0 26
#define PUMP_PIN 32
#define WAKE_PIN GPIO_NUM_4
#define SOIL_SENSOR_PIN 25

int soilValue = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  digitalWrite(PUMP_PIN, LOW);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("EXT0 Wakeup (LoRa DIO0)"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("TIMER"); break;
    default : Serial.println("Other"); break;
  }

  soilValue = analogRead(SOIL_SENSOR_PIN);
  Serial.print("Soil value: ");
  Serial.println(soilValue);

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed.");
    while (true);
  }

  LoRa.receive();
  Serial.println("Waiting for LoRa packet...");

  unsigned long pumpStart = millis();         // จับเวลาตอนเปิดปั๊ม
  unsigned long pumpDuration = 10000;   

  while (millis() - pumpStart < pumpDuration) {  // รอ packet 3 วินาที
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String msg = "";
      while (LoRa.available()) {
        msg += (char)LoRa.read();
      }

      Serial.println("Received: " + msg);

      if (msg == "ON") {
        if (soilValue > 3500) {
          digitalWrite(PUMP_PIN, HIGH);
          Serial.println("PUMP ON LoRa");
          delay(5000);  // หรือเปลี่ยนตามต้องการ
          digitalWrite(PUMP_PIN, LOW);
        } else {
          digitalWrite(PUMP_PIN, LOW);
          Serial.println("PUMP OFF, DENIED MASTER");
        }
      } else if (msg == "OFF") {
        digitalWrite(PUMP_PIN, LOW);
        Serial.println("PUMP OFF, FORCE FROM MASTER");
      }

      if (digitalRead(PUMP_PIN) == LOW) {
        Serial.println("Pump forced OFF via RPC");
        break;
      }
    }
  }

  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0);  // รอ LoRa ปลุกผ่าน DIO0
  Serial.println("Going to sleep...");
  delay(100);
  esp_deep_sleep_start();
}

void loop() {
  // ไม่ใช้
}
