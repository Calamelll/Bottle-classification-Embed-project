
// ================= CONFIGURATION =================
// 1. ใส่รหัส Blynk ของคุณที่นี่
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Embeded Project"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// 2. ใส่รหัส WiFi
char ssid[] = "ชื่อ_WIFI";
char pass[] = "รหัส_WIFI";

// 3. ใส่ IP ของเครื่องคอมพิวเตอร์ที่รัน Server (ห้ามใช้ localhost)
// ตัวอย่าง: "http://192.168.1.50:5000/predict"
const char* aiServerUrl = "http://192.168.1.XXX:5000/predict";

// =================================================

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "esp_camera.h"
#include <HTTPClient.h>

// กำหนดขา Pin กล้อง (AI-THINKER Model)
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

BlynkTimer timer;

// ฟังก์ชันช่วยแกะค่าจาก JSON แบบบ้านๆ (String parsing)
// รองรับรูปแบบ: {"result": "plastic", "confidence": 0.98}
String extractJsonString(String json, String key) {
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = json.indexOf("\"", start);
  return json.substring(start, end);
}

float extractJsonFloat(String json, String key) {
  String pattern = "\"" + key + "\":"; // สังเกตว่าไม่มีฟันหนูเปิดหลัง colon สำหรับตัวเลข
  int start = json.indexOf(pattern);
  if (start < 0) return 0.0;
  start += pattern.length();
  
  // หาจุดสิ้นสุด (อาจเป็น , หรือ })
  int endComma = json.indexOf(",", start);
  int endBrace = json.indexOf("}", start);
  int end = -1;
  
  if (endComma > 0 && endBrace > 0) end = min(endComma, endBrace);
  else if (endComma > 0) end = endComma;
  else end = endBrace;
  
  if (end < 0) return 0.0;
  return json.substring(start, end).toFloat();
}

// ฟังก์ชันถ่ายรูปและส่งไป AI
void captureAndClassify() {
  // 1. ถ่ายรูป
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    Blynk.virtualWrite(V2, "Camera Error");
    return;
  }
  
  Blynk.virtualWrite(V2, "Sending...");

  // 2. ส่งไป Server
  HTTPClient http;
  http.begin(aiServerUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpCode = http.POST(fb->buf, fb->len);

  // 3. รับผลลัพธ์
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Server Response: " + payload);
    
    // ตัวอย่าง payload: { "result": "plastic", "confidence": 0.98 }
    
    String wasteType = extractJsonString(payload, "result");
    float confidence = extractJsonFloat(payload, "confidence");
    
    // แปลงความมั่นใจเป็น %
    int confPercent = (int)(confidence * 100);

    Serial.print("Type: "); Serial.println(wasteType);
    Serial.print("Conf: "); Serial.println(confPercent);

    // 4. ส่งขึ้น Blynk
    Blynk.virtualWrite(V0, wasteType);   // V0 = ประเภทขยะ
    Blynk.virtualWrite(V1, confPercent); // V1 = ความมั่นใจ %
    Blynk.virtualWrite(V2, "Done");      // V2 = สถานะ
    
    // --- ตรงนี้ไว้ใส่โค้ดสั่ง Servo Motor ---
    if (wasteType == "plastic") {
       // สั่ง Servo หมุนไปทางขยะพลาสติก
       Serial.println(">> Action: Plastic Bin");
    } else if (wasteType == "metal") {
       // สั่ง Servo หมุนไปทางขยะโลหะ
       Serial.println(">> Action: Metal Bin");
    }
    // ------------------------------------

  } else {
    Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    Blynk.virtualWrite(V2, "Server Error");
  }

  esp_camera_fb_return(fb); // คืนหน่วยความจำ
  http.end();
}

void setup() {
  Serial.begin(115200);
  
  // ตั้งค่ากล้อง
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
  
  config.frame_size = FRAMESIZE_QVGA; // 320x240 เล็กแต่เร็ว
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
    return;
  }

  // เชื่อมต่อ Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // ตั้งเวลาให้ทำงานทุกๆ 5 วินาที (ปรับได้)
  timer.setInterval(5000L, captureAndClassify);
  
  Serial.println("Waste Classifier Started!");
}

void loop() {
  Blynk.run();
  timer.run();
}
