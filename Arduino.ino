#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const int lightPin = 35;  // 조도센서가 연결된 핀 번호

const char* ssid = "";      // Wi-Fi 네트워크 이름
const char* password = "";  // Wi-Fi 비밀번호

const char* user_id = "";
const char* user_pw = "";

const char* serverUrl = "http://localhost:8080/api/authenticate";    // 서버 URL

String Token = ""; // 전역 변수로 토큰 선언

WiFiUDP ntpUDP; //USP 소켓 생성
NTPClient timeClient(ntpUDP, "pool.ntp.org"); //NTP 클라이언트 생성

void setup() {
  Serial.begin(9600);

  // Wi-Fi 연결 시작
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  // Wi-Fi 연결 확인
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());

  // NTP 클라이언트 초기화
  timeClient.begin();

  // 로그인 요청 보내기
  Token = loginRequest();

  pinMode(lightPin, INPUT);  // 34번 핀을 디지털 입력으로 설정

  // Sync time
  syncTime();
}

void loop() {
  // 센서 데이터 수집
  float airTemp = 456.456;
  float airHumid = 456.456;
  float soilHumid = 999.999;
  float lightIntensity = analogRead(lightPin) / 1023.0 * 100.0;
  bool status = true;

  // 현재 시간 가져오기
  timeClient.update();

  // 현재 시간을 Unix 타임스탬프로 가져오기
  time_t currentTime = timeClient.getEpochTime();

  // 시간을 GMT+9로 조정하기
  currentTime += 9 * 3600;

  // Unix 타임스탬프를 struct tm으로 변환하기
  struct tm *timeInfo;
  timeInfo = localtime(&currentTime);

  // 날짜 구성 요소 가져오기
  int year = timeInfo->tm_year + 1900;
  int month = timeInfo->tm_mon + 1;
  int day = timeInfo->tm_mday;

  // 시간 구성 요소 가져오기
  int hour = timeInfo->tm_hour;
  int minute = timeInfo->tm_min;
  int second = timeInfo->tm_sec;

  // 날짜와 시간 문자열 만들기
  String date = String(year) + "-" + formatDigits(month) + "-" + formatDigits(day) + "T" + formatDigits(hour) + ":" + formatDigits(minute) + ":" + formatDigits(second);

  sendSensorData(Token, airTemp, airHumid, soilHumid, lightIntensity, status, date);
  delay(1800000); // 30분 마다
}

// 로그인 함수
String loginRequest() {
  // HTTPClient 객체 생성
  HTTPClient http;

  // 로그인 요청을 위한 JSON 페이로드 생성
  String payload = "{\"username\":\"" + String(user_id) + "\", \"password\":\"" + String(user_pw) + "\"}";

  // POST 요청 설정
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  // 로그인 요청 보내기
  int httpCode = http.POST(payload);

  // 응답 확인
  if (httpCode > 0) {
    // 응답 받은 데이터 출력
    String jsonResponse = http.getString();
    Serial.println("HTTP response: " + jsonResponse);

    // JSON 데이터 파싱
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, jsonResponse);

    // JSON 파싱 오류 확인
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
    else {
       // 토큰 추출
      const char* jwtToken = jsonDoc["token"];

      // 토큰 출력 및 반환
      Serial.print("JWT token: ");
      Serial.println(jwtToken);
      http.end();
      return jwtToken;
    }
  }
  else {
    Serial.println("Error on HTTP request");
  }

  // HTTP 연결 닫기
  http.end();
}


//센서 데이터 삽입
void sendSensorData(String authToken, float airTemp, float airHumid, float soilHumid, 
float lightIntensity, bool status, String date) {
  // HTTPClient 객체 생성
  HTTPClient http;
  int cnt = 0;

  // JSON 페이로드 생성
  String payload = "{\"airTemp\":" + String(airTemp, 3) +
                   ", \"airHumid\":" + String(airHumid, 3) +
                   ", \"soilHumid\":" + String(soilHumid, 3) +
                   ", \"lightIntensity\":" + String(lightIntensity, 3) +
                   ", \"status\":" + String(status) +
                   ", \"date\":\"" + date + "\"}";
  while(1)
  {
    // POST 요청 설정
  http.begin("http://localhost:8080/api/post-sensor-data");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  // 데이터 전송
  int httpCode = http.POST(payload);
  Serial.println(httpCode);

  // 응답 확인
  if (httpCode == 200) 
  {
    String response = http.getString();
    Serial.println("HTTP response: " + response);

    // HTTP 연결 닫기
    http.end();
    break;
  }

  else if (cnt == 50)
  {
    //50회 반복후 탈출
    Serial.println("Loigin Error");
    // HTTP 연결 닫기
    http.end();
    break;
  } 

  else 
  {
    // 로그인 요청 보내기
  loginRequest();
  cnt = cnt + 1;
  }

  }
}

//NTP서버와 시간 동기화
void syncTime() {
  // Wait for Wi-Fi connection
  while (!WiFi.isConnected()) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi connected");

  // Sync time with NTP server
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
}

// 10미만에 숫자에 대해서 앞에 0 붙여서 문자열 반환
String formatDigits(int digits) {
  // Add leading zero if needed
  if (digits < 10) {
    return "0" + String(digits);
  } else {
    return String(digits);
  }
}
