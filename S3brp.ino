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
int ledMode = 0; 
uint32_t ledColor = 0xFF0000; 
unsigned long lastStrobe = 0;
bool strobeState = false;

// Buffer IR Raw
uint16_t rawData[400];
uint16_t rawLength = 0;

/* --- FUNGSI HELPER --- */
uint16_t getRawLength(decode_results *results) {
    return results->rawlen - 1;
}

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

/* --- UI DASHBOARD --- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>ROGUE S3 ULTIMATE</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
    body { background:#050505; color:#00ff41; font-family:monospace; text-align:center; padding:10px; }
    .box { border:1px solid #00ff41; padding:15px; margin:10px auto; max-width:500px; background:#111; box-shadow:0 0 10px #00ff41; }
    .grid { display:grid; grid-template-columns: 1fr 1fr; gap:10px; }
    button { background:#00ff41; color:#000; border:none; padding:12px; font-weight:bold; cursor:pointer; width:100%; border-radius:3px; margin:5px 0; }
    input { background:#000; border:1px solid #00ff41; color:#00ff41; padding:10px; width:90%; margin:5px 0; }
    .danger { background:#ff0000 !important; color:#fff !important; }
</style></head>
<body>
    <div class="box">
        <h2>[ ROGUE S3 COMMAND ]</h2>
        <button onclick="fetch('/relay')">TOGGLE RELAY 5V</button>
        <div class="grid"><input type="color" onchange="setRGB(this.value)"><button onclick="fetch('/led/mode?m=2')">STROBO</button></div>
        <button onclick="fetch('/led/mode?m=0')" class="danger">LED OFF</button>
        <hr>
        <div class="grid"><button onclick="fetch('/ir/record')">REKAM REMOTE</button><button onclick="fetch('/ir/send')">TEMBAK DARI SD</button></div>
        <hr>
        <form action="/save" method="GET">
            <input name="s" placeholder="New SSID"><input name="p" placeholder="New Pass">
            <div class="grid">
                <input name="pr" type="number" placeholder="Relay Pin"><input name="irx" type="number" placeholder="IR RX Pin">
                <input name="itx" type="number" placeholder="IR TX Pin"><input name="sc" type="number" placeholder="SD CS Pin"><input name="lp" type="number" placeholder="LED Pin">
            </div>
            <button type="submit" style="background:#444; color:#fff;">SIMPAN & REBOOT</button>
        </form>
    </div>
    <script>function setRGB(h){let r=parseInt(h.slice(1,3),16),g=parseInt(h.slice(3,5),16),b=parseInt(h.slice(5,7),16);fetch(`/led/set?r=${r}&g=${g}&b=${b}`);}</script>
</body></html>)rawliteral";

void setup() {
    Serial.begin(115200);
    pref.begin("rogue_v2", false);
    cfg_ssid = pref.getString("s", "ROGUE_S3_AP");
    cfg_pass = pref.getString("p", "mercon123");
    pin_relay = pref.getInt("pr", 1); pin_irx = pref.getInt("irx", 2);
    pin_itx = pref.getInt("itx", 3); pin_sdcs = pref.getInt("sc", 10); pin_led = pref.getInt("lp", 48);

    pixels = new Adafruit_NeoPixel(1, pin_led, NEO_GRB + NEO_KHZ800);
    pixels->begin(); pixels->show();
    pinMode(pin_relay, OUTPUT); digitalWrite(pin_relay, HIGH);

    if (SD.begin(pin_sdcs)) loadIR();
    irrecv = new IRrecv(pin_irx); irsend = new IRsend(pin_itx);
    irrecv->enableIRIn(); irsend->begin();

    WiFi.softAP(cfg_ssid.c_str(), cfg_pass.c_str());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
    server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *r){ relayState=!relayState; digitalWrite(pin_relay, relayState?LOW:HIGH); r->send(200, "text/plain", "OK"); });
    server.on("/led/set", HTTP_GET, [](AsyncWebServerRequest *r){ ledColor=pixels->Color(r->arg("r").toInt(),r->arg("g").toInt(),r->arg("b").toInt()); ledMode=1; r->send(200); });
    server.on("/led/mode", HTTP_GET, [](AsyncWebServerRequest *r){ ledMode=r->arg("m").toInt(); r->send(200); });
    server.on("/ir/send", HTTP_GET, [](AsyncWebServerRequest *r){ if(rawLength>0) irsend->sendRaw(rawData,rawLength,38); r->send(200); });
    server.on("/save", HTTP_GET, [](AsyncWebServerRequest *r){
        if(r->hasArg("s"))pref.putString("s",r->arg("s")); if(r->hasArg("p"))pref.putString("p",r->arg("p"));
        if(r->hasArg("pr"))pref.putInt("pr",r->arg("pr").toInt()); if(r->hasArg("irx"))pref.putInt("irx",r->arg("irx").toInt());
        if(r->hasArg("itx"))pref.putInt("itx",r->arg("itx").toInt()); if(r->hasArg("sc"))pref.putInt("sc",r->arg("sc").toInt());
        if(r->hasArg("lp"))pref.putInt("lp",r->arg("lp").toInt()); r->send(200); delay(2000); ESP.restart();
    });
    server.begin();
}

void loop() {
    if (ledMode == 1) { pixels->setPixelColor(0, ledColor); pixels->show(); }
    else if (ledMode == 2 && millis() - lastStrobe > 50) { strobeState=!strobeState; pixels->setPixelColor(0, strobeState?ledColor:0); pixels->show(); lastStrobe=millis(); }
    else if (ledMode == 0) { pixels->setPixelColor(0,0); pixels->show(); }

    if (irrecv->decode(&results)) {
        rawLength = getRawLength(&results);
        for (uint16_t i = 0; i < rawLength; i++) { 
            // kRawTick udah ada di library, panggil langsung aja
            rawData[i] = results.rawbuf[i+1] * kRawTick; 
        }
        saveIR(); irrecv->resume();
    }
}
