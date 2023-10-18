#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AM2320_asukiaaa.h>
#include <BluetoothSerial.h>  // Add BluetoothSerial library

AM2320_asukiaaa mySensor;
BluetoothSerial SerialBT;  // Create a BluetoothSerial object

const int lightPin = 35;     // 조도센서가 연결된 핀 번호
const int SoilHumiPin = 34;  //토양 습도 센서가 연결된 핀 번호

const char* ssid = "";      // Wi-Fi 네트워크 이름 (to be received from the app)
const char* password = "";  // Wi-Fi 비밀번호 (to be received from the app)

const char* user_id = "";  // 사용자 ID
const char* user_pw = "";     // 사용자 PW

const char* serverUrl = "";  // 서버 URL

const int redPin = 25;    // R 핀 번호
const int greenPin = 26;  // G 핀 번호
const int bluePin = 27;   // B 핀 번호

String Token = "";  // 전역 변수로 토큰 선언

WiFiUDP ntpUDP;                                //UDP 소켓 생성
NTPClient timeClient(ntpUDP, "pool.ntp.org");  //NTP 클라이언트 생성

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT");  // Initialize Bluetooth with a name

  // Wi-Fi 연결 시작
  Serial.print("Waiting for WiFi info from Bluetooth...");

  while (!SerialBT.available()) {
    delay(1000);
  }

  Serial.println("Bluetooth connected.");  // Bluetooth 연결 완료 메시지 출력

  // Read WiFi SSID and password from Bluetooth
  String json = SerialBT.readString();
  Serial.println("Received JSON data from Bluetooth: " + json);

  // Parse JSON to get WiFi credentials
  DynamicJsonDocument doc(512);
  deserializeJson(doc, json);

  const char* received_ssid = doc["ssid"];
  const char* received_password = doc["password"];

  const char* received_user_id = doc["user_id"];
  const char* received_user_pw = doc["user_pw"];
  user_id = received_user_id;
  user_pw = received_user_pw;
  // End Bluetooth module
  SerialBT.end();

  if (received_ssid && received_password) {
    ssid = received_ssid;
    password = received_password;
    Serial.println("Using received WiFi SSID and password.");
  } else {
    Serial.println("Error: WiFi SSID or password not received.");
    // Send error message to the app via Bluetooth
    SerialBT.println("Error: WiFi SSID or password not received. Please re-enter.");
    return;  // Exit setup to allow user to re-enter credentials
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // Wi-Fi 연결 확인
  if (!connectToWiFi()) {
    // Send error message to the app via Bluetooth
    SerialBT.println("Error: WiFi connection failed. Please re-enter SSID and password.");
    return;  // Exit setup to allow user to re-enter credentials
  }

  Serial.println("Wi-Fi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());

  // NTP 클라이언트 초기화
  timeClient.begin();

  // AM2320 센서 초기화
  Wire.begin();
  mySensor.setWire(&Wire);

  // 로그인 요청 보내기
  Token = loginRequest();

  pinMode(lightPin, INPUT);
  pinMode(SoilHumiPin, INPUT);

  // Sync time
  syncTime();
}

void loop() {

  // 현재 시간 가져오기
  timeClient.update();

  // 온습도 센서 데이터 가져오기
  mySensor.update();

  // 센서 데이터 수집
  float airTemp = mySensor.temperatureC;
  float airHumid = mySensor.humidity;
  float soilHumid = analogRead(SoilHumiPin);
  float lightIntensity = analogRead(lightPin) / 1023.0 * 100.0;
  bool status = true;

  // 현재 시간을 Unix 타임스탬프로 가져오기
  time_t currentTime = timeClient.getEpochTime();

  // 시간을 GMT+9로 조정하기
  currentTime += 9 * 3600;

  // Unix 타임스탬프를 struct tm으로 변환하기
  struct tm* timeInfo;
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
  delay(10000);  // 10초 마다
}


bool connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  // Wi-Fi 연결이 되지 않았을 때 (빨간색)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    analogWrite(redPin, 255);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  }

  // Wi-Fi 연결 성공 (파란색)
  analogWrite(redPin, 0);
  analogWrite(greenPin, 0);
  analogWrite(bluePin, 255);

  return true;
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

      // 토큰 발급 성공 (초록색)
      analogWrite(redPin, 0);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 0);

      // 발급된 토큰 반환
      return jwtToken;
    }
  } else {
    Serial.println("Error on HTTP request");

    // 로그인 실패(주황색)
    analogWrite(redPin, 255);
    analogWrite(greenPin, 165);
    analogWrite(bluePin, 0);
  }

  // HTTP 연결 닫기
  http.end();
}

// 센서 데이터 삽입
void sendSensorData(String authToken, float airTemp, float airHumid, float soilHumid,
                    float lightIntensity, bool status, String date) {

  HTTPClient http;  // HTTPClient 객체 생성
  int cnt = 0;      // 로그인 재시도 횟수 저장

  // JSON 페이로드 생성
  String payload = "{\"airTemp\":" + String(airTemp, 3) + ", \"airHumid\":" + String(airHumid, 3) + ", \"soilHumid\":" + String(soilHumid, 3) + ", \"lightIntensity\":" + String(lightIntensity, 3) + ", \"status\":" + String(status) + ", \"date\":\"" + date + "\"}";
  while (1) {
    // POST 요청 설정
    http.begin(""); //센서 데이터 삽입 링크
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + authToken);

    // 데이터 전송
    int httpCode = http.POST(payload);
    Serial.println(httpCode);

    // 응답 확인
    if (httpCode == 200) {
      String response = http.getString();
      Serial.println("HTTP response: " + response);

      // HTTP 연결 닫기
      http.end();
      break;
    }

    else if (cnt == 500) {
      //50회 반복후 탈출
      Serial.println("Loigin Error");

      // HTTP 연결 닫기
      http.end();

      // 로그인 실패(노란색)
      analogWrite(redPin, 255);    // Red ON
      analogWrite(greenPin, 255);  // Green ON
      analogWrite(bluePin, 0);     // Blue OFF
      break;
    }

    else {
      // 로그인 재시도 요청 보내기
      Token = loginRequest();

      //로그인 시도 횟수 증가
      cnt = cnt + 1;

      // 로그인 재시도 요청 중 (보라색)
      analogWrite(redPin, 255);   // Red ON
      analogWrite(greenPin, 0);   // Green OFF
      analogWrite(bluePin, 255);  // Blue ON
    }

    delay(1000);  // 10초 마다
  }
}

// NTP서버와 시간 동기화
void syncTime() {
  // Wait for Wi-Fi connection
  while (!WiFi.isConnected()) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi connected");

  // 동기화된 시간 디바이스에 업데이트
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
}

// 10미만에 숫자에 대해서 앞에 0 붙여서 문자열 반환
String formatDigits(int digits) {

  if (digits < 10) {
    return "0" + String(digits);
  } else {
    return String(digits);
  }
}
