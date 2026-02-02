#include <WiFi.h>
#include "time.h"
#include "wifi.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// Display pins (must match User_Setup.h)
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   21  // Backlight control

// Touch screen pins (XPT2046 - MUST match User_Setup.h)
#define TOUCH_CS  33  // Changed to 33 to match User_Setup.h
#define TOUCH_IRQ -1  // Set to -1 if not using interrupt

// RGB LED Pins (Common Anode)
#define LED_R 25
#define LED_G 26
#define LED_B 27

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
// Touch screen without interrupt pin (using polling)
XPT2046_Touchscreen ts(TOUCH_CS);

int camera_status = 0;
int speaker_status = 0;
String output = "Touch the screen to test";
String version = "0.4.3";
int loadProgress = 0;
unsigned long lastUpdate = 0;
int lastPercentage = -1;
bool touchHandled = false;
unsigned long lastTouchTime = 0;

// Modern color palette
#define BG_COLOR ILI9341_WHITE
#define TEXT_COLOR ILI9341_BLACK
#define ACCENT_COLOR 0x318C
#define STATUS_BAR_COLOR 0xDEFB
#define PROGRESS_COLOR 0x07E0
#define OFF_COLOR 0xDEDB
#define ON_COLOR 0x07E0
#define BOX_COLOR 0xEF5D
#define OUTPUT_BOX_COLOR 0xF7BE
#define TOUCH_COLOR 0xF800  // Red for touch feedback

// Function declarations from control.ino
extern void setupControl();
extern void processTouch(int x, int y);
extern String getProcessedCommand();

// Helper to control RGB (Common Anode: LOW = ON)
void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
}

void drawRoundedBox(int x, int y, int w, int h, int radius, uint16_t color, bool fill = true) {
  if (fill) {
    tft.fillRoundRect(x, y, w, h, radius, color);
  }
  tft.drawRoundRect(x, y, w, h, radius, TEXT_COLOR);
}

// Overloaded function to handle both String and const char*
void drawCenteredText(const String &text, int y, int textSize = 1, uint16_t color = TEXT_COLOR) {
  tft.setTextSize(textSize);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((320 - w) / 2, y);
  tft.print(text);
}

void drawCenteredText(const char* text, int y, int textSize = 1, uint16_t color = TEXT_COLOR) {
  tft.setTextSize(textSize);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((320 - w) / 2, y);
  tft.print(text);
}

void drawStatusBar() {
  // Status bar background
  tft.fillRect(0, 0, 320, 40, STATUS_BAR_COLOR);
  
  // WiFi indicator with modern look
  int wifiRadius = 12;
  int wifiX = 20;
  int wifiY = 20;
  
  tft.fillCircle(wifiX, wifiY, wifiRadius, WiFi.status() == WL_CONNECTED ? ON_COLOR : 0xF800);
  tft.drawCircle(wifiX, wifiY, wifiRadius, TEXT_COLOR);
  
  // Draw WiFi symbol
  tft.setTextColor(BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(wifiX - 3, wifiY - 4);
  tft.print("W");
  
  // Version in corner
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(280, 15);
  tft.print("v");
  tft.print(version);
  
  // Bottom border of status bar
  tft.drawFastHLine(0, 39, 320, 0xCE79);
}

void drawProgressBar(int x, int y, int width, int height, int progress) {
  // Draw background
  tft.drawRoundRect(x, y, width, height, 6, TEXT_COLOR);
  
  // Draw fill with rounded edges (constrain progress to 0-100)
  progress = constrain(progress, 0, 100);
  int fillWidth = (progress * (width - 4)) / 100;
  if (fillWidth > 0) {
    tft.fillRoundRect(x + 2, y + 2, fillWidth, height - 4, 4, ACCENT_COLOR);
  }
}

void drawOutputBox() {
  // Clear the output area completely
  tft.fillRect(10, 160, 300, 70, BG_COLOR);
  
  // Draw full-width rounded rectangle for output text
  drawRoundedBox(10, 160, 300, 70, 12, OUTPUT_BOX_COLOR);
  
  // Draw output text inside the box (left aligned, smaller size)
  tft.setTextSize(1); // Smaller text size
  tft.setTextColor(TEXT_COLOR);
  
  // Clear area for output text (leave padding)
  tft.fillRect(20, 170, 280, 50, OUTPUT_BOX_COLOR);
  
  // Split output into lines based on newline characters
  String displayText = output;
  int lineCount = 0;
  int currentPos = 0;
  
  // Find newlines and draw each line
  while (currentPos < displayText.length() && lineCount < 3) {
    int nextNewline = displayText.indexOf('\n', currentPos);
    String line;
    
    if (nextNewline == -1) {
      line = displayText.substring(currentPos);
      currentPos = displayText.length();
    } else {
      line = displayText.substring(currentPos, nextNewline);
      currentPos = nextNewline + 1;
    }
    
    // Trim line if too long (optional safety)
    if (line.length() > 40) {
      line = line.substring(0, 37) + "...";
    }
    
    // Draw the line
    tft.setCursor(20, 170 + (lineCount * 15));
    tft.print(line);
    
    lineCount++;
  }
}

// Function to show touch feedback
void showTouchFeedback(int x, int y) {
  // Draw a small circle at touch point
  tft.fillCircle(x, y, 5, TOUCH_COLOR);
  delay(50);
  // Redraw the affected area
  tft.fillRect(x-6, y-6, 12, 12, OUTPUT_BOX_COLOR);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLED(0, 0, 1); // Initially blue

  // Initialize display backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Turn on backlight

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR);

  // Initialize XPT2046 touch screen
  ts.begin();
  ts.setRotation(1); // Set rotation to match display

  // Initialize control system
  setupControl();

  // Draw initial UI - KEEPING YOUR EXACT LOADING ANIMATION
  drawStatusBar();
  
  // App title - perfectly centered
  drawCenteredText("ARC HUB", 70, 4, TEXT_COLOR);
  
  // Subtitle
  drawCenteredText("Initializing System", 110, 1);
  
  // Progress bar area with proper spacing
  int barWidth = 220;
  int barHeight = 16;
  int barX = (320 - barWidth) / 2;
  int barY = 140;  // Bar position
  
  // Draw empty progress bar
  tft.drawRoundRect(barX, barY, barWidth, barHeight, 6, TEXT_COLOR);
  
  // Loading text - positioned below where percentage will be
  int statusY = barY + barHeight + 45;  // 45px below bar (leaves room for percentage)
  tft.fillRect(0, statusY - 5, 320, 20, BG_COLOR);
  drawCenteredText("Connecting to Network", statusY, 1);
  
  // Begin the WiFi
  WiFi.begin(ssid, password);

  // Modern loading animation - faster progress
  unsigned long lastAnim = 0;
  bool animToggle = false;
  int animationStep = 0;  // Simple counter for smooth progress
  
  while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    
    // LED animation
    if (currentMillis - lastAnim > 200) {
      lastAnim = currentMillis;
      animToggle = !animToggle;
      setLED(0, 0, animToggle ? 1 : 0);
      
      // FASTER progress from 0-85% while connecting
      animationStep++;
      loadProgress = (animationStep * 85) / 50; // Faster scaling (divide by 50 instead of 100)
      loadProgress = constrain(loadProgress, 0, 85);
      
      // Only update if percentage changed
      if (loadProgress != lastPercentage) {
        // Update progress bar
        drawProgressBar(barX, barY, barWidth, barHeight, loadProgress);
        
        // Clear EXACT area for percentage text
        // Calculate text bounds to clear exactly what we need
        char percentStr[10];
        sprintf(percentStr, "%d%%", loadProgress);
        
        tft.setTextSize(2);
        int16_t x1, y1;
        uint16_t textWidth, textHeight;
        tft.getTextBounds(percentStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
        
        // Clear area based on actual text size + padding
        int clearX = barX + (barWidth - textWidth) / 2 - 2; // -2px left padding
        int clearY = barY + barHeight + 10; // 10px below bar
        int clearWidth = textWidth + 4; // +4px total padding
        int clearHeight = textHeight + 2; // +2px bottom padding
        
        tft.fillRect(clearX, clearY, clearWidth, clearHeight, BG_COLOR);
        
        // Draw new percentage
        tft.setTextColor(TEXT_COLOR);
        tft.setCursor(barX + (barWidth - textWidth) / 2, barY + barHeight + 12);
        tft.print(percentStr);
        
        lastPercentage = loadProgress;
      }
    }
    
    // Check if WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
      // Ensure we show 85% before moving to 100%
      if (loadProgress < 85) {
        loadProgress = 85;
        drawProgressBar(barX, barY, barWidth, barHeight, loadProgress);
        
        // Update percentage display to 85%
        char percentStr[10];
        sprintf(percentStr, "%d%%", loadProgress);
        
        tft.setTextSize(2);
        int16_t x1, y1;
        uint16_t textWidth, textHeight;
        tft.getTextBounds(percentStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
        
        int clearX = barX + (barWidth - textWidth) / 2 - 2;
        int clearY = barY + barHeight + 10;
        int clearWidth = textWidth + 4;
        int clearHeight = textHeight + 2;
        
        tft.fillRect(clearX, clearY, clearWidth, clearHeight, BG_COLOR);
        
        tft.setTextColor(TEXT_COLOR);
        tft.setCursor(barX + (barWidth - textWidth) / 2, barY + barHeight + 12);
        tft.print(percentStr);
      }
      break;
    }
    
    delay(30); // Faster delay for quicker progress
  }

  // FAST completion to 100% - GUARANTEED to finish at 100%
  // Start from current progress and go to 100 in 5% increments
  int startProgress = loadProgress;
  
  for (int i = startProgress; i <= 100; i++) {
    // Ensure we always reach 100
    if (i > 100) i = 100;
    
    drawProgressBar(barX, barY, barWidth, barHeight, i);
    
    // Clear EXACT area for percentage text
    char percentStr[10];
    sprintf(percentStr, "%d%%", i);
    
    tft.setTextSize(2);
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    tft.getTextBounds(percentStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Clear area based on actual text size + padding
    int clearX = barX + (barWidth - textWidth) / 2 - 2; // -2px left padding
    int clearY = barY + barHeight + 10; // 10px below bar
    int clearWidth = textWidth + 4; // +4px total padding
    int clearHeight = textHeight + 2; // +2px bottom padding
    
    tft.fillRect(clearX, clearY, clearWidth, clearHeight, BG_COLOR);
    
    // Draw new percentage
    tft.setTextColor(TEXT_COLOR);
    tft.setCursor(barX + (barWidth - textWidth) / 2, barY + barHeight + 12);
    tft.print(percentStr);
    
    // Fast but not too fast - adjust speed based on progress
    if (i < 95) {
      delay(15); // Fast for most of the progress
    } else {
      delay(30); // Slightly slower for the last few percentages
    }
  }
  
  // FINAL GUARANTEE: Force show 100% one more time
  drawProgressBar(barX, barY, barWidth, barHeight, 100);
  
  char finalPercentStr[10] = "100%";
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  tft.getTextBounds(finalPercentStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
  
  int clearX = barX + (barWidth - textWidth) / 2 - 2;
  int clearY = barY + barHeight + 10;
  int clearWidth = textWidth + 4;
  int clearHeight = textHeight + 2;
  
  tft.fillRect(clearX, clearY, clearWidth, clearHeight, BG_COLOR);
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(barX + (barWidth - textWidth) / 2, barY + barHeight + 12);
  tft.print(finalPercentStr);
  
  // Show connected
  tft.fillRect(0, statusY - 5, 320, 20, BG_COLOR);
  drawCenteredText("Connected ✓", statusY, 1, ON_COLOR);
  delay(200); // Shorter delay
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Clear screen for main interface
  delay(200);
  tft.fillScreen(BG_COLOR);
  setLED(0, 1, 0); // Green when connected
  
  // Initial output message with command response
  output = "Touch screen to see command response";
  String cmdResponse = getProcessedCommand();
  if (cmdResponse.length() > 0) {
    output = "Ready!\nCommand: " + cmdResponse;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update every second
  if (currentMillis - lastUpdate >= 1000) {
    lastUpdate = currentMillis;
    
    // Clear main area (preserve status bar)
    tft.fillRect(0, 40, 320, 200, BG_COLOR);
    
    // Update status bar
    drawStatusBar();
    
    // Get and display time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // Big centered time (hours and minutes only, no seconds)
      char timeStr[6];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      drawCenteredText(timeStr, 60, 8, TEXT_COLOR); // Moved up more
      
      // Shorter date format that fits on screen
      char dateStr[16];
      strftime(dateStr, sizeof(dateStr), "%a %d %b %Y", &timeinfo); // Shorter format
      drawCenteredText(dateStr, 130, 2); // Adjusted position
    }
    
    // Draw output box (moved up, full width, no label)
    drawOutputBox();
    
    // Very subtle bottom border (optional, can remove)
    tft.drawFastHLine(10, 235, 300, 0xDEFB);
  }

  // Touch screen handling
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    
    // Convert raw touch coordinates to screen coordinates
    // Calibration values for Wokwi simulation
    int16_t touchX = map(p.x, 200, 3700, 0, 320);
    int16_t touchY = map(p.y, 200, 3700, 0, 240);
    
    // For rotation 1 (landscape), swap and invert axes
    int16_t screenX = 320 - touchY;
    int16_t screenY = touchX;
    
    // Ensure coordinates are within screen bounds
    screenX = constrain(screenX, 0, 319);
    screenY = constrain(screenY, 0, 239);
    
    // Only process touch every 300ms to debounce
    if (currentMillis - lastTouchTime > 300) {
      lastTouchTime = currentMillis;
      
      // Show visual touch feedback
      showTouchFeedback(screenX, screenY);
      
      // Notify control system about touch
      processTouch(screenX, screenY);
      
      // Get the command output from control system
      String cmdResponse = getProcessedCommand();
      
      // Update output with both touch info and command response
      output = "Touch: X=" + String(screenX) + ", Y=" + String(screenY);
      output += "\n" + cmdResponse;
      
      Serial.println(output);
      
      // Update the display immediately
      drawOutputBox();
      
      // Different LED colors based on touch area
      if (screenY < 100) {
        // Top area (above time)
        setLED(1, 0, 0); // Red
      } else if (screenY >= 160 && screenY <= 230) {
        // Output box area
        setLED(0, 0, 1); // Blue
      } else {
        // Middle area (time/date area)
        setLED(0, 1, 0); // Green
      }
    }
  }
  
  // Small delay to prevent CPU overload
  delay(10);
}
