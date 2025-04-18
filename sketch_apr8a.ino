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

#define SS 9
#define RST 14
#define DIO0 26

int alreadySendLoRa = 0;

#define WAKE_PIN GPIO_NUM_4

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // ใช้เวลา UTC, รีเฟรชทุกๆ 60 วินาที

int soilValue = 0;
int pHValue= 0;
float temp = 0;
const int dryThreshold = 3500;

String command = "ON";

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

  LoRa.setPins(SS, RST, DIO0);
  Serial.println("LoRa ready");

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

  client.setServer(mqtt_server, port);
  client.setCallback(callback);
  client.loop();

  reconnect();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("EXT0 (GPIO4)"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("TIMER"); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED : Serial.println("Undefined"); break;
    default : Serial.println("Other"); break;
  }
  
  if (soilValue > dryThreshold) {
    digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
    Serial.println("Pump ON (IMMEDIATELY SOIL DRY > 3500)");
    delay(5000);  // เปิดปั๊ม 1 ชั่วโมง
    digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊ม
    Serial.println("Pump OFF");

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
      digitalWrite(PUMP_PIN, HIGH);  // เปิดปั๊ม
      Serial.println("Pump ON at the scheduled time");
      delay(5000);  // เปิดปั๊ม 1 ชั่วโมง
      digitalWrite(PUMP_PIN, LOW);   // ปิดปั๊ม
      Serial.println("Pump OFF");
    }
  }

  if(digitalRead(PUMP_PIN) == HIGH && alreadySendLoRa == 0){
    LoRa.beginPacket();
    LoRa.print(command);
    LoRa.endPacket();
    delay(1000);

    Serial.println("Sent: " + command);
    alreadySendLoRa = 1;
    delay(10000); // ส่งทุก 10 วินาที
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

  delay(100);
  // เข้าสู่โหมด deep sleep
  esp_deep_sleep_start();
}

void loop() {

}
