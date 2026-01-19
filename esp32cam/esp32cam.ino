#include <WiFi.h>
#include <HTTPClient.h>
#include <esp32cam.h>

const char* WIFI_SSID = "OPPO Reno5 Lite";
const char* WIFI_PASS = "madengineer";

const char* SERVER = "http://10.249.144.42:5000";

// Çözünürlük
static auto hiRes = esp32cam::Resolution::find(1024, 768);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== ESP32-CAM BAŞLADI ===");

  // Kamera kurulumu
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "📸 Kamera hazır" : "❌ Kamera hatası");

    if (!ok) while (true);
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("📡 WiFi bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi bağlı");
  Serial.print("📍 ESP32 IP: ");
  Serial.println(WiFi.localIP());

  registerToServer();
}

void loop() {
  checkServerCommand();
  delay(3000);
}

void registerToServer() {
  HTTPClient http;
  http.begin(String(SERVER) + "/register");
  int code = http.POST("");
  Serial.print("🔗 Register cevap: ");
  Serial.println(code);
  http.end();
}

void checkServerCommand() {
  HTTPClient http;
  http.begin(String(SERVER) + "/get_message");

  int code = http.GET();
  if (code == 200) {
    String cmd = http.getString();
    Serial.println("📨 Komut: " + cmd);

    if (cmd == "CAPTURE") {
      captureAndUpload();
    }
  }

  http.end();
}


void captureAndUpload() {
  Serial.println("📸 Foto çekiliyor...");

  auto frame = esp32cam::capture();
  if (!frame) {
    Serial.println("❌ Foto çekilemedi");
    return;
  }

  Serial.printf("📦 Foto boyutu: %d byte\n", frame->size());

  HTTPClient http;
  http.begin(String(SERVER) + "/upload");
  http.addHeader("Content-Type", "image/jpeg");

  int code = http.POST(frame->data(), frame->size());
  Serial.print("⬅️ Upload cevap: ");
  Serial.println(code);

  http.end();
}