#if __has_include(<esp_rom_md5.h>)
  #include <esp_rom_md5.h>
  #ifndef _MD5_H_
    #define _MD5_H_
    // Mapping fungsi lama ke fungsi baru di Core v3
    #define MD5Init esp_rom_md5_init
    #define MD5Update esp_rom_md5_update
    #define MD5Final esp_rom_md5_final
    typedef md5_context_t MD5_CTX;
  #endif
#endif

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "P-DTNR";
const char* password = "andyanjir123";

const int relayPin = 35; // Sesuaikan GPIO-mu
bool relayStatus = false; 

AsyncWebServer server(80);

// Fungsi Sakti buat mati-nyalain Relay Active Low di ESP32
void controlRelay(bool nyalakan) {
  if (nyalakan) {
    // Kalo mau NYALA: Set ke OUTPUT dan kasih LOW
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);
    relayStatus = true;
    Serial.println("Relay: ON (Logic LOW)");
  } else {
    // Kalo mau MATI: Set ke INPUT (High Impedance)
    // Ini trik supaya pin 'ngambang' dan gak narik arus, relay pasti mati total
    pinMode(relayPin, INPUT); 
    relayStatus = false;
    Serial.println("Relay: OFF (High Impedance)");
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>TESTING PROJECT</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: white; }
    .btn { padding: 15px 30px; font-size: 20px; border-radius: 50px; border: none; cursor: pointer; transition: 0.3s; }
    .on { background: #ff4757; box-shadow: 0 0 15px #ff4757; }
    .off { background: #2ed573; box-shadow: 0 0 15px #2ed573; }
  </style>
</head>
<body>
  <h2>ESP32-S3 System</h2>
  <p>Status: <strong id="stat">%STATE%</strong></p>
  <button class="btn" onclick="toggle()">PRESS TOGGLE</button>
  <script>
    function toggle() {
      fetch('/toggle').then(r => r.text()).then(d => { document.getElementById('stat').innerHTML = d; });
    }
  </script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  // Awal booting langsung paksa MATI (High Impedance)
  controlRelay(false);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  Serial.println("\nGas! IP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, [](const String& var){
      if(var == "STATE") return relayStatus ? String("ON") : String("OFF");
      return String();
    });
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    controlRelay(!relayStatus);
    request->send(200, "text/plain", relayStatus ? "ON" : "OFF");
  });

  server.begin();
}

void loop() {}
