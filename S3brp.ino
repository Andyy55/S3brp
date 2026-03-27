#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>

/* -- CONFIG -- */
const char* ssid = "APAAJALAH";
const char* pass = "bosmuda00";

AsyncWebServer server(80);
Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);

// Global State
int ledMode = 3; 
uint32_t ledColor = 0xFF0000;
unsigned long lastUpdate = 0;
uint8_t rainbowPos = 0;
int breathVal = 0, breathDir = 2;
bool isArmed = false;

// Function Wheel untuk Rainbow
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if(WheelPos < 170) { WheelPos -= 85; return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3); }
  WheelPos -= 170; return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

/* --- UI WEB CYBERPUNK --- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MUSANG PROPLAYER</title>
<style>
    body { background:#0a0a0a; color:#ff0000; font-family:'Courier New', monospace; text-align:center; padding:20px; }
    .container { border:2px solid #ff0000; padding:20px; max-width:400px; margin:auto; box-shadow: 0 0 20px #f00; border-radius:10px; }
    h1 { text-shadow: 2px 2px #500; letter-spacing: 5px; }
    .status { font-size: 1.2em; margin: 15px; padding:10px; background:#200; border:1px solid #f00; }
    .btn { background:#500; color:#fff; border:1px solid #f00; padding:15px; width:100%; font-weight:bold; cursor:pointer; margin:5px 0; border-radius:5px; transition: 0.3s; }
    .btn:active { background:#f00; color:#000; }
    .btn-detonate { background:#f00; color:#fff; font-size:1.5em; display:none; animation: blink 0.5s infinite; }
    @keyframes blink { 0% {opacity:1;} 50% {opacity:0.3;} 100% {opacity:1;} }
   .grid { display:grid; grid-template-columns: 1fr 1fr; gap:10px; margin-top:10px; }
    .led-btn { background:#111; color:#0f0; border:1px solid #0f0; font-size:0.8em; padding:10px; }
</style>
</head>
<body>
    <div class="container">
        <h1>[ ROGUE S3 ]</h1>
        <div class="status" id="stat">SYSTEM: DISARMED</div>
        
        <div id="authBox">
            <input type="password" id="passCode" placeholder="ENTER SAFETY CODE" style="background:#000; color:#f00; border:1px solid #f00; padding:10px; width:80%; text-align:center;">
            <button class="btn" onclick="armSystem()">ARM SYSTEM</button>
        </div>

        <button id="detBtn" class="btn btn-detonate" onclick="detonate()">FIRE!!!</button>
        
        <hr style="border:1px solid #333; margin:20px 0;">
        <h3 style="color:#0f0;">LED FX CONTROL</h3>
        <h4 style="margin:5px; color:#0f0;">LIVE RGB TUNER</h4>
<input type="range" id="R" style="accent-color:red; width:100%;" min="0" max="255" value="0" oninput="L()">
<input type="range" id="G" style="accent-color:green; width:100%;" min="0" max="255" value="0" oninput="L()">
<input type="range" id="B" style="accent-color:blue; width:100%;" min="0" max="255" value="0" oninput="L()">

<script>
// Nama fungsi ini harus sinkron sama oninput di atas!
function L() {
    let r = document.getElementById('R').value;
    let g = document.getElementById('G').value;
    let b = document.getElementById('B').value;
    fetch(`/led/set?r=${r}&g=${g}&b=${b}`);
}
</script>

        <div class="grid">
            <button class="led-btn" onclick="setMode(1)">STATIC</button>
            <button class="led-btn" onclick="setMode(2)">STROBE FAST</button>
            <button class="led-btn" onclick="setMode(3)">RAINBOW FLOW</button>
            <button class="led-btn" onclick="setMode(4)">BREATH SLOW</button>
            <button class="led-btn" onclick="setMode(5)">POLICE SIREN</button>
            <button class="led-btn" onclick="setMode(6)">GLITCH NIGHT</button>
            <button class="led-btn" onclick="setMode(7)">HEARTBEAT</button>
            <button class="led-btn" onclick="setMode(8)">FIRE FLICKER</button>
            <button class="led-btn" onclick="setMode(9)">RGB CHASE</button>
            <button class="led-btn" onclick="setMode(0)">STEALH OFF</button>
        </div>
    </div>

<script>
    function setRGB(h){
        let r=parseInt(h.slice(1,3),16), g=parseInt(h.slice(3,5),16), b=parseInt(h.slice(5,7),16);
        fetch(`/led/set?r=${r}&g=${g}&b=${b}`);
    }
    function setMode(m){ fetch(`/led/mode?m=${m}`); }
    
    function armSystem(){
        let code = document.getElementById('passCode').value;
        if(code === "777"){
            document.getElementById('authBox').style.display = 'none';
            document.getElementById('detBtn').style.display = 'block';
            document.getElementById('stat').innerText = "SYSTEM: READY BOS!";
            document.getElementById('stat').style.background = "#f00";
            document.getElementById('stat').style.color = "#fff";
        } else { alert("WRONG CODE, TOLOL!"); }
    }

    function detonate(){
        if(confirm("CONFIRMATION")){
            fetch('/fire');
            setTimeout(() => { location.reload(); }, 3000);
        }
    }
</script>
</body></html>)rawliteral";

void setup() {
    Serial.begin(115200);
    pixels.begin();
    
    // Relay Safety: Input agar floating pas awal
    pinMode(1, INPUT); 

    WiFi.softAP(ssid, pass);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
    
    server.on("/led/set", HTTP_GET, [](AsyncWebServerRequest *r){
    ledColor = pixels.Color(r->arg("r").toInt(), r->arg("g").toInt(), r->arg("b").toInt());
    ledMode = 1; // Paksa balik ke static biar live
    pixels.setPixelColor(0, ledColor);
    pixels.show(); 
    r->send(200);
});


    server.on("/led/mode", HTTP_GET, [](AsyncWebServerRequest *r){ ledMode = r->arg("m").toInt(); r->send(200); });

    server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *r){
        // DETONASI: Relay ON selama 2 detik terus OFF lagi
        pinMode(1, OUTPUT);
        digitalWrite(1, LOW); // Trigger Relay
        r->send(200, "text/plain", "BOOM!");
        delay(2000); 
        pinMode(1, INPUT); // Balik Safe
    });

    server.begin();
}

void loop() {
    unsigned long now = millis();
    
    if (ledMode == 1) { // Static
        pixels.setPixelColor(0, ledColor);
    } 
    else if (ledMode == 2 && now - lastUpdate > 30) { // Strobe
        static bool s = false; s = !s;
        pixels.setPixelColor(0, s ? ledColor : 0);
        lastUpdate = now;
    } 
    else if (ledMode == 3 && now - lastUpdate > 15) { // Rainbow
        pixels.setPixelColor(0, Wheel(rainbowPos++));
        lastUpdate = now;
    }
    else if (ledMode == 4 && now - lastUpdate > 10) { // Breath
        breathVal += breathDir;
        if(breathVal <= 0 || breathVal >= 255) breathDir *= -1;
        uint8_t r=(ledColor>>16)&0xFF, g=(ledColor>>8)&0xFF, b=ledColor&0xFF;
        pixels.setPixelColor(0, pixels.Color((r*breathVal)/255, (g*breathVal)/255, (b*breathVal)/255));
        lastUpdate = now;
    }
    else if (ledMode == 5 && now - lastUpdate > 100) { // Police
        static bool p = false; p = !p;
        pixels.setPixelColor(0, p ? 0xFF0000 : 0x0000FF);
        lastUpdate = now;
    }
    else if (ledMode == 6 && now - lastUpdate > 50) { // Glitch
        pixels.setPixelColor(0, random(0, 2) ? ledColor : pixels.Color(random(255), random(255), random(255)));
        lastUpdate = now;
    }
    else if (ledMode == 7 && now - lastUpdate > 10) { // Heartbeat
        int beat = (1 + sin(now / 200.0)) * 127;
        uint8_t r=(ledColor>>16)&0xFF;
        pixels.setPixelColor(0, pixels.Color((r*beat)/255, 0, 0));
        lastUpdate = now;
    }
    else if (ledMode == 8 && now - lastUpdate > 80) { // Fire
        pixels.setPixelColor(0, pixels.Color(random(200, 255), random(50, 100), 0));
        lastUpdate = now;
    }
    else if (ledMode == 9 && now - lastUpdate > 200) { // RGB Chase
        static int c = 0;
        if(c==0) pixels.setPixelColor(0, 0xFF0000);
        else if(c==1) pixels.setPixelColor(0, 0x00FF00);
        else pixels.setPixelColor(0, 0x0000FF);
        c = (c+1)%3;
        lastUpdate = now;
    }
    else if (ledMode == 0) {
        pixels.setPixelColor(0, 0);
    }

    pixels.show();
}
