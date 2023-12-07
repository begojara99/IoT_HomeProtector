#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"

#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

Audio audio;

String ssid = "KAU-Guest";
String password = "";

void setup() {
  Serial.begin(115200);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED)
  Serial.println("@");
  delay(1500);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(50);
  
  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Olsen-Banden.mp3");
}

void loop() {
  audio.loop();
}
