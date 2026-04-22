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
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .card { background: #1e293b; padding: 40px; border-radius: 20px; box-shadow: 0 10px 40px rgba(0,0,0,0.6); text-align: center; border: 1px solid #334155; width: 90%; max-width: 400px; }
    h2 { margin-top: 0; font-size: 28px; font-weight: 800; background: -webkit-linear-gradient(#38bdf8, #818cf8); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: 1px; }
    .status-box { font-size: 22px; margin: 30px 0; padding: 15px; background: #0f172a; border-radius: 12px; border: 1px solid #475569; letter-spacing: 1px; }
    .btn { padding: 16px 40px; font-size: 18px; font-weight: bold; color: white; background: linear-gradient(135deg, #3b82f6, #6366f1); border: none; border-radius: 50px; cursor: pointer; transition: all 0.2s ease; box-shadow: 0 4px 15px rgba(99, 102, 241, 0.4); outline: none; }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(99, 102, 241, 0.6); }
    .btn:active { transform: translateY(1px); }
    .on-text { color: #ef4444; font-weight: 900; text-shadow: 0 0 12px rgba(239, 68, 68, 0.6); }
    .off-text { color: #22c55e; font-weight: 900; text-shadow: 0 0 12px rgba(34, 197, 94, 0.6); }
  </style>
</head>
<body>
  <div class="card">
    <h2>SYSTEM OVERRIDE</h2>
    <div class="status-box">STATUS: <span id="stat">%STATE%</span></div>
    <button class="btn" onclick="toggle()">TOGGLE POWER</button>
  </div>
  <script>
    // Fungsi buat update warna text otomatis sesuai status
    function updateColor() {
      let statEl = document.getElementById('stat');
      statEl.className = statEl.innerHTML.trim() === 'ON' ? 'on-text' : 'off-text';
    }
    
    function toggle() {
      fetch('/toggle').then(r => r.text()).then(d => { 
        document.getElementById('stat').innerHTML = d; 
        updateColor();
      });
    }
    
    // Jalanin pas web pertama kali dibuka
    window.onload = updateColor;
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
