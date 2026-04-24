#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid = "ROGUE-PRO-SYNC";
const char* password = "musangking";

AsyncWebServer server(80);
String allData = ""; 
bool startSignal = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Rogue Ultra Sync</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background: #000; color: #0f0; font-family: monospace; padding: 15px; text-align: center; }
    textarea { background: #111; color: #0f0; border: 2px solid #0f0; width: 95%; height: 300px; padding: 10px; font-size: 14px; border-radius: 10px; }
    .btn { background: #0f0; color: #000; padding: 15px; width: 100%; border: none; font-weight: bold; margin-top: 10px; border-radius: 50px; cursor: pointer; font-size: 18px; }
    .guide { text-align: left; font-size: 0.8em; color: #888; border: 1px dashed #0f0; padding: 10px; margin-bottom: 10px; }
  </style>
</head><body>
  <h2 style="text-shadow: 0 0 10px #0f0;">ROGUE SYNC PRO</h2>
  <div class="guide">
    <b>Format:</b> Kata | JedaTampil | Baris | JedaMati<br>
    Contoh:<br>
    Kesucian|400|1|0 (Nahan)<br>
    Juaa...|2000|2|500 (Tampil 2s, Mati 0.5s)
  </div>
  <form action="/update">
    <textarea name="text" placeholder="Kata|JedaTampil|Baris|JedaMati"></textarea>
    <button type="submit" class="btn">EXECUTE SEQUENCE</button>
  </form>
</body></html>)rawliteral";

void showCentered(String text, int y, int size) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, y);
  display.print(text);
}

void setup() {
  Wire.begin(8, 9);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  WiFi.softAP(ssid, password);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    allData = request->getParam("text")->value();
    startSignal = true;
    request->send(200, "text/plain", "Sequence Synced!");
  });
  server.begin();
}

void loop() {
  if (startSignal) {
    for (int i = 3; i >= 1; i--) {
      display.clearDisplay();
      showCentered(String(i), 15, 6);
      display.display();
      delay(1000);
    }

    display.clearDisplay();
    display.setTextSize(1);
    
    String row1 = ""; 
    String row2 = "";

    int startIdx = 0;
    int nextIdx = allData.indexOf('\n');

    while (startIdx < allData.length()) {
      String line = (nextIdx != -1) ? allData.substring(startIdx, nextIdx) : allData.substring(startIdx);
      line.trim();

      if (line.length() > 0) {
        int s1 = line.indexOf('|');
        int s2 = line.indexOf('|', s1 + 1);
        int s3 = line.indexOf('|', s2 + 1);

        if (s1 != -1 && s2 != -1 && s3 != -1) {
          String word = line.substring(0, s1);
          int delayShow = line.substring(s1 + 1, s2).toInt();
          int rowNum = line.substring(s2 + 1, s3).toInt();
          int delayOff = line.substring(s3 + 1).toInt();

          if (rowNum == 1) row1 += word + " ";
          else row2 += word + " ";

          display.clearDisplay();
          display.setCursor(0, 15); display.print(row1);
          display.setCursor(0, 40); display.print(row2);
          display.display();
          
          delay(delayShow);

          if (delayOff > 0) {
            display.clearDisplay();
            display.display();
            row1 = "";
            row2 = "";
            delay(delayOff); // Ini jeda layar matinya
          }
        }
      }
      if (nextIdx == -1) break;
      startIdx = nextIdx + 1;
      nextIdx = allData.indexOf('\n', startIdx);
    }
    startSignal = false;
  }
}
