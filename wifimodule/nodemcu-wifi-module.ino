#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "OPPO Reno5 Lite";
const char* password = "madengineer";

const char* TRIGGER_URL = "http://10.249.144.42:5000/trigger";
const char* RESULT_URL  = "http://10.249.144.42:5000/result";

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  // Başlangıçta STM'ye hazır bilgisi ver
  Serial.println("SISTEM_HAZIR");
}

void loop() {
  // STM32'den herhangi bir veri gelirse tetiklen
  if (Serial.available() > 0) {
    // Tamponu temizle
    while(Serial.available() > 0) { Serial.read(); delay(1); }

    // 1. ADIM: TETİK GÖNDER
    if (sendTrigger()) {
      Serial.println("capture yollandi"); // STM32'ye haber ver
      
      // 2. ADIM: SONUCU BEKLE VE AL
      // ESP32-CAM'in fotoğraf çekip yüklemesi vakit alır, bu yüzden döngüye giriyoruz
      waitForResult();
    }
  }
}

bool sendTrigger() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, TRIGGER_URL);
  int httpCode = http.GET();
  http.end();
  return (httpCode == 200);
}

void waitForResult() {
  WiFiClient client;
  HTTPClient http;
  String payload = "";
  int attempts = 0;

  // En fazla 10 saniye boyunca sonucu kovala (Her saniye bir kontrol)
  while (attempts < 10) {
    delay(1000); 
    if (http.begin(client, RESULT_URL)) {
      int httpCode = http.GET();
      if (httpCode == 200) {
        payload = http.getString();
        
        // Eğer sunucu boş liste dönmüyorsa (last_results doluysa)
        // JSON formatı "[]" değilse veriyi STM'ye bas
        if (payload != "[]" && payload != "") {
          // JSON karakterlerini temizlemek için basit bir işlem (İsteğe bağlı)
          payload.replace("[", "");
          payload.replace("]", "");
          payload.replace("\"", "");
          
          Serial.println(payload); // Örn: "label(120,80), label2(200,150)"
          http.end();
          return; 
        }
      }
      http.end();
    }
    attempts++;
  }
  Serial.println("TIMEOUT: Sonuc alinamadi");
}