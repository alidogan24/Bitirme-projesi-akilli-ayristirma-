import os
import cv2
import numpy as np
from ultralytics import YOLO
from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)

CAPTURE_DIR = "captured"
OUTPUT_DIR = "outputs"
os.makedirs(CAPTURE_DIR, exist_ok=True)
os.makedirs(OUTPUT_DIR, exist_ok=True)

model = YOLO("best.pt")

# ===== GLOBAL DEĞİŞKENLER =====
esp_command = "IDLE"
last_results = []             # Görselde gösterilen tüm sonuçlar
last_results_filtered = []    # ESP32'ye gidecek sadeleştirilmiş sonuç


# ================= ESP32-CAM =================
@app.route("/get_message", methods=["GET"])
def get_message():
    global esp_command
    cmd = esp_command
    esp_command = "IDLE"
    return cmd, 200


@app.route("/upload", methods=["POST"])
def upload_image():
    global last_results, last_results_filtered

    img_bytes = request.data
    if not img_bytes:
        return "NO_IMAGE", 400

    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    raw_path = f"{CAPTURE_DIR}/{ts}.jpg"
    out_path = f"{OUTPUT_DIR}/{ts}.jpg"

    img = cv2.imdecode(np.frombuffer(img_bytes, np.uint8), cv2.IMREAD_COLOR)
    cv2.imwrite(raw_path, img)

    last_results, last_results_filtered = run_yolo(img, out_path)

    return "OK", 200


def run_yolo(image, out_path):
    results = model.predict(image, conf=0.5, imgsz=640)

    detections = []        # metal(200,500) gibi ham sonuçlar
    filtered_results = [] # metal(500) gibi sade sonuçlar

    valid_candidates = []  # (label, cy)

    for r in results:
        boxes = r.obb.xyxyxyxy.cpu().numpy()
        classes = r.obb.cls.cpu().numpy().astype(int)

        for box, cid in zip(boxes, classes):
            pts = np.array(box, dtype=np.int32).reshape((-1, 2))
            cx, cy = int(np.mean(pts[:, 0])), int(np.mean(pts[:, 1]))
            name = model.names[cid]

            label = f"{name}({cx},{cy})"
            detections.append(label)

            # 0–560 arasıysa aday olarak kaydet
            if 0 <= cy <= 560:
                valid_candidates.append((name, cy))

            # Görsel çizimler
            cv2.polylines(image, [pts], True, (0, 255, 0), 3)
            cv2.putText(image, label, (pts[0][0], pts[0][1] - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

    cv2.imwrite(out_path, image)

    # ===== ESP32'ye gönderilecek tek sonuç =====
    if len(valid_candidates) > 0:
        name, cy = valid_candidates[0]
        filtered_results.append(f"{name}({cy})")

    print("📍 YOLO HAM SONUÇLARI:")
    for d in detections:
        print(" ", d)

    print("📤 ESP'ye gidecek:")
    for f in filtered_results:
        print(" ", f)

    return detections, filtered_results


# ================= NodeMCU =================
@app.route("/trigger", methods=["GET"])
def trigger():
    global esp_command, last_results, last_results_filtered
    last_results = []
    last_results_filtered = []
    esp_command = "CAPTURE"
    print("🎯 NodeMCU → CAPTURE tetiklendi")
    return "CAPTURE_SENT", 200


@app.route("/result", methods=["GET"])
def result():
    return jsonify(last_results_filtered), 200


if __name__ == "__main__":
    print("🚀 Python Server hazır")
    app.run(host="0.0.0.0", port=5000)
