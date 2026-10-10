#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h> // ต้องติดตั้ง Library: ESP32Servo
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// 1. ตั้งค่า WiFi และ Firebase (ต้องตรงกับ ESP32-CAM)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL" 

// กำหนด User/Pass (เหมือนตัวส่ง)
#define USER_EMAIL "YOUR_FIREBASE_USER_EMAIL"
#define USER_PASSWORD "YOUR_FIREBASE_USER_PASSWORD"

// 2. ตั้งค่า Servo
Servo myServo;
const int servoPin = 13; // ขาที่ต่อสายสัญญาณ Servo (GPIO 13)

// องศา Servo (ปรับตามกลไกจริง)
const int ANGLE_IDLE = 90;    // ตรงกลาง
const int ANGLE_GLASS = 45;   // สำหรับแก้ว
const int ANGLE_PLASTIC = 135;// สำหรับพลาสติก
// เพิ่มประเภทอื่นได้ถ้า AI ส่งมา เช่น "metal"

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String lastWasteType = ""; // ตัวแปรจำค่าล่าสุด

void setup() {
  Serial.begin(115200);
  
  // ต่อ WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  
  // ต่อ Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ตั้งค่า Servo
  myServo.attach(servoPin);
  myServo.write(ANGLE_IDLE); // เริ่มที่ตรงกลาง
  Serial.println("Servo Receiver Ready!");
}

void loop() {
  if (Firebase.ready()) {
    // อ่านค่าจาก Path "/current_trash" ที่เราเพิ่งเพิ่มในตัวส่ง
    if (Firebase.RTDB.getString(&fbdo, "/current_trash")) {
      String currentType = fbdo.stringData();

      // ตรวจสอบว่าค่าเปลี่ยนไปจากเดิมไหม และไม่ใช่ค่าว่าง
      if (currentType != "" && currentType != lastWasteType) {
        Serial.print("New Waste Detected: ");
        Serial.println(currentType);

        if (currentType == "glass") {
           Serial.println("Action: Sort Glass");
           myServo.write(ANGLE_GLASS);
           delay(3000); // ค้างไว้ 3 วินาที รอของตก
           myServo.write(ANGLE_IDLE); // กลับมาตรงกลาง
        } 
        else if (currentType == "plastic") {
           Serial.println("Action: Sort Plastic");
           myServo.write(ANGLE_PLASTIC);
           delay(3000); 
           myServo.write(ANGLE_IDLE);
        }
        else if (currentType == "metal") {
           // ใส่ Logic เพิ่มตรงนี้ถ้ามี metal
        }

        lastWasteType = currentType; // อัปเดตตัวแปรจำค่า
        
        // (Optional) สั่ง Reset ค่าใน Firebase เพื่อไม่ให้ค้าง
        // Firebase.RTDB.setString(&fbdo, "/current_trash", "");
        // lastWasteType = ""; 
      }
    } else {
      // กรณีอ่านไม่สำเร็จ (เช่นเน็ตหลุดชั่วคราว)
      // Serial.println(fbdo.errorReason());
    }
  }
  delay(200); // Loop check ทุก 0.2 วินาที
}
