#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "esp_sleep.h"

// กำหนดค่า Wi-Fi
const char* ssid = "iPhonePok";
const char* password = "12345678n";

// กำหนดค่าเวลา
#define SOIL_SENSOR_PIN 33
#define PUMP_PIN 26
#define pH_SENSOR 33

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // ใช้เวลา UTC, รีเฟรชทุกๆ 60 วินาที

int soilValue = 0;
int pHValue= 0;
const int dryThreshold = 3500;

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  // เชื่อมต่อกับ Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // เริ่มต้น NTPClient
  timeClient.begin();
  timeClient.setTimeOffset(7 * 3600); // ปรับเวลาเป็น GMT+7 (สำหรับแสดงผลใน Serial)

  // อัพเดตเวลา NTP
  timeClient.update();
  String currentTime = timeClient.getFormattedTime(); // ดึงเวลาปัจจุบันในรูปแบบ hh:mm:ss
  
  // แสดงเวลาใน serial monitor
  Serial.print("Current time: ");
  Serial.println(currentTime);
  delay(5000);

  soilValue = analogRead(SOIL_SENSOR_PIN);
  pHValue = analogRead(pH_SENSOR);

  Serial.println(pHValue);

  if (soilValue < dryThreshold) {
    digitalWrite(PUMP_PIN, HIGH); 
      digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
      Serial.println("Pump ON at the scheduled time");
      delay(5000);  // เปิดปั๊ม 1 ชั่วโมง
      digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊ม
      Serial.println("Pump OFF");
  } else {
    digitalWrite(PUMP_PIN, LOW); 
  }

  int firstColon = currentTime.indexOf(':');
  int secondColon = currentTime.indexOf(':', firstColon + 1);

  String minuteStr = currentTime.substring(firstColon + 1, secondColon);
  int minute = minuteStr.toInt();

  // เช็คว่าเวลาตรงกับ 8:00 AM หรือ 5:00 PM
  if (currentTime.startsWith("08") || currentTime.startsWith("17"))  {
    if(minute >= 0 && minute <= 30){ // เช็คเวลาหลักนาที 8:00 - 8:30 || 17:00 - 17:30
      digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
      Serial.println("Pump ON at the scheduled time");
      delay(5000);  // เปิดปั๊ม 1 ชั่วโมง
      digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊ม
      Serial.println("Pump OFF");
    }
  }
  // ตั้งเวลาให้ ESP32 ตื่นทุกๆ 1 ชั่วโมง (3600 วินาที)
  Serial.println("Wake up...");
  //esp_sleep_enable_timer_wakeup(3600 * 1000000);
  esp_sleep_enable_timer_wakeup(36 * 1000000);
  
  Serial.flush(); 
  Serial.println("Entering Deep Sleep...");

  // เข้าสู่โหมด deep sleep
  esp_deep_sleep_start();
}

void loop() {
  
}
