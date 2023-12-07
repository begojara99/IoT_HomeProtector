#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

const char *ssid = "KAU-Guest"; // WiFi SSID 입력
//const char *password = "12345678"; // WiFi 패스워드 입력

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org"); // NTP 서버 주소 추가
Servo myservo;

int delayTime = 5000; // 5초 지연 시간

void setup() {
  Serial.begin(115200); // 시리얼 통신 시작
  Serial.println("시리얼 통신을 시작합니다...");

  // 서보 모터 연결
  myservo.attach(13); // 서보 모터를 D13에 연결
  Serial.println("서보 모터가 연결되었습니다.");

  // WiFi 연결
  WiFi.begin(ssid);
  //WiFi.begin(ssid, password);
  Serial.println("WiFi 연결을 시도합니다...");

  int maxAttempts = 10; // 최대 시도 횟수
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi에 연결할 수 없습니다. 설정을 확인해 주세요.");
    return; // WiFi에 연결되지 않으면 프로그램 종료
  } else {
    Serial.println("WiFi에 성공적으로 연결되었습니다.");
  }

  // NTP 클라이언트 시작
  timeClient.begin();
  timeClient.setTimeOffset(3600 * 9); // 한국 시간대 (UTC+9)
  Serial.println("NTP 클라이언트가 시작되었습니다.");
}

void operateServo() {
  timeClient.update();
  int hours = timeClient.getHours();
  Serial.print("현재 시간: ");
  Serial.println(hours);

  if (hours >= 12 && hours < 16) {
    Serial.println("서보 모터 작동 시작...");
    for (int pos = 0; pos <= 180; pos += 1) {
      myservo.write(pos);
      delay(15);
    }

    for (int pos = 180; pos >= 0; pos -= 1) {
      myservo.write(pos);
      delay(15);
    }

    Serial.println("서보 모터 작동 종료.");
    delay(delayTime);
  } else {
    Serial.println("서보 모터 작동 시간이 아닙니다.");
    delay(1000);
  }
}

void loop() {
  operateServo();
}
