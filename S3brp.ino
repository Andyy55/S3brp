#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

/* --- KONFIGURASI GLOBAL --- */
Preferences pref;
AsyncWebServer server(80);
Adafruit_NeoPixel *pixels = NULL;
IRrecv *irrecv = NULL;
IRsend *irsend = NULL;
decode_results results;

// Variabel Settings (Simpan di NVS)
String cfg_ssid, cfg_pass;
int pin_relay, pin_irx, pin_itx, pin_sdcs, pin_led;
bool relayState = false;
int ledMode = 0; // 0:Off, 1:Static, 2:Strobe
uint32_t ledColor = 0xFF0000; 
unsigned long lastStrobe = 0;
bool strobeState = false;

// Buffer IR Raw (Buat Clone Remote AC/TV)
uint16_t rawData[400];
uint16_t rawLength = 0;

/* --- UI DASHBOARD (HTML + CSS + JS) --- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ROGUE S3 ULTIMATE</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { background:#050505; color:#00ff41; font-family:monospace; text-align:center; padding:10px; }
        .box { border:1px solid #00ff41; padding:15px; margin:10px auto; max-width:500px; background:#111; box-shadow:0 0 10px #00ff41; }
        .grid { display:grid; grid-template-columns: 1fr 1fr; gap:10px; }
        button { background:#00ff41; color:#000; border:none; padding:12px; font-weight:bold; cursor:pointer; width:100%; border-radius:3px; margin:5px 0; }
        button:active { background:#008f11; }
        input, select { background:#000; border:1px solid #00ff41; color:#00ff41; padding:10px; width:90%; margin:5px 0; }
        .danger { background:#ff0000 !important; color:#fff !important; }
        h2 { border-bottom:2px solid #00ff41; padding-bottom:10px; text-shadow:0 0 5px #00ff41; }
        label { font-size:0.8em; color:#888; display:block; text-align:left; margin-left:5%; }
    </style>
</head>
<body>
    <div class="box">
        <h2>[ ROGUE S3 COMMAND ]</h2>
        
        <div class="card">
            <h3>RELAY & LED</h3>
            <button onclick="fetch('/relay')">TOGGLE RELAY 5V</button>
            <input type="color" onchange="setRGB(this.value)" id="colorPicker">
            <div class="grid">
                <button onclick="fetch('/led/mode?m=2')">STROBO</button>
                <button onclick="fetch('/led/mode?m=0')" class="danger">LED OFF</button>
            </div>
        </div>

        <div class="card">
            <h3>IR CLONER (SD CARD)</h3>
            <div class="grid">
                <button onclick="fetch('/ir/record')">REKAM REMOTE</button>
                <button onclick="fetch('/ir/send')">TEMBAK DARI SD</button>
            </div>
            <small>Status: <span id="irStat">Idle</span></small>
        </div>

        <div class="card">
            <h3>WIFI ATTACK</h3>
            <button class="danger" onclick="fetch('/deauth')">START DEAUTH ATTACK</button>
        </div>

        <div class="card">
            <h3>HARDWARE CONFIG</h3>
            <form action="/save" method="GET">
                <label>WiFi SSID & Password</label>
                <input name="s" placeholder="New SSID">
                <input name="p" placeholder="New Password">
                
                <label>Pin Configuration (GPIO)</label>
                <div class="grid">
                    <input name="pr" type="number" placeholder="Relay Pin">
                    <input name="irx" type="number" placeholder="IR RX Pin">
                    <input name="itx" type="number" placeholder="IR TX Pin">
                    <input name="sc" type="number" placeholder="SD CS Pin">
                    <input name="lp" type="number" placeholder="RGB LED Pin">
                </div>
                <button type="submit" style="background:#444; color:#fff;">SIMPAN & REBOOT</button>
            </form>
        </div>
    </div>
    <script>
        function setRGB(h){
            let r=parseInt(h.slice(1,3),16), g=parseInt(h.slice(3,5),16), b=parseInt(h.slice(5,7),16);
            fetch(`/led/set?r=${r}&g=${g}&b=${b}`);
        }
    </script>
</body>
</html>
)rawliteral";

/* --- FUNGSI SD CARD --- */
void saveIR() {
    File f = SD.open("/rogue.bin", FILE_WRITE);
    if(f) {
        f.write((uint8_t*)&rawLength, sizeof(rawLength));
        f.write((uint8_t*)rawData, rawLength * sizeof(uint16_t));
        f.close();
    }
}

void loadIR() {
    File f = SD.open("/rogue.bin", FILE_READ);
    if(f) {
        f.read((uint8_t*)&rawLength, sizeof(rawLength));
        f.read((uint8_t*)rawData, rawLength * sizeof(uint16_t));
        f.close();
    }
}

// LANJUT KE PART 2 (SETUP, HANDLERS, LOOP)
/* --- LOGIKA SETUP (INISIALISASI SEMUA MODUL) --- */
void setup() {
    Serial.begin(115200);
    
    // Buka memori NVS
    pref.begin("rogue_v2", false);
    cfg_ssid = pref.getString("s", "ROGUE_S3_AP");
    cfg_pass = pref.getString("p", "mercon123");
    pin_relay = pref.getInt("pr", 1);  // G1
    pin_irx = pref.getInt("irx", 2);   // G2
    pin_itx = pref.getInt("itx", 3);   // G3
    pin_sdcs = pref.getInt("sc", 10);  // G10
    pin_led = pref.getInt("lp", 48);   // G48

    // Init LED RGB
    pixels = new Adafruit_NeoPixel(1, pin_led, NEO_GRB + NEO_KHZ800);
    pixels->begin();
    pixels->show();

    // Init Relay
    pinMode(pin_relay, OUTPUT);
    digitalWrite(pin_relay, HIGH); // Off

    // Init SD Card
    if (!SD.begin(pin_sdcs)) {
        Serial.println("SD Gagal/Gaada!");
    } else {
        Serial.println("SD OK!");
        loadIR(); // Load data IR terakhir dari SD
    }

    // Init IR
    irrecv = new IRrecv(pin_irx);
    irsend = new IRsend(pin_itx);
    irrecv->enableIRIn();
    irsend->begin();

    // WiFi AP Mode
    WiFi.softAP(cfg_ssid.c_str(), cfg_pass.c_str());
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());

    /* --- SERVER HANDLERS --- */
    
    // Main Page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    // Relay Toggle
    server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request){
        relayState = !relayState;
        digitalWrite(pin_relay, relayState ? LOW : HIGH);
        request->send(200, "text/plain", relayState ? "NYALA" : "MATI");
    });

    // LED Color & Mode
    server.on("/led/set", HTTP_GET, [](AsyncWebServerRequest *request){
        int r = request->arg("r").toInt();
        int g = request->arg("g").toInt();
        int b = request->arg("b").toInt();
        ledColor = pixels->Color(r, g, b);
        ledMode = 1; // Static
        request->send(200, "text/plain", "Warna Set!");
    });

    server.on("/led/mode", HTTP_GET, [](AsyncWebServerRequest *request){
        ledMode = request->arg("m").toInt();
        request->send(200, "text/plain", "Mode Diganti");
    });

    // IR Cloner
    server.on("/ir/record", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "REKAM AKTIF! ARAHKAN REMOT...");
        // Logika rekam ada di loop
    });

    server.on("/ir/send", HTTP_GET, [](AsyncWebServerRequest *request){
        if (rawLength > 0) {
            irsend->sendRaw(rawData, rawLength, 38);
            request->send(200, "text/plain", "TEMBAK BERHASIL!");
        } else {
            request->send(200, "text/plain", "DATA KOSONG/SD ERROR");
        }
    });

    // Deauth (Logic Placeholder - Gabungin sama Library ESP8266/ESP32 Deauther manual)
    server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "DEAUTH BERJALAN (Cek Serial)");
        Serial.println("Warning: Menjalankan Deauth Broadcast...");
        // Tambahkan fungsi deauth lo di sini
    });

    // Save Settings
    server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasArg("s")) pref.putString("s", request->arg("s"));
        if(request->hasArg("p")) pref.putString("p", request->arg("p"));
        if(request->hasArg("pr")) pref.putInt("pr", request->arg("pr").toInt());
        if(request->hasArg("irx")) pref.putInt("irx", request->arg("irx").toInt());
        if(request->hasArg("itx")) pref.putInt("itx", request->arg("itx").toInt());
        if(request->hasArg("sc")) pref.putInt("sc", request->arg("sc").toInt());
        if(request->hasArg("lp")) pref.putInt("lp", request->arg("lp").toInt());
        request->send(200, "text/plain", "OK! REBOOTING...");
        delay(2000);
        ESP.restart();
    });

    server.begin();
}

/* --- LOOP UTAMA (BACKGROUND TASK) --- */
void loop() {
    // 1. Logika LED (Static / Strobo)
    if (ledMode == 0) {
        pixels->setPixelColor(0, 0);
        pixels->show();
    } else if (ledMode == 1) {
        pixels->setPixelColor(0, ledColor);
        pixels->show();
    } else if (ledMode == 2) { // Mode Strobo Polisi
        if (millis() - lastStrobe > 50) {
            strobeState = !strobeState;
            pixels->setPixelColor(0, strobeState ? ledColor : 0);
            pixels->show();
            lastStrobe = millis();
        }
    }

    // 2. Logika Rekam IR
    if (irrecv->decode(&results)) {
        rawLength = getRawLength(&results);
        // Copy hasil scan ke buffer rawData
        for (uint16_t i = 0; i < rawLength; i++) {
            rawData[i] = results.rawbuf[i+1] * kRawTick;
        }
        saveIR(); // Langsung amankan ke SD Card
        Serial.println("Remot Berhasil Di-Kloning ke SD!");
        irrecv->resume();
    }
}
