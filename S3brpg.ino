#include <WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <Adafruit_NeoPixel.h>

// Pin RGB bawaan ESP32-S3 biasanya di GPIO 48 atau 38 (sesuaikan sama board lo)
#define LED_PIN     48 
#define NUM_PIXELS  1

Adafruit_NeoPixel rgb(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

const char* ssid_rumah = "AYYUBI";
const char* password_rumah = "akukoklupa";
const char* ssid_esp32 = "MUSANGANJAY";
const char* password_esp32 = "bosmuda00";

bool lastStatus = false;

void blinkRGB(uint32_t color, int times) {
  for (int i = 0; i < times; i++) {
    rgb.setPixelColor(0, color);
    rgb.show();
    delay(150); // Kedip cepet
    rgb.setPixelColor(0, rgb.Color(0, 0, 0)); // Mati
    rgb.show();
    delay(150);
  }
}

void setup() {
  Serial.begin(115200);
  rgb.begin();
  rgb.setBrightness(50); // Biar gak silau banget
  rgb.show(); // Pastiin mati pas awal

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid_rumah, password_rumah);
  
  Serial.print("Menghubungkan...");
  
  // Nunggu konek pertama kali
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nKonek!");
  blinkRGB(rgb.Color(0, 255, 0), 3); // Kedip Ijo 3x
  lastStatus = true;

  WiFi.softAP(ssid_esp32, password_esp32);
  
  ip_napt_init(8, 64);
  ip_napt_enable_no(SOFTAP_IF, 1);
}

void loop() {
  bool currentStatus = (WiFi.status() == WL_CONNECTED);

  // Jika status berubah dari konek ke putus
  if (lastStatus == true && currentStatus == false) {
    blinkRGB(rgb.Color(255, 0, 0), 3); // Kedip Merah 3x
    lastStatus = false;
  } 
  // Jika status berubah dari putus ke konek lagi (reconnect)
  else if (lastStatus == false && currentStatus == true) {
    blinkRGB(rgb.Color(0, 255, 0), 3); // Kedip Ijo 3x
    lastStatus = true;
  }

  delay(1000); // Cek status tiap detik biar gak berat
}
