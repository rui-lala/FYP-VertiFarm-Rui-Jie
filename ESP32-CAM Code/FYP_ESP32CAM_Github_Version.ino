#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"
#include "time.h"

// Wi-Fi credentials
const char *ssid = "WIFI SSID";
const char *password = "WIFI PASSWORD";
// Time Configuration 
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; 
const int daylightOffset_sec = 0;

// Google Script Info
const char* myDomain = "script.google.com";
String myScript = "/macros/s/XXXXXXXXxxxxxxxxxxxxxxxxxxxxxxxxxxxx/exec"; //change to your own script ID from Google

// ==========================================
// CHANGE THIS FOR EACH CAMERA
// ==========================================
#define CAMERA_ID "CAM_1" 
// ==========================================

// Timing & Sleep
uint64_t sleepDurationMin = 5; 

// Google Script Parameter Helpers
String mimeType = "&mimetype=image/jpeg";
String myImage = "&cameraId=" + String(CAMERA_ID) + "&data=";

// Camera Pin Definition (AI-Thinker)
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

String getTimestampedFilename() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "filename=" + String(CAMERA_ID) + "_error.jpg"; 
  }
  char filename[40];
  strftime(filename, sizeof(filename), "filename=%Y%m%d_%H%M%S.jpg", &timeinfo);
  return String(CAMERA_ID) + "_" + String(filename);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  delay(1000);

  Serial.printf("\n--- Wake Up %s ---\n", CAMERA_ID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Time Sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing time...");
  delay(2000); 

  // Camera Config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_XGA; // adjust accordingly for the resolution
  config.jpeg_quality = 12; //        
  config.fb_count = 1;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed. Restarting...");
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_dcw(s, 0);
    s->set_wb_mode(s, 3); // Office
    s->set_brightness(s, 1);
    s->set_hmirror(s, 1); // Flip horizontally
    s->set_vflip(s, 1);   // Flip vertically 
  }

  Serial.println("Warming up sensor...");
  for (int i = 0; i < 10; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(200);
  }

  saveCapturedImage();

  Serial.printf("Entering Deep Sleep for %llu minute...\n", sleepDurationMin);
  esp_sleep_enable_timer_wakeup(sleepDurationMin * 60 * 1000000ULL); 
  esp_deep_sleep_start();
}

void loop() {}

void saveCapturedImage() {
  Serial.println("Connecting to Google Script...");
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30000); // 30 second internal timeout

  // TRY TO CONNECT 3 TIMES
  bool connected = false;
  for(int i=0; i<3; i++) {
    if (client.connect(myDomain, 443)) {
      connected = true;
      break;
    }
    Serial.println("Connection failed, retrying...");
    delay(2000);
  }

  if (connected) {
    Serial.println("Connection successful!");

    camera_fb_t * fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Encoding image
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "";
    Serial.println("Encoding image...");
    for (int i = 0; i < fb->len; i++) {
      base64_encode(output, (input++), 3);
      if (i % 3 == 0) imageFile += urlencode(String(output));
      if (i % 1000 == 0) yield(); 
    }

    String Data = getTimestampedFilename() + mimeType + myImage;
    esp_camera_fb_return(fb);
    
    Serial.println("Sending image to Google Drive...");
    client.println("POST " + myScript + " HTTP/1.1");
    client.println("Host: " + String(myDomain));
    client.println("Content-Length: " + String(Data.length() + imageFile.length()));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: close");
    client.println();
    
    client.print(Data);
    // Send in chunks to prevent memory overflow
    for (int Index = 0; Index < imageFile.length(); Index += 1000) {
      client.print(imageFile.substring(Index, Index + 1000));
      yield();
    }

    Serial.println("Waiting for server response...");
    long int StartTime = millis();
    while (!client.available()) {
      delay(200);
      if ((millis() - StartTime) > 30000) {
        Serial.println("Timeout - No response from Google.");
        break;
      }
    }
    
    while (client.available()) {
      Serial.print((char)client.read());
    }
    Serial.println("\nUpload finished.");
    client.stop();

  } else {
    Serial.println("Connection to Google failed after retries!");
  }
}

String urlencode(String str) {
  String encodedString = "";
  char c, code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) encodedString += c;
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%'; encodedString += code0; encodedString += code1;
    }
    yield();
  }
  return encodedString;
}
