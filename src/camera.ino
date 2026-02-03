#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

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

#define LED_PIN 12
#define STATUS_LED 13

// Global camera variables
bool cameraActive = false;
unsigned long frameCount = 0;

// Forward declaration (server is defined in main.ino)
extern WebServer server;

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

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);
  s->set_special_effect(s, 0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_ae_level(s, 0);
  s->set_aec_value(s, 300);
  s->set_gain_ctrl(s, 1);
  s->set_agc_gain(s, 0);
  s->set_gainceiling(s, (gainceiling_t)0);
  s->set_bpc(s, 0);
  s->set_wpc(s, 1);
  s->set_raw_gma(s, 1);
  s->set_lenc(s, 1);
  s->set_hmirror(s, 0);
  s->set_vflip(s, 0);
  s->set_dcw(s, 1);
  s->set_colorbar(s, 0);

  Serial.println("Camera initialized!");
}

void handleCameraRoot() {
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

void handleCameraCapture() {
  camera_fb_t * fb = NULL;
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

void handleCameraStream() {
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
    delay(30);
  }

  cameraActive = false;
  Serial.println("Stream ended");
}

void handleCameraStatus() {
  String json = "{";
  json += "\"active\":" + String(cameraActive ? "true" : "false") + ",";
  json += "\"frames\":" + String(frameCount) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void camera_setupRoutes() {
  server.on("/", handleCameraRoot);
  server.on("/capture", handleCameraCapture);
  server.on("/stream", handleCameraStream);
  server.on("/status", handleCameraStatus);
}

// Note: Do NOT include setup() or loop() functions in this file
