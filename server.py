from transformers import AutoImageProcessor, AutoModelForImageClassification
from PIL import Image
from io import BytesIO
import torch
from flask import Flask, request, jsonify

# 1. เปลี่ยนโมเดลเป็น Waste Classifier
MODEL_NAME = "rootstrap-org/waste-classifier"

print(f"Loading model: {MODEL_NAME} ... (ครั้งแรกจะดาวน์โหลดโมเดล อาจใช้เวลา)")
# ใช้ AutoModel แทน MobileNetV2 เพื่อความยืดหยุ่น (เผื่อโมเดลเป็น ResNet หรือตัวอื่น)
processor = AutoImageProcessor.from_pretrained(MODEL_NAME)
model = AutoModelForImageClassification.from_pretrained(MODEL_NAME)
model.eval()  # โหมด inference

app = Flask(__name__)

def classify_image_bytes(image_bytes: bytes):
    """รับ bytes ของรูป → แปลงเป็นรูป → ส่งเข้าโมเดล → คืนประเภทวัสดุ"""
    try:
        # เปิดรูปและแปลงเป็น RGB
        img = Image.open(BytesIO(image_bytes)).convert("RGB")
        
        # Preprocess รูปให้เข้ากับโมเดล
        inputs = processor(images=img, return_tensors="pt")

        with torch.no_grad():
            outputs = model(**inputs)
            logits = outputs.logits
            pred_id = logits.argmax(-1).item()
            
            # ดึงชื่อ Label (เช่น plastic, metal, glass, cardboard)
            label = model.config.id2label[pred_id]
            
            # ดึงค่าความมั่นใจ (Confidence Score)
            probs = torch.nn.functional.softmax(logits, dim=-1)
            confidence = probs[0][pred_id].item()

        return label, confidence
    except Exception as e:
        print(f"Error processing image: {e}")
        return "error", 0.0

@app.route("/", methods=["GET"])
def index():
    return "Waste Classifier AI server is running"

@app.route("/predict", methods=["POST"])
def predict():
    """รับรูป JPEG ดิบจาก ESP32 → วิเคราะห์ขยะ → ส่งผลกลับ"""
    if not request.data:
        return jsonify({"error": "no image data"}), 400

    image_bytes = request.data
    label, confidence = classify_image_bytes(image_bytes)

    print(f"Detected: {label} ({confidence:.2f})")

    # ส่ง JSON กลับไปให้ ESP32
    return jsonify({
        "result": label,        # ผลลัพธ์: plastic, metal, glass, etc.
        "confidence": confidence # ความมั่นใจ (0.0 - 1.0)
    })

if __name__ == "__main__":
    # host = 0.0.0.0 เพื่อให้ ESP32 ในวง LAN มองเห็น
    app.run(host="0.0.0.0", port=5000)