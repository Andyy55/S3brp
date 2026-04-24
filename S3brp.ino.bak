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

// Definisi pin LED RGB bawaan ESP32-S3 (Biasanya GPIO 48)
#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

const char* ssid = "P-DTNR";
const char* password = "andyanjir123";

const int relayPin = 35; // Sesuaikan GPIO-mu
bool relayStatus = false; 

AsyncWebServer server(80);

// Variabel untuk non-blocking LED blink
unsigned long previousMillis = 0;
bool ledState = false;

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

// UI HTML + CSS Super Keren (Cyberpunk / Modern Card Style)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>COMMAND CENTER</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; background: #020617; color: white; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .card { background: #1e293b; padding: 50px; border-radius: 25px; text-align: center; border: 2px solid #38bdf8; width: 300px; box-shadow: 0 0 20px rgba(56, 189, 248, 0.3); }
    h2 { color: #38bdf8; letter-spacing: 2px; margin-bottom: 30px; }
    #stat { font-size: 40px; display: block; margin-bottom: 30px; font-weight: bold; }
    .btn { width: 100%; padding: 20px; font-size: 20px; font-weight: bold; color: white; background: #2563eb; border: none; border-radius: 15px; cursor: pointer; box-shadow: 0 5px 15px rgba(0,0,0,0.3); }
    .on-text { color: #f43f5e; text-shadow: 0 0 15px #f43f5e; }
    .off-text { color: #10b981; text-shadow: 0 0 15px #10b981; }
  </style>
</head>
<body>
  <div class="card">
    <h2>CORE CONTROL</h2>
    <span id="stat">LOADING...</span>
    <button class="btn" onclick="toggle()">FIRE TOGGLE</button>
  </div>
  <script>
    function updateStatus(status) {
      const el = document.getElementById('stat');
      el.innerHTML = status;
      el.className = (status === 'ON') ? 'on-text' : 'off-text';
    }
    // Ambil status awal pas web dibuka
    fetch('/status').then(r => r.text()).then(updateStatus);
    
    function toggle() {
      fetch('/toggle').then(r => r.text()).then(updateStatus);
    }
  </script>
</body>
</html>)rawliteral";


void setup() {
  Serial.begin(115200);

  // Matikan LED RGB di awal biar bersih
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);

  // Awal booting langsung paksa MATI (High Impedance)
  controlRelay(false);

  WiFi.softAP(ssid, password); 
  Serial.print("Access Point Berhasil! IP: ");
  Serial.println(WiFi.softAPIP());
  
  

    // Route Halaman Utama (Tanpa Processor biar gak error blank)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Route buat ngambil status awal
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", relayStatus ? "ON" : "OFF");
  });

  // Route buat toggle
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    controlRelay(!relayStatus);
    request->send(200, "text/plain", relayStatus ? "ON" : "OFF");
  });


  server.begin();
}

void loop() {
     unsigned long currentMillis = millis();

  if (!relayStatus) {
    // STATE: RELAY OFF 
    // Efek: Hijau nyala sekilas (100ms) "Dip/Lap", terus mati jeda 2.5 detik
    if (ledState && (currentMillis - previousMillis >= 100)) { 
      previousMillis = currentMillis;
      ledState = false;
      neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Matikan lampu
    } else if (!ledState && (currentMillis - previousMillis >= 1500)) {
      previousMillis = currentMillis;
      ledState = true;
      neopixelWrite(RGB_BUILTIN, 0, 205, 0); // Nyalakan Hijau sekilas
    }
  } else {
    // STATE: RELAY ON 
    // Efek: Merah nyala sekilas (100ms) "Dip/Lap", terus mati jeda cepet (800ms)
    if (ledState && (currentMillis - previousMillis >= 100)) {
      previousMillis = currentMillis;
      ledState = false;
      neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Matikan lampu
    } else if (!ledState && (currentMillis - previousMillis >= 300)) {
      // Jedanya 800ms biar cepet tapi tetep kelihatan "Dip" nya, bukan nyala terus
      previousMillis = currentMillis;
      ledState = true;
      neopixelWrite(RGB_BUILTIN, 250, 0, 0); // Nyalakan Merah sekilas
    }
  }
}
