const int sensorPin = 27; // กำหนดขา Digital Pin ที่ต่อสายสัญญาณดำ

void setup() {
  pinMode(sensorPin, INPUT); // ตั้งค่าขาเซ็นเซอร์เป็น Input
  Serial.begin(115200); // เริ่มการสื่อสาร Serial Monitor
  Serial.println("LJ12A3-4-Z/BX Sensor Test");
}

void loop() {
  int sensorValue = digitalRead(sensorPin); // อ่านค่าจากขาเซ็นเซอร์

  if (sensorValue == LOW) { // ถ้าค่าเป็น LOW (ตรวจพบโลหะ)
    Serial.println("Object Detected!"); // แสดงข้อความ "ตรวจพบวัตถุ"
  } else { // ถ้าค่าเป็น HIGH (ไม่พบโลหะ)
    Serial.println("No Object"); // แสดงข้อความ "ไม่พบวัตถุ"
  }
  delay(500); // หน่วงเวลา 0.5 วินาที
}
