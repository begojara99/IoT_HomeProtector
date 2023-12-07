#include <WiFi.h>
#include <AWS_IOT.h>
#include <Arduino_JSON.h>
#include "time.h"
#include <EEPROM.h>
#include <ESP32Servo.h>
#include "Arduino.h"
#include "Audio.h"

#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26
#define EEPROM_SIZE 256

Audio audio;

const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;

int audio_interval = 5000; // 5 seconds
long new_audio_millis = 0;
long old_audio_millis = -5000;

// PIR motion sensor settings
const int MOTION_PIN = 23;
volatile bool motion_detected;

// Ultrasonic sensor settings
const int TRIG_PIN = 19;
const int ECHO_PIN = 21;
const int ULTRA_INTERVAL = 3000;
const int ULTRA_DEVIATION = 20;
long old_distance, new_distance;
unsigned long ultra_old_trigger;
unsigned long ultra_new_trigger;
volatile bool ultra_detected;
bool ultra_set;

AWS_IOT homeProtector;

//const char* ssid = "Realmadrid";
//const char* password = "kingofmadrid";
const char* ssid = "kau-seminar";
const char* password = "kau-seminar";
char HOST_ADDRESS[] = "a2rbe83eau7cu5-ats.iot.ap-northeast-2.amazonaws.com";
char CLIENT_ID[] = "Home_Protector";
char sTOPIC_NAME[] = "$aws/things/Home_Protector/shadow/update/delta"; // subscribe topic name
char pTOPIC_NAME1[] = "$aws/things/Home_Protector/shadow/update"; // publish topic name 1
char pTOPIC_NAME2[] = "Home_Protector/images/upload"; // publish topic name 2

Servo myservo;
int cnt = 1;

// ready to buzz
const int LED_CHANNEL = 1;
const int LED_PIN = 17;   // Timer 켜졌을 때
const int LED_PIN2 = 16;  // Alarm 켜졌을 때
const int RESOLUTION = 8;
const int BUZ_PIN = 22;
const int DUTY = 8;
String buzzState = "off";

int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
char payload[512];
char rcvdPayload[512];
bool http_response = false;
String http_buffer;

const int CAPTURE_INTERVAL = 14000; // Microseconds between captures
bool internet_connected = false;
bool capture_trigger = false;
unsigned long new_capture_millis;
unsigned long old_capture_millis;

int n_pairs, r_index = 3;
int freq_val, dur_val;
int n_freq[] = { 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 446, 494, 523 };
int n_dur[] = { 2000, 1500, 1000, 750, 500, 375, 250 };


static const int servoPin = 14;

//부저 울리는 함수
void playNote(int note, int dur) {
  ledcSetup(LED_CHANNEL, n_freq[note], RESOLUTION);
  ledcWrite(LED_CHANNEL, DUTY);
  Serial.println(note);
  delay(dur);
}

//부저 끄는 함수
void stopNote() {
  ledcWrite(LED_CHANNEL, 0);
}


// Replace with your network credentials
//const char* ssid = "kau-seminar";
//const char* password = "kau-seminar";

// const char* ssid = "Galaxy Quantum250_F5_4B";
// const char* password = "77626009";

// const char* ssid = "iPhone";
// const char* password = "wldnwldn";

// Set web server port number to 80
WiFiServer server(80);
WiFiClient wifiClient;

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9;  // 3600
const int daylightOffset_sec = 0;     // 3600



int serverYear, serverMonth, serverDay;
int serverTotalTime, serverHours, serverMinutes, serverSeconds;
struct tm serverInfo;


unsigned long convertToMinutes(int year, int month, int day, int hours, int minutes) {
  // 1년 = 365일, 윤년 고려 안함

  // Serial.println("입력 받은 값 :: "+String(year) + "년" +String(month) +"월" + String(day)+"일   " + String(hours)+"시"+String(minutes)+"분");
  unsigned long totalMinutes = (year - 1) * 365 * 24 * 60;
  totalMinutes += (month - 1) * 30 * 24 * 60;  //간단 예제 -> 1달 = 30일 가정 ..
  totalMinutes += (day - 1) * 24 * 60;
  totalMinutes += hours * 60;
  totalMinutes += minutes;

  return totalMinutes;
}

bool isTimeInRange(int year, int month, int day, int hours, int minutes, int startYear, int startMonth, int startDay, int startHour, int startMinute, int endYear, int endMonth, int endDay, int endHour, int endMinute) {
  unsigned long startTime = convertToMinutes(startYear, startMonth, startDay, startHour, startMinute);
  unsigned long endTime = convertToMinutes(endYear, endMonth, endDay, endHour, endMinute);
  unsigned long currentTime = convertToMinutes(year, month, day, hours, minutes);

  // Serial.println("isTime In Range함수인데요 ");
  //Serial.println("startTime" + String(startTime));
  //Serial.println("endTime" + String(endTime));
  //Serial.println("currentTime"+String(currentTime));

  //Serial.println("return 값 : " + String(currentTime >= startTime && currentTime <= endTime));

  return (currentTime >= startTime && currentTime <= endTime);
}


void printLocalTime() {
  if (!getLocalTime(&serverInfo)) {
    Serial.println("Failed to obtain serverTime");
    return;
  }
  serverTotalTime = serverInfo.tm_hour * 3600 + serverInfo.tm_min * 60 + serverInfo.tm_sec;
  Serial.println(&serverInfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println("Year: " + String(serverInfo.tm_year + 1900) + ", Month: " + String(serverInfo.tm_mon + 1));
  Serial.println();
  wifiClient.println("<script>var totalTime=" + String(serverInfo.tm_hour * 3600 + serverInfo.tm_min * 60 + serverInfo.tm_sec) + "; setInterval(function(){totalTime++; document.getElementById('timer').innerHTML='NowTime: '+Math.floor(totalTime/3600) + ':' + Math.floor(totalTime%3600/60) + ':' + totalTime%3600%60;}, 1000);</script>");
  wifiClient.println(&serverInfo, "<h2 id='timer'>NowTime: %H:%M:%S</h2>");
  serverYear = serverInfo.tm_year + 1900;
  serverMonth = serverInfo.tm_mon + 1;
  serverDay = serverInfo.tm_mday;
  wifiClient.println("Year: " + String(serverInfo.tm_year + 1900) + ", Month: " + String(serverInfo.tm_mon + 1) + ", Day: " + String(serverInfo.tm_mday));

  wifiClient.println("<hr/>");
}

void timeSettings() {

  wifiClient.println("<h2> SET TIMER </h2> ");

  //시작시간 세팅
  wifiClient.println("<form method=\"get\">");
  wifiClient.println("<h4> Set Start Time </h4> ");
  wifiClient.println("<p><label>Date: <input type = \"date\" name =\"setStartDate\" ></label></p>");
  wifiClient.println("<p><label>Time : <input type = \"time\" name = \"setStartTime\"></label></p>");
  //끝 시간 세팅
  wifiClient.println("<h4> Set End Time </h4> ");
  wifiClient.println("<p><label>Date: <input type = \"date\" name =\"setEndDate\" ></label></p>");
  wifiClient.println("<p><label>Time : <input type = \"time\" name = \"setEndTime\"></label></p>");
  wifiClient.println("<p><input type=\"submit\" value=\"Alarm settings\"></p></form>");
  wifiClient.println("<hr/>");
}

void nightTimeSettings() {
  wifiClient.println("<h2> SET LIGHT </h2> ");
  wifiClient.println("<form method=\"get\">");
  wifiClient.println("<h4> Set Start Time </h4> ");
  wifiClient.println("<p><label>Time : <input type = \"time\" name = \"setNightStartTime\"></label></p>");
  wifiClient.println("<p><label>Time : <input type = \"time\" name = \"setNightEndTime\"></label></p>");
  wifiClient.println("<p><input type=\"submit\" value=\"Alarm settings\"></p></form>");
  wifiClient.println("<hr/>");
}

/* start date - time */
int startYear, startMonth, startDay;
int startHour, startMinute;

/* end date - time */
int endYear, endMonth, endDay;
int endHour, endMinute;



void extractTime() {
  // Extract alarmHours
  Serial.println("func extractTime ");

  String tmp = "";

  String startDate = "";

  int i = header.indexOf("setStartDate") + 13;

  while (isDigit(header[i]) || header[i] == '-') {
    tmp += header[i++];
  }

  startDate = tmp;
  i++;
  tmp = "";
  Serial.println("Start Date : " + startDate);

  startYear = startDate.substring(0, 4).toInt();
  startMonth = startDate.substring(5, 7).toInt();
  startDay = startDate.substring(8, 10).toInt();


  String startTime = "";
  // i += header.indexOf("setStartTime")+13;
  i += 13;
  Serial.println("now i : " + String(i) + ", header[i] = " + String(header[i]) + String(header[i + 1]));

  while (isDigit(header[i]) || header[i] == '%' || header[i] == 'A') {
    tmp += header[i++];
  }

  startTime = tmp;
  i++;
  tmp = "";

  startHour = startTime.substring(0, 2).toInt();
  startMinute = startTime.substring(5, 7).toInt();
  Serial.println("Start Time : " + String(startHour) + ":" + String(startMinute));


  String endDate = "";
  String endTime = "";
  i += 11;
  while (isDigit(header[i]) || header[i] == '-') {
    tmp += header[i++];
  }
  i++;
  endDate = tmp;
  tmp = "";
  Serial.println("End Date : " + endDate);

  endYear = endDate.substring(0, 4).toInt();
  endMonth = endDate.substring(5, 7).toInt();
  endDay = endDate.substring(8, 10).toInt();

  i += 11;

  while (isDigit(header[i]) || header[i] == '%' || header[i] == 'A') {
    tmp += header[i++];
  }
  i++;
  endTime = tmp;
  tmp = "";

  //%3A인코딩 안된 것 (:)임 => 나중에 시간 계산
  endHour = endTime.substring(0, 2).toInt();
  endMinute = endTime.substring(5, 7).toInt();
  Serial.println("setEndTime : " + String(endHour) + ":" + String(endMinute));
}

/* 알람 시간 */
int nightStartHour, nightStartMin;

int nightEndHour, nightEndMin;

void extractNightTime() {
  String tmp = "";


  int i = header.indexOf("setNightStartTime") + 18;

  while (isDigit(header[i]) || header[i] == '%' || header[i] == 'A') {
    tmp += header[i++];
  }

  i++;
  Serial.println("tmp : " + tmp);

  // 추출된 문자열을 시간과 분으로 분리

  nightStartHour = tmp.substring(0, 2).toInt();
  nightStartMin = tmp.substring(5, 7).toInt();

  tmp = "";
  Serial.println("Extracted Night Time: " + String(nightStartHour) + ":" + String(nightStartMin));


  i += 16;  //setNightEndTime
  while (isDigit(header[i]) || header[i] == '%' || header[i] == 'A') {
    tmp += header[i++];
  }


  nightEndHour = tmp.substring(0, 2).toInt();
  nightEndMin = tmp.substring(5, 7).toInt();
  Serial.println("Extracted End Time: " + String(nightEndHour) + ":" + String(nightEndMin));
}

// PIR motion sensor interrupt handler
void IRAM_ATTR detectMotion() {
  if ((ultra_new_trigger - ultra_old_trigger) > ULTRA_INTERVAL) {
    motion_detected = true;
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);

  myservo.attach(servoPin);
  myservo.write(0);
  delay(1000);
  // Initialize the output variables as outputs

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // Connecting to AWS
  if (homeProtector.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    Serial.println("Connected to AWS");
    delay(2000);
  }
  // PIR motion sensor setup
  // MOTION_PIN mode INPUT_PULLUP
  pinMode(MOTION_PIN, INPUT_PULLUP);
  // Set MOTION_PIN as interrupt, assign interrupt function and set RISING mode
  attachInterrupt(digitalPinToInterrupt(MOTION_PIN), detectMotion, RISING);
  // Ultrasonic sensor setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // web server settings
  server.begin();
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  ledcAttachPin(BUZ_PIN, LED_CHANNEL);
  // audio settings
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(5);
  audio.connecttohost("http://vis.media-ice.musicradio.com/CapitalMP3");
}

void loop() {
  // Sensor detection
  if (motion_detected || ultra_detected) {
    capture_trigger = true;
    Serial.println("Sensor detected");
    motion_detected = false;
    ultra_detected = false;
    ultra_set = true;
    ultra_old_trigger = millis();
  }
  ultra_new_trigger = millis();
  if ((ultra_new_trigger - ultra_old_trigger) > ULTRA_INTERVAL) {
    // Triggering by 10us pulse
    digitalWrite(TRIG_PIN, LOW); // Trigger low for 2us
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); // Trigger high for 10us
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration, distance;
    // Getting duration for echo pulse
    duration = pulseIn(ECHO_PIN, HIGH);
    // sound speed = 340m/s = 34/1000cm/us
    // distance = duration * 34/1000 * 1/2
    distance = duration * 17 / 1000;
    old_distance = new_distance;
    new_distance = distance;
    // Prevent redundant ultrasonic sensor detection
    if (ultra_set) {
      ultra_set = false;
      // Set the new distance to old distance
      old_distance = new_distance;
    }
    // Check if ultrasonic sensor detcted objects
    if (abs(new_distance - old_distance) > ULTRA_DEVIATION) {
      ultra_detected = true;
      ultra_old_trigger = millis();
    }
  }
  // Read data from ESP32-CAM using UART communication
  if (Serial.available()) {
    String serial_buffer = Serial.readStringUntil('\n');
    Serial.println(serial_buffer);
    if (http_response) {
      http_response = false;
      http_buffer = serial_buffer;
      // JSON parsing process
      JSONVar httpObject = JSON.parse(http_buffer);
      const char* body = (const char*)httpObject["body"];
      httpObject = JSON.parse(body);
      const char* objectURL = (const char*)httpObject["Location"];
      Serial.println(objectURL);
      // sprintf(payload, "{\"location\":\"%s\"}", objectURL);
      sprintf(payload, "%s", objectURL);
      if (homeProtector.publish(pTOPIC_NAME2, payload) == 0) {
        Serial.print("Publish Message:");
        Serial.println(payload);
      }
      else
        Serial.println("Publish failed");
    }
    if (serial_buffer.equals("esp_http_client_get_status_code: 200\r")) {
      http_response = true;
    }
  }
  // Sensor detected and Capture
  new_capture_millis = millis();
  if (capture_trigger || (new_capture_millis - old_capture_millis) > CAPTURE_INTERVAL) {
    capture_trigger = false;
    old_capture_millis = millis();
    Serial.println("Sensor detected and Capture");
  }
  // Sound detection and play music
  sample = analogRead(36);  // 아날로그 핀
  if (sample < 50) {
    new_audio_millis = millis();
    old_audio_millis = millis();
  }
  if (new_audio_millis - old_audio_millis < audio_interval) {
    new_audio_millis = millis();
    audio.loop();
    Serial.println("Playing");
  }
  // Web server page
  wifiClient = server.available();  // Listen for incoming clients
  if (wifiClient) {                 // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");                                             // print a message out in the serial port
    String currentLine = "";                                                   // make a String to hold incoming data from the client
    while (wifiClient.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (wifiClient.available()) {  // if there's bytes to read from the client,
        char c = wifiClient.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the cliendt knows what's coming, then a blank line:
            wifiClient.println("HTTP/1.1 200 OK");
            wifiClient.println("Content-type:text/html");
            wifiClient.println("Connection: close");
            wifiClient.println();


            // buzzer on/off
            if (header.indexOf("GET /23/on") >= 0) {
              Serial.println("GPIO 23 on");
              buzzState = "on";
              playNote(4, 0);
            } else if (header.indexOf("GET /23/off") >= 0) {
              Serial.println("GPIO 23 off");
              // 부저off
              stopNote();
              buzzState = "off";
            }




            // Display the HTML web page
            wifiClient.println("<!DOCTYPE html><html>");
            wifiClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            wifiClient.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            wifiClient.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            wifiClient.println(".button { background-color: #4CAF50;border: none; color: white; padding: 16px 40px;");
            wifiClient.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            wifiClient.println(".button2 {background-color: #555555;}</style></head>");
            // Web Page Heading
            wifiClient.println("<body><h1>ESP32 Iot Project</h1>");

            wifiClient.println("<p>Buzz Status : " + buzzState + "</p>");
            if (buzzState == "off") {
              wifiClient.println("<p><a href=\"/23/on\"><button class=\"button button2\">ON</button></a></p>");
            } else {
              wifiClient.println("<p><a href=\"/23/off\"><button class=\"button\">OFF</button></a></p>");
            }





            // Display local time
            printLocalTime();

            // AlarmSetting
            timeSettings();


            nightTimeSettings();
            // Extract time from alarmSettings!
            if (header.indexOf("setStartDate") != -1 && header.indexOf("setStartTime") != -1 && header.indexOf("setEndDate") != -1 && header.indexOf("setEndTime") != -1) {
              extractTime();
            }
            if (header.indexOf("setNightStartTime") != -1 || header.indexOf("setNightEndTime") != -1) {
              extractNightTime();
            }

            if (!startYear && !startHour && !endYear && !endHour) {
              wifiClient.println("<h2>The Timer is not set. Set the Timer.</h2>");
            } else {
              wifiClient.println("<h2>Setting Time " + String(startYear) + "/" + String(startMonth) + "/" + String(startDay) + " " + String(startHour) + ":" + String(startMinute) + "~" + String(endYear) + "/" + String(endMonth) + "/" + String(endDay) + " " + String(endHour) + ":" + String(endMinute) + "</h2>");
            }
            if (!nightStartHour && !nightStartMin && !nightEndHour && !nightEndMin) {
              wifiClient.println("<h2>The alarm is not set. Set the alarm.</h2>");
            } else {
              wifiClient.println("<h2>AlarmTime: " + String(nightStartHour) + ":" + String(nightStartMin) + "~" + String(nightEndHour) + ":" + String(nightEndMin) + "</h2>");
            }

            wifiClient.println("<hr/>");
            wifiClient.println("<h3>Home Cam</h3>");
            // 카메라 연결 IP주소
            wifiClient.println("<img id=\"stream\" src=\"http://192.168.56.203:81/stream\" crossorigin >");



            wifiClient.println("</body></html>");
            // The HTTP response ends with another blank line
            wifiClient.println();
            // Break out of the while loop
            break;
          }       //** if (currentLine.length() == 0) {
          else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }                      //** if (c == '\n') {
        else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;    // add it to the end of the currentLine
        }
      }  //* if (wifiClient.available()) {
    }    //** while
    // Clear the header variable
    header = "";
    // Close the connection
    wifiClient.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }  //** if (wifiClient) {
  // The alarm goes off
  serverHours = serverTotalTime / 3600;
  serverMinutes = serverTotalTime % 3600 / 60;
  serverSeconds = serverTotalTime % 3600 % 60;

  // Increase serverTotalTime per second
  currentTime = millis();
  if (currentTime - previousTime >= 1000) {
    previousTime = currentTime;
    serverTotalTime++;
  }

  /// 시간 비교

  if (isTimeInRange(serverYear, serverMonth, serverDay, serverHours, serverMinutes, startYear, startMonth, startDay, startHour, startMinute, endYear, endMonth, endDay, endHour, endMinute)) {
    // 시작과 종료 시간 사이
    //Serial.println("시작- 종료 사이 실행 실행 ");

    // play_note(0, 3);
    // NIGHT 알람 설정
    if (
      (serverHours > nightStartHour || (serverHours == nightStartHour && serverMinutes >= nightStartMin)) && (serverHours < nightEndHour || (serverHours == nightEndHour && serverMinutes <= nightEndMin))) {
      if (nightStartHour == serverHours && nightStartMin == serverMinutes) {
        if (cnt == 1) {
          for (int pos = 0; pos <= 180; pos += 1) {
            myservo.write(pos);
            Serial.print("1~");
            delay(15);
          }
          for (int pos = 180; pos >= 0; pos -= 1) {
            myservo.write(pos);
            Serial.print("2!");
            delay(15);
          }
          cnt += 1;
        }
      } else if (nightEndHour == serverHours && nightEndMin == serverMinutes) {
        if (cnt == 2) {
          for (int pos = 0; pos <= 180; pos += 1) {
            Serial.println(" 서보모터 돌아감 Night 끝 1111~~~1~1~1~1~!! ");
            myservo.write(pos);
            delay(15);
          }
          for (int pos = 180; pos >= 0; pos -= 1) {
            Serial.println(" 서보모터 돌아감 Night 끝2222 ~~~~~~~2~2~!! ");
            myservo.write(pos);
            delay(15);
          }
          cnt += 1;
        }
      }

      //나이트 알람 체크 함수 실행
      //Serial.println("함수 내 Night ALARM에 맞춰 실행");

    } else {
      // stopNote();
    }
    // ---------------------------------


  } else {
    // 시작과 종료 시간 사이X
    // noTone(BUZ_PIN);
    //Serial.println("시작 -종료 를 벗어난 시간대 실행안해 안해 ");
  }








  // -----------------------
  // 현재 시간이 nightStartTime과 nightEndTime 사이에 있으면 LED를 켬
  //digitalWrite(LED_PIN, HIGH);
  // 그 외의 시간에는 LED를 끔
  //digitalWrite(LED_PIN, LOW);
  // -----------------------
}
