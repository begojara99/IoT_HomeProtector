#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26
Audio audio;

String ssid = "KAU-Guest";
String password = "";

const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;

int capture_interval = 4000;  // Microseconds between captures
long current_millis = 0;
long last_capture_millis = -4000;

void setup() {
  Serial.begin(115200);

  // 오디오 설정
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED)
  delay(1500);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(50);
  audio.connecttohost("http://vis.media-ice.musicradio.com/CapitalMP3");
  //  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Olsen-Banden.mp3");
}

void loop() {
  sample = analogRead(36);  // 아날로그 핀
  Serial.println(sample);

  if (sample < 50) {
    current_millis = millis();
    last_capture_millis = millis();
  }
  if (current_millis - last_capture_millis < capture_interval) {
    current_millis = millis();
    audio.loop();
    Serial.println("@@@");
  }
}
