// =============================================================
//  ESP32-CAM Waste Sorter [FIXED: Compilation Error]
// =============================================================

// --- 1. ต้องตั้งค่า Blynk ก่อนเรียก Library เสมอ (สำคัญมาก) ---
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Embeded Project"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// --- 2. ตั้งค่า Firebase ---
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL"

// --- 3. Libraries ---
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h> // ต้องอยู่ใต้ #define BLYNK_...
#include "esp_camera.h"
#include <HTTPClient.h>
#include "time.h"

// Firebase Libraries
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- 4. ตั้งค่า WiFi ---
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// --- 5. ตั้งค่า AI Server IP (เช็ค IP ปัจจุบันและอย่าลืม :5000) ---
const char* aiServerUrl = "http://172.20.10.3:5000/predict"; 

// =============================================================

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Camera Pins (AI-THINKER Model)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ตั้งค่าเวลา
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; 
const int daylightOffset_sec = 0;

// ================= HELPERS =================

void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Unknown Time";
  }
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

String extractJsonString(String json, String key) {
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = json.indexOf("\"", start);
  return json.substring(start, end);
}

float extractJsonFloat(String json, String key) {
  String pattern = "\"" + key + "\":";
  int start = json.indexOf(pattern);
  if (start < 0) return 0.0;
  start += pattern.length();
  int endComma = json.indexOf(",", start);
  int endBrace = json.indexOf("}", start);
  int end = -1;
  if (endComma > 0 && endBrace > 0) end = min(endComma, endBrace);
  else if (endComma > 0) end = endComma;
  else end = endBrace;
  return json.substring(start, end).toFloat();
}

// ================= TASK: AI & Firebase =================
// ฟังก์ชันนี้จะทำงานแยกขนานกับ Blynk เพื่อไม่ให้เมมโมรี่ชนกัน

void aiTask(void *pvParameters) {
  while (true) {
    // 1. หน่วงเวลา 5 วินาที (แทนการใช้ Timer)
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // เช็ค WiFi ก่อน
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, skipping...");
      continue;
    }

    // 2. ถ่ายรูป
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      Blynk.virtualWrite(V2, "Camera Fail");
      continue;
    }

    Blynk.virtualWrite(V2, "Scanning...");

    // 3. ส่งไป Server
    HTTPClient http;
    http.begin(aiServerUrl);
    http.addHeader("Content-Type", "image/jpeg");
    
    // ส่ง POST Request
    int httpCode = http.POST(fb->buf, fb->len);

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("AI Response: " + payload);
      
      String wasteType = extractJsonString(payload, "result");
      float confidence = extractJsonFloat(payload, "confidence");
      int confPercent = (int)(confidence * 100);

      Serial.printf("Detected: %s (%d%%)\n", wasteType.c_str(), confPercent);

      // --- อัปเดต Blynk ---
      Blynk.virtualWrite(V0, wasteType);
      Blynk.virtualWrite(V1, confPercent);
      Blynk.virtualWrite(V2, "Done");

      // --- ส่ง Firebase (เมื่อมั่นใจ > 50%) ---
      if (confPercent > 50 && wasteType != "") {
       if (Firebase.ready()) {
          // 1. ส่วนเดิม: เก็บ Log ประวัติ (Keep History)
          FirebaseJson json;
          json.set("type", wasteType);
          json.set("confidence", confPercent);
          json.set("timestamp", getTimestamp());
          Firebase.RTDB.pushJSON(&fbdo, "/waste_logs", &json); // เก็บลง Database

          // 2. >>> ส่วนที่ต้องเพิ่ม <<< : ส่งคำสั่งไปบอร์ด Servo (Real-time Command)
          // บอร์ดที่ 2 จะรออ่านค่าจาก Path "/current_trash" นี้
          Serial.print("Updating Status for Servo... ");
          if(Firebase.RTDB.setString(&fbdo, "/current_trash", wasteType)){
             Serial.println("OK");
          } else {
             Serial.println("Error");
          }
       }
    }

    } else {
      Serial.printf("HTTP Error: %d\n", httpCode);
      Blynk.virtualWrite(V2, "Server Error");
    }

    esp_camera_fb_return(fb); // คืน Memory
    http.end();
  }
}

// ================= SETUP =================

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; 
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
    return;
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. เชื่อมต่อ Blynk & WiFi
  // ฟังก์ชันนี้จะเรียก WiFi.begin ให้อัตโนมัติ
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // 2. ตั้งค่าระบบ
  setupTime();
  setupCamera();

  // 3. เริ่ม Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "YOUR_FIREBASE_USER_EMAIL"; 
  auth.user.password = "YOUR_FIREBASE_USER_PASSWORD";
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // 4. สร้าง Task แยกสำหรับการถ่ายรูป
  xTaskCreatePinnedToCore(
    aiTask,       // ฟังก์ชัน Task
    "AITask",     // ชื่อ Task
    20 * 1024,    // ขนาด Stack (20KB)
    NULL,         // Parameter
    1,            // Priority
    NULL,         // Handle
    1             // Core 1
  );

  Serial.println("System Started with Multithreading!");
}

void loop() {
  // Loop หลักทำงานแค่ Blynk อย่างเดียวเพื่อความเสถียร
  Blynk.run();
}
