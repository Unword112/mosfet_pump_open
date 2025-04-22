#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "esp_sleep.h"
#include <LoRa.h>

// กำหนดค่า Wi-Fi
const char* ssid = "iPhonePok";
const char* password = "12345678n";

const char* mqtt_server = "demo.thingsboard.io";
const char* ID = "d71fa310-1751-11f0-b194-bf88b865708c";
const char* token = "5XayITvut6awxdSuZgtq";
const int port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// กำหนดค่าเวลา
#define SOIL_SENSOR_PIN 33
#define PUMP_PIN 26
#define pH_SENSOR 34

#define dH_SENSOR 32
#define DHTTYPE DHT22
DHT dht(dH_SENSOR, DHTTYPE);

bool alreadySendLoRa = false;

#define WAKE_PIN GPIO_NUM_4

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // ใช้เวลา UTC, รีเฟรชทุกๆ 60 วินาที

int soilValue = 0;
int pHValue= 0;
float temp = 0;
const int dryThreshold = 3500;

String commandON = "ON";
String commandOFF = "OFF";

void sendCommand() {
  String commandToSend;

  if (digitalRead(PUMP_PIN) == HIGH) {
    commandToSend = commandON;
  } else {
    commandToSend = commandOFF;
  }

  LoRa.beginPacket();
  LoRa.print(commandToSend);
  int result = LoRa.endPacket(); // ตรวจสอบผลลัพธ์การส่ง
  
  if (result == 0) {
    Serial.println("Failed to send command.");
    alreadySendLoRa = false;
  } else {
    Serial.print("Sent command: ");
    Serial.println(commandToSend);
    alreadySendLoRa = true;
  }
}
void reconnect() {
  while (!client.connected()) {
    if (client.connect(ID, token, "")) {
      client.subscribe("v1/devices/me/rpc/request/+"); // Subscribe RPC
    } else {
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("Received: " + message);

   if (message.indexOf("setPump") != -1) {
    if (message.indexOf("true") != -1) {
      digitalWrite(PUMP_PIN, LOW);
      Serial.println("Pump FORCE STOP");
    }
  }

}

void sendData(float temp, float pH, int soil) {
  String payload = "{\"temperature\": " + String(temp, 1) +
                   ", \"pH_Value\": " + String(pH, 1) +
                   ", \"Soil_Value\": " + String(soil) + "}";
  client.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.println("Sent to ThingsBoard: " + payload);
  delay(5000);
}

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(WAKE_PIN, INPUT_PULLUP);
  digitalWrite(PUMP_PIN, LOW);

  // เชื่อมต่อกับ Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  LoRa.setPins(5, 22, 25); // NSS, RESET, DIO0
  if (!LoRa.begin(433E6)) { // 433 MHz (ปรับตามโมดูลของคุณ)
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa initialized.");

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

  dht.begin();
  temp = dht.readTemperature();

  soilValue = analogRead(SOIL_SENSOR_PIN);
  pHValue = analogRead(pH_SENSOR);

  Serial.println(soilValue);

  delay(1000);
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0); // ปลุกเมื่อ GPIO4 = 0 (LOW)
  Serial.println(WAKE_PIN);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("EXT0 (GPIO4)"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("TIMER"); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED : Serial.println("Undefined"); break;
    default : Serial.println("Other"); break;
  }

  client.setServer(mqtt_server, port);
  client.setCallback(callback);

  if (!client.connected()) {
    reconnect();
    Serial.println("MQTT not connected, reconnecting...");
  } else {
    client.loop();
    Serial.println("MQTT connected");
  }
  
  if (soilValue > dryThreshold) {
    digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
    Serial.println("Pump ON (IMMEDIATELY SOIL DRY > 3500)");

    unsigned long pumpStart = millis();         // จับเวลาตอนเปิดปั๊ม
    unsigned long pumpDuration = 10000;         // ตั้งเวลาให้ปั๊มทำงาน 10 วินาที

    while (millis() - pumpStart < pumpDuration) {
      client.loop();  // ให้รับ RPC ได้ตลอดระยะที่ปั๊มทำงาน

      // ตรวจสอบการเชื่อมต่อ MQTT
      if (!client.connected()) {
        reconnect();
      }

      // ตรวจสอบว่ามีการสั่งปิดผ่าน RPC หรือไม่
      if (digitalRead(PUMP_PIN) == LOW) {
        Serial.println("Pump forced OFF via RPC");
        break;
      }

      delay(10); // ลดภาระ CPU
    }

    digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊มเมื่อครบเวลา หรือถูกสั่งหยุด
    Serial.println("Pump OFF");
    alreadySendLoRa = false;

    delay(5000);
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
      if (soilValue > dryThreshold){
        digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
        Serial.println("Pump ON at the scheduled time");
        delay(5000);  // เปิดปั๊ม 1 ชั่วโมง
        digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊ม
        Serial.println("Pump OFF");
      }
    }
  }

  int soil_percent = map(soilValue, 0, 4095, 100, 1);
  int pH_percent = map(pHValue, 0, 4095, 0, 14);
  sendData(temp, pH_percent, soil_percent);

  // ตั้งเวลาให้ ESP32 ตื่นทุกๆ 1 ชั่วโมง (3600 วินาที)
  Serial.println("Wake up...");
  //esp_sleep_enable_timer_wakeup(3600 * 1000000);
  esp_sleep_enable_timer_wakeup(36 * 1000000);
  
  Serial.flush(); 
  Serial.println("Entering Deep Sleep...");

  delay(10000);
  // เข้าสู่โหมด deep sleep
  esp_deep_sleep_start();
}

void loop() {

}
