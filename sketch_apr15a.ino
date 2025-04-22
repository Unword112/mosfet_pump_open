#include <LoRa.h>

#define SS 5
#define RST 22
#define DIO0 26
#define PUMP_PIN 32
#define WAKE_PIN GPIO_NUM_4
#define SOIL_SENSOR_PIN 34

int soilValue = 0;
const int dryThreshold = 3500;

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  digitalWrite(PUMP_PIN, LOW);

  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0);  // รอ LoRa ปลุกผ่าน DIO0

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

  Serial.println("Waiting for LoRa packet...");

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();  // อ่านข้อความทั้งหมด
    }
    int command = msg.toInt();
    Serial.println("Received: " + String(command));

    if (command == 1) {
        if (soilValue > dryThreshold) {
          digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
          Serial.println("Pump ON FOLLOW OPEN BY MASTER");

          unsigned long pumpStart = millis();         // จับเวลาตอนเปิดปั๊ม
          unsigned long pumpDuration = 10000;         // ตั้งเวลาให้ปั๊มทำงาน 10 วินาที

          while (millis() - pumpStart < pumpDuration) {
            // ตรวจสอบว่ามีการสั่งปิดผ่าน RPC หรือไม่
            if (digitalRead(PUMP_PIN) == LOW) {
              Serial.println("Pump forced OFF via RPC");
              break;
            }

            delay(10);
          }

          digitalWrite(PUMP_PIN, LOW);  
          Serial.println("Pump OFF");

          delay(2000);
        } else {
            digitalWrite(PUMP_PIN, LOW);
            Serial.println("Soil is not dry, deny ON command.");
          }
        } else if (command == 0) {
            digitalWrite(PUMP_PIN, LOW);
            Serial.println("Pump OFF by Master.");
        }
    }
      Serial.println("Wake up...");

  // ตั้งเวลาให้ ESP32 ตื่น (3600 วินาที คือ 1 ชั่วโมง)
  //esp_sleep_enable_timer_wakeup(3600 * 1000000)
  esp_sleep_enable_timer_wakeup(36 * 1000000);  

  Serial.println("Entering Deep Sleep...");
  LoRa.receive();  // ตั้งให้ LoRa เข้าสู่โหมดรับ ก่อนเข้าสู่ deep sleep
  Serial.flush();  // รอให้ Serial พิมพ์ข้อความทั้งหมดก่อน
  delay(10000);    // หน่วงเวลาเล็กน้อยเพื่อความชัวร์
  esp_deep_sleep_start();  // เข้าสู่ deep sleep
}


void loop() {
  // ไม่ใช้
}
