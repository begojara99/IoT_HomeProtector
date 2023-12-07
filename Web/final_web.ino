#define SWAP 0  // sw access point

// Load Wi-Fi library
#include <WiFi.h>
#include "time.h"
#include <EEPROM.h>
#include <ESP32Servo.h>
#include <Arduino_JSON.h>
#define EEPROM_SIZE 256


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

/////
int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
char payload[512];
char rcvdPayload[512];
bool http_response = false;
String http_buffer;

int capture_interval = 14000; // Microseconds between captures
bool internet_connected = false;
long current_millis;
long last_capture_millis = 0;
//////

int n_pairs, r_index = 3;
int freq_val, dur_val;
int n_freq[] = { 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 446, 494, 523 };
int n_dur[] = { 2000, 1500, 1000, 750, 500, 375, 250 };


static const int servoPin = 27;

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
#if SWAP
const char* ssid = "ESP32-AP";
const char* password = "123456789";  // password should be long!!
#else
const char* ssid = "kau-seminar";
const char* password = "kau-seminar";

// const char* ssid = "Galaxy Quantum250_F5_4B";
// const char* password = "77626009";

// const char* ssid = "iPhone";
// const char* password = "wldnwldn";


#endif

// Set web server port number to 80
WiFiServer server(80);
WiFiClient client;

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
  client.println("<script>var totalTime=" + String(serverInfo.tm_hour * 3600 + serverInfo.tm_min * 60 + serverInfo.tm_sec) + "; setInterval(function(){totalTime++; document.getElementById('timer').innerHTML='NowTime: '+Math.floor(totalTime/3600) + ':' + Math.floor(totalTime%3600/60) + ':' + totalTime%3600%60;}, 1000);</script>");
  client.println(&serverInfo, "<h2 id='timer'>NowTime: %H:%M:%S</h2>");
  serverYear = serverInfo.tm_year + 1900;
  serverMonth = serverInfo.tm_mon + 1;
  serverDay = serverInfo.tm_mday;
  client.println("Year: " + String(serverInfo.tm_year + 1900) + ", Month: " + String(serverInfo.tm_mon + 1) + ", Day: " + String(serverInfo.tm_mday));

  client.println("<hr/>");
}

void timeSettings() {

  client.println("<h2> SET TIMER </h2> ");

  //시작시간 세팅
  client.println("<form method=\"get\">");
  client.println("<h4> Set Start Time </h4> ");
  client.println("<p><label>Date: <input type = \"date\" name =\"setStartDate\" ></label></p>");
  client.println("<p><label>Time : <input type = \"time\" name = \"setStartTime\"></label></p>");
  //끝 시간 세팅
  client.println("<h4> Set End Time </h4> ");
  client.println("<p><label>Date: <input type = \"date\" name =\"setEndDate\" ></label></p>");
  client.println("<p><label>Time : <input type = \"time\" name = \"setEndTime\"></label></p>");
  client.println("<p><input type=\"submit\" value=\"Alarm settings\"></p></form>");
  client.println("<hr/>");
}

void nightTimeSettings() {
  client.println("<h2> SET LIGHT </h2> ");
  client.println("<form method=\"get\">");
  client.println("<h4> Set Start Time </h4> ");
  client.println("<p><label>Time : <input type = \"time\" name = \"setNightStartTime\"></label></p>");
  client.println("<p><label>Time : <input type = \"time\" name = \"setNightEndTime\"></label></p>");
  client.println("<p><input type=\"submit\" value=\"Alarm settings\"></p></form>");
  client.println("<hr/>");
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


void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);

  myservo.attach(servoPin);
  myservo.write(0);
  delay(1000);
  // Initialize the output variables as outputs


#if SWAP
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
#else
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
#endif
  server.begin();
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  ledcAttachPin(BUZ_PIN, LED_CHANNEL);
}

void loop() {

  // CAM 연결 코드
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
    }
    if (serial_buffer.equals("esp_http_client_get_status_code: 200\r")) {
      http_response = true;
    }
  }
  current_millis = millis();
  if (current_millis - last_capture_millis > capture_interval) { // Take another picture
    last_capture_millis = millis();
    Serial.println("Sensor detected and Capture");
  }
  //////////////////////


  client = server.available();  // Listen for incoming clients
  if (client) {                 // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");                                             // print a message out in the serial port
    String currentLine = "";                                                   // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the cliendt knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


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
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50;border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            // Web Page Heading
            client.println("<body><h1>ESP32 Iot Project</h1>");

            client.println("<p>Buzz Status : " + buzzState + "</p>");
            if (buzzState == "off") {
              client.println("<p><a href=\"/23/on\"><button class=\"button button2\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/23/off\"><button class=\"button\">OFF</button></a></p>");
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
              client.println("<h2>The Timer is not set. Set the Timer.</h2>");
            } else {
              client.println("<h2>Setting Time " + String(startYear) + "/" + String(startMonth) + "/" + String(startDay) + " " + String(startHour) + ":" + String(startMinute) + "~" + String(endYear) + "/" + String(endMonth) + "/" + String(endDay) + " " + String(endHour) + ":" + String(endMinute) + "</h2>");
            }
            if (!nightStartHour && !nightStartMin && !nightEndHour && !nightEndMin) {
              client.println("<h2>The alarm is not set. Set the alarm.</h2>");
            } else {
              client.println("<h2>AlarmTime: " + String(nightStartHour) + ":" + String(nightStartMin) + "~" + String(nightEndHour) + ":" + String(nightEndMin) + "</h2>");
            }

            client.println("<hr/>");
            client.println("<h3>Home Cam</h3>");
            // 카메라 연결 IP주소 
            client.println("<img id=\"stream\" src=\"http://192.168.0.3:81/stream\" crossorigin >");
            


            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
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
      }  //* if (client.available()) {
    }    //** while
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }  //** if (client) {
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
