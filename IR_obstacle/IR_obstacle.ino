// กำหนดขาเซนเซอร์
const int irSensorPin = 23; 

void setup() {
  Serial.begin(115200);
  
  // กำหนดขาเป็น Input (โมดูลนี้จ่ายไฟออกมาเองได้ ไม่ต้องใช้ PULLUP ก็ได้)
  pinMode(irSensorPin, INPUT); 
  
  Serial.println("IR Obstacle Sensor Test Started...");
}

void loop() {
  int sensorStatus = digitalRead(irSensorPin); // อ่านค่าสถานะ

  if (sensorStatus == LOW) { 
    // ค่า LOW คือ "เจอสิ่งกีดขวาง" (Active Low)
    Serial.println("Object Detected! (เจอวัตถุ)");
  } 
  else {
    // ค่า HIGH คือ "ทางสะดวก"
    Serial.println("Clear path (ไม่เจอวัตถุ)");
  }
  
  delay(200); // หน่วงเวลาสั้นๆ เพื่อให้อ่านค่าทัน
}
