/*
 * ARC HUB v0.8.0 — Live camera feed via WiFi + TJpg_Decoder
 *
 * Required libraries (install via Arduino Library Manager):
 *   - TFT_eSPI  (by Bodmer)
 *   - XPT2046_Touchscreen
 *   - TJpg_Decoder  (by Bodmer)  <---- THIS IS THE KEY ONE
 *
 * ESP32-CAM must be running esp32cam_wifi_server.ino on the same network.
 * Update camIP below to match.
 */

#include <WiFi.h>
#include "time.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <TJpg_Decoder.h>          // JPEG decoder — renders block-by-block to TFT
#include <HTTPClient.h>
#include <WiFiClient.h>

// ─── Touchscreen pins ────────────────────────────────────────────────────────
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ─── RGB LED Pins (Common Anode) ─────────────────────────────────────────────
#define LED_R 18
#define LED_G 26
#define LED_B 27

// ─── WiFi / NTP ──────────────────────────────────────────────────────────────
const char* ssid      = "CountyBB-EB0E40";
const char* password  = "2144fb97e3";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec      = 0;
const int   daylightOffset_sec = 3600;

// ─── ESP32-CAM address ───────────────────────────────────────────────────────
// Change this to the IP that the ESP32-CAM prints on startup
const char* camIP = "192.168.1.171";   // <<< CHANGE THIS

// ─── Objects ─────────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// ─── State ───────────────────────────────────────────────────────────────────
String  output  = "Touch CAM to view camera";
String  version = "0.8.0";
unsigned long lastUpdate    = 0;
unsigned long lastTouchTime = 0;
bool    cameraActive        = false;

// Frame buffer — QVGA JPEG from ESP32-CAM is typically 10-40 KB.
// 50 KB is plenty of headroom.
#define FRAME_BUF_SIZE (50 * 1024)
uint8_t frameBuf[FRAME_BUF_SIZE];

// ─── Colour palette ──────────────────────────────────────────────────────────
#define BG_COLOR          TFT_WHITE
#define TEXT_COLOR        TFT_BLACK
#define ACCENT_COLOR      0x318C
#define STATUS_BAR_COLOR  0xDEFB
#define ON_COLOR          0x07E0
#define OUTPUT_BOX_COLOR  0xF7BE
#define TOUCH_COLOR       0xF800
#define BUTTON_COLOR      0x4A69

// ─── CAM button geometry ─────────────────────────────────────────────────────
#define CAM_BTN_X  240
#define CAM_BTN_Y    5
#define CAM_BTN_W   70
#define CAM_BTN_H   30

// ─── TJpg_Decoder callback ───────────────────────────────────────────────────
// Called once per decoded 8x8 or 16x16 block.  We just push it straight to the TFT.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= 240) return 0;                      // off the bottom — stop decoding
  tft.pushImage(x, y, w, h, bitmap);           // draw the block
  return 1;                                    // keep going
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
}

void drawCenteredText(const String &text, int y, int sz = 1, uint16_t col = TEXT_COLOR) {
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(text, 160, y, 1);
}

void drawCenteredText(const char* text, int y, int sz = 1, uint16_t col = TEXT_COLOR) {
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(text, 160, y, 1);
}

void drawCameraButton() {
  uint16_t btnCol = cameraActive ? ON_COLOR : BUTTON_COLOR;
  tft.fillRoundRect(CAM_BTN_X, CAM_BTN_Y, CAM_BTN_W, CAM_BTN_H, 5, btnCol);
  tft.drawRoundRect(CAM_BTN_X, CAM_BTN_Y, CAM_BTN_W, CAM_BTN_H, 5, TEXT_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(cameraActive ? "LIVE" : "CAM",
                 CAM_BTN_X + CAM_BTN_W/2, CAM_BTN_Y + CAM_BTN_H/2, 1);
}

void drawStatusBar() {
  tft.fillRect(0, 0, 320, 40, STATUS_BAR_COLOR);

  // WiFi circle
  tft.fillCircle(20, 20, 12, WiFi.status() == WL_CONNECTED ? ON_COLOR : 0xF800);
  tft.drawCircle(20, 20, 12, TEXT_COLOR);
  tft.setTextColor(BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("W", 20, 20, 1);

  // Version
  tft.setTextColor(TEXT_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("v" + version, 50, 15, 1);

  drawCameraButton();
  tft.drawFastHLine(0, 39, 320, 0xCE79);
}

void drawProgressBar(int x, int y, int w, int h, int pct) {
  tft.drawRoundRect(x, y, w, h, 6, TEXT_COLOR);
  pct = constrain(pct, 0, 100);
  int fw = (pct * (w - 4)) / 100;
  if (fw > 0)
    tft.fillRoundRect(x+2, y+2, fw, h-4, 4, ACCENT_COLOR);
}

void drawOutputBox() {
  tft.fillRect(10, 160, 300, 70, BG_COLOR);
  tft.fillRoundRect(10, 160, 300, 70, 12, OUTPUT_BOX_COLOR);
  tft.drawRoundRect(10, 160, 300, 70, 12, TEXT_COLOR);
  tft.fillRect(20, 170, 280, 50, OUTPUT_BOX_COLOR);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextDatum(TL_DATUM);

  String t = output;
  if (t.length() > 40) {
    int bp = 40;
    for (int i = 40; i > 0; i--) { if (t.charAt(i)==' ') { bp=i; break; } }
    tft.drawString(t.substring(0, bp), 20, 170, 1);
    tft.drawString(t.substring(bp+1),  20, 185, 1);
  } else {
    tft.drawString(t, 20, 175, 1);
  }
}

// ─── Fetch one JPEG frame and decode it onto the TFT ────────────────────────
bool fetchAndDrawFrame() {
  WiFiClient client;
  HTTPClient http;

  String url = "http://" + String(camIP) + "/capture";
  http.begin(client, url);
  http.setTimeout(3000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return false;
  }

  int totalSize = http.getSize();

  // Read the full JPEG body into frameBuf
  Stream* stream = http.getStreamPtr();
  int idx = 0;
  unsigned long deadline = millis() + 3000;

  if (totalSize > 0 && totalSize <= FRAME_BUF_SIZE) {
    // Known size — read exactly that many bytes
    while (idx < totalSize && millis() < deadline) {
      int avail = stream->available();
      if (avail > 0) {
        int toRead = min(avail, totalSize - idx);
        idx += stream->readBytes(&frameBuf[idx], toRead);
      }
      delay(1);
    }
  } else {
    // Unknown or chunked size — read until connection closes or buf full
    while (millis() < deadline && idx < FRAME_BUF_SIZE) {
      int avail = stream->available();
      if (avail > 0) {
        int toRead = min(avail, FRAME_BUF_SIZE - idx);
        idx += stream->readBytes(&frameBuf[idx], toRead);
      } else if (!client.connected()) {
        break;
      }
      delay(1);
    }
  }

  http.end();

  if (idx < 100) {
    Serial.printf("Frame too small: %d bytes\n", idx);
    return false;
  }

  Serial.printf("Decoding %d-byte JPEG\n", idx);

  // Decode and draw directly onto the TFT starting at y=40 (below status bar).
  // TJpg_Decoder calls tft_output() for every decoded block — that's where
  // the actual pixels hit the screen.
  TJpgDec.drawJpg(0, 40, frameBuf, idx);

  return true;
}

// ─── Clock screen ────────────────────────────────────────────────────────────
void drawClockScreen() {
  tft.fillRect(0, 40, 320, 200, BG_COLOR);
  drawStatusBar();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    tft.setTextSize(8);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TEXT_COLOR);
    tft.drawString(timeStr, 160, 70, 1);

    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%a %d %b %Y", &timeinfo);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(dateStr, 160, 140, 1);
  }

  drawOutputBox();
  tft.drawFastHLine(10, 235, 300, 0xDEFB);
}

// ─── Toggle camera mode ──────────────────────────────────────────────────────
void toggleCamera() {
  cameraActive = !cameraActive;

  if (cameraActive) {
    Serial.println("Camera ON");
    setLED(0, 0, 1);                          // blue = camera mode
    tft.fillScreen(TFT_BLACK);                // clear screen
    drawStatusBar();                           // redraw status bar on top
    output = "Fetching frame...";
  } else {
    Serial.println("Camera OFF");
    setLED(0, 1, 0);                          // green = normal
    output = "Touch CAM to view camera";
  }
  drawCameraButton();
}

// ─── Touch handling ──────────────────────────────────────────────────────────
void handleTouch() {
  if (!(ts.tirqTouched() && ts.touched())) return;

  TS_Point p = ts.getPoint();
  int16_t x = constrain(map(p.x, 200, 3700,  1, 320), 0, 319);
  int16_t y = constrain(map(p.y, 240, 3800,  1, 240), 0, 239);

  unsigned long now = millis();
  if (now - lastTouchTime < 300) return;
  lastTouchTime = now;

  // CAM button hit?
  if (x >= CAM_BTN_X && x < CAM_BTN_X + CAM_BTN_W &&
      y >= CAM_BTN_Y && y < CAM_BTN_Y + CAM_BTN_H) {
    toggleCamera();
    return;
  }

  // Ignore other touches while camera is streaming
  if (cameraActive) return;

  // Normal touch feedback on clock screen
  tft.fillCircle(x, y, 5, TOUCH_COLOR);
  output = "Touch: X=" + String(x) + " Y=" + String(y);
  drawOutputBox();

  if      (y < 100)            setLED(1, 0, 0);
  else if (y >= 160 && y < 230) setLED(0, 0, 1);
  else                           setLED(0, 1, 0);
}

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLED(0, 0, 1);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchscreenSPI);
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);

  // ── TJpg_Decoder setup ──────────────────────────────────────────────────
  TJpgDec.setJpgScale(1);                     // 1 = full size (no scaling down)
  TJpgDec.setCallback(tft_output);            // register our render callback
  // ────────────────────────────────────────────────────────────────────────

  // Loading screen
  drawStatusBar();
  drawCenteredText("ARC HUB", 70, 4);
  drawCenteredText("Initializing System", 110, 1);

  int barW = 220, barH = 16;
  int barX = (320 - barW) / 2, barY = 140;
  tft.drawRoundRect(barX, barY, barW, barH, 6, TEXT_COLOR);

  int statusY = barY + barH + 45;
  drawCenteredText("Connecting to WiFi...", statusY, 1);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int step = 0;
  while (WiFi.status() != WL_CONNECTED && step < 100) {
    step++;
    int pct = constrain((step * 85) / 50, 0, 85);
    drawProgressBar(barX, barY, barW, barH, pct);

    char buf[8]; sprintf(buf, "%d%%", pct);
    tft.fillRect(barX, barY+barH+10, barW, 20, BG_COLOR);
    tft.setTextSize(2); tft.setTextColor(TEXT_COLOR); tft.setTextDatum(TC_DATUM);
    tft.drawString(buf, barX + barW/2, barY+barH+10, 1);

    setLED(0, 0, step % 2);
    delay(200);
  }

  // Fill to 100 %
  for (int i = constrain((step*85)/50, 0, 85); i <= 100; i++) {
    drawProgressBar(barX, barY, barW, barH, i);
    char buf[8]; sprintf(buf, "%d%%", i);
    tft.fillRect(barX, barY+barH+10, barW, 20, BG_COLOR);
    tft.setTextSize(2); tft.setTextColor(TEXT_COLOR); tft.setTextDatum(TC_DATUM);
    tft.drawString(buf, barX + barW/2, barY+barH+10, 1);
    delay(15);
  }

  tft.fillRect(0, statusY-5, 320, 20, BG_COLOR);
  drawCenteredText("WiFi Connected!", statusY, 1, ON_COLOR);
  delay(600);

  Serial.print("Display IP: "); Serial.println(WiFi.localIP());
  Serial.print("Camera URL: http://"); Serial.print(camIP); Serial.println("/capture");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(400);

  tft.fillScreen(BG_COLOR);
  setLED(0, 1, 0);
  output = "Touch CAM to view camera";

  Serial.println("\n=== Setup Complete ===");
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop() {
  handleTouch();

  if (cameraActive) {
    // Fetch + decode one frame, then loop immediately for next
    if (!fetchAndDrawFrame()) {
      // Draw error text over the camera area so user knows something is wrong
      tft.fillRect(0, 40, 320, 200, TFT_BLACK);
      drawStatusBar();
      tft.setTextColor(TFT_RED);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.drawString("No frame", 160, 140, 1);
      tft.setTextSize(1);
      tft.drawString("Check camera IP", 160, 165, 1);
      delay(2000);
    }
  } else {
    // Clock mode — update once per second
    unsigned long now = millis();
    if (now - lastUpdate >= 1000) {
      lastUpdate = now;
      drawClockScreen();
    }
  }

  delay(10);
}
