#include "esp_http_client.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "Arduino.h"
#include "Base64.h"
#include "mbedtls/base64.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

#define MAX_HTTP_BUFFER_SIZE 1024

char http_buffer[MAX_HTTP_BUFFER_SIZE];
int http_buffer_offset;

const char* ssid = "KAU-Guest";
const char* password = "";

int capture_interval = 10000; // Microseconds between captures
bool internet_connected = false;
long current_millis;
long last_capture_millis = 0;

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  // Connecting to WiFi
  if (init_wifi()) {
    internet_connected = true;
    //    Serial.println("Internet connected");
    startCameraServer();
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
  }
}

bool init_wifi() {
  int connAttempts = 0;
  //  Serial.println("\r\nConnecting to: " + String(ssid));
  WiFi.begin(ssid, password);
  //  Serial.print(".");
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    //    Serial.print(".");
    if (connAttempts > 10) {
      //      Serial.println("\nConnection failed");
      return false;
    }
    connAttempts++;
  }
  //  Serial.println("");
  return true;
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      Serial.println("HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      Serial.println("HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      Serial.println();
      Serial.printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      Serial.println();
      Serial.printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // Process http response data
        if (http_buffer_offset + evt->data_len < MAX_HTTP_BUFFER_SIZE) {
          memcpy(&http_buffer[http_buffer_offset], evt->data, evt->data_len);
          http_buffer_offset += evt->data_len;
        }
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      Serial.println("");
      Serial.println("HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      Serial.println("HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

static esp_err_t take_send_photo() {
  Serial.println("Taking picture...");
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return ESP_FAIL;
  }

  int image_buf_size = 4000 * 1000;
  uint8_t *image = (uint8_t *)ps_calloc(image_buf_size, sizeof(char));

  size_t length = fb->len;
  size_t olen;
  Serial.print("Image length is ");
  Serial.println(length);
  int err1 = mbedtls_base64_encode(image, image_buf_size, &olen, fb->buf, length);

  esp_http_client_handle_t http_client;
  esp_http_client_config_t config_client = {0};

  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP);
  timeClient.begin();
  timeClient.update();
  String Time =  String(timeClient.getEpochTime());
  String MAC = String(WiFi.macAddress());
  Serial.print("Time: " );
  Serial.print(Time);
  Serial.print(", MAC: ");
  Serial.println(MAC);

  String post_url2 = "https://cvle4x2ofl.execute-api.ap-northeast-2.amazonaws.com/alpha/" + MAC + "/" + Time; // Location where images are POSTED
  char post_url3[post_url2.length() + 1];
  post_url2.toCharArray(post_url3, sizeof(post_url3));

  config_client.url = post_url3;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;

  http_client = esp_http_client_init(&config_client);
  esp_http_client_set_post_field(http_client, (const char *)fb->buf, fb->len);
  esp_http_client_set_header(http_client, "Content-Type", "image/jpeg");
  esp_err_t err = esp_http_client_perform(http_client);

  if (err == ESP_OK) {
    Serial.print("esp_http_client_get_status_code: ");
    Serial.println(esp_http_client_get_status_code(http_client));
  }

  Serial.printf("%s\n", http_buffer);
  memset(http_buffer, 0, sizeof(http_buffer));
  http_buffer_offset = 0;

  esp_http_client_cleanup(http_client);

  esp_camera_fb_return(fb);
}

void loop()
{
  // Taking picture every 10 seconds
  current_millis = millis();
  if (current_millis - last_capture_millis > capture_interval) {
    last_capture_millis = millis();
    take_send_photo();
  }
  // Taking picture when an object is detected by sensors
  if (Serial.available()) {
    String serial_buffer = Serial.readStringUntil('\n');
    if (serial_buffer.equals("Sensor detected and Capture\r")) {
      last_capture_millis = millis();
      take_send_photo();
    }
  }
}
