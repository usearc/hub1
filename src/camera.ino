#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>


// WiFi credentials (must match display)
const char* ssid = "your wifi";
const char* password = "your pasword";


// Camera model AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27


#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


#define LED_PIN 4
#define STATUS_LED 33


WebServer server(80);


bool cameraActive = false;
unsigned long frameCount = 0;


void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Init with high specs for quality
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;  // 320x240 - perfect for display!
    config.jpeg_quality = 10;  // 0-63 lower means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Adjust camera settings
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale...)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable

  Serial.println("Camera initialized successfully!");
}


// Handle root page
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32-CAM Server</h1>";
  html += "<p>Camera Status: " + String(cameraActive ? "ACTIVE" : "READY") + "</p>";
  html += "<p>Frames served: " + String(frameCount) + "</p>";
  html += "<p><a href='/capture'>Get Single Frame</a></p>";
  html += "<p><a href='/stream'>Live Stream</a></p>";
  html += "<img src='/capture' width='320' height='240'>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


// Capture single frame
void handleCapture() {
  camera_fb_t * fb = NULL;

  // Turn on LED while capturing
  digitalWrite(LED_PIN, HIGH);

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    digitalWrite(LED_PIN, LOW);
    return;
  }

  frameCount++;
  cameraActive = true;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
  digitalWrite(LED_PIN, LOW);

  Serial.printf("Frame %lu sent (%d bytes)\n", frameCount, fb->len);
}


// Stream video
void handleStream() {
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  Serial.println("Stream started");
  cameraActive = true;

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    digitalWrite(LED_PIN, HIGH);

    String header = "--frame\r\n";
    header += "Content-Type: image/jpeg\r\n\r\n";
    client.print(header);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);
    digitalWrite(LED_PIN, LOW);

    frameCount++;

    if (frameCount % 30 == 0) {
      Serial.printf("Streaming... frame %lu\n", frameCount);
    }

    delay(30);  // ~30 FPS max
  }

  cameraActive = false;
  Serial.println("Stream ended");
}


// Get status
void handleStatus() {
  String json = "{";
  json += "\"active\":" + String(cameraActive ? "true" : "false") + ",";
  json += "\"frames\":" + String(frameCount) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("\n\n=================================");
  Serial.println("  ESP32-CAM WiFi Camera Server");
  Serial.println("=================================\n");

  // Startup blink
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(200);
    digitalWrite(STATUS_LED, LOW);
    delay(200);
  }

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("\n>>> IMPORTANT: Copy this IP address <<<");
    Serial.println("Put this in the display code as camIP!");
    Serial.println("=========================================\n");

    digitalWrite(STATUS_LED, HIGH);
    delay(1000);
    digitalWrite(STATUS_LED, LOW);
  } else {
    Serial.println("\n✗ WiFi connection failed!");
    // Blink error
    while(1) {
      digitalWrite(STATUS_LED, HIGH);
      delay(100);
      digitalWrite(STATUS_LED, LOW);
      delay(100);
    }
  }

  // Initialize camera
  Serial.println("Initializing camera...");
  setupCamera();

  // Setup web server
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("✓ Web server started");
  Serial.println("\nCamera URLs:");
  Serial.println("  Web Interface: http://" + WiFi.localIP().toString() + "/");
  Serial.println("  Single Frame:  http://" + WiFi.localIP().toString() + "/capture");
  Serial.println("  Live Stream:   http://" + WiFi.localIP().toString() + "/stream");
  Serial.println("\n✓ Ready to stream!\n");

  // Success - solid LED
  digitalWrite(STATUS_LED, HIGH);
}


void loop() {
  server.handleClient();

  // Heartbeat when not active
  if (!cameraActive) {
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 2000) {
      lastBlink = millis();
      digitalWrite(STATUS_LED, LOW);
      delay(20);
      digitalWrite(STATUS_LED, HIGH);
    }
  }

  delay(1);
}
