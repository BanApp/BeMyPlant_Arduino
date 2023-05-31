#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const int lightPin = 34;  // 조도센서가 연결된 핀 번호

const char* ssid = "";      // Wi-Fi 네트워크 이름
const char* password = "";  // Wi-Fi 비밀번호

const char* user_id = "";
const char* user_pw = "";

const char* serverUrl = "http://localhost:8080/api/authenticate";    // 서버 URL

char token[100];  // 전역 변수로 토큰 선언

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
  loginRequest(token);

  pinMode(lightPin, INPUT);  // 34번 핀을 디지털 입력으로 설정
}

void loop() {
  // 센서 데이터 수집
  float airTemp = 123.123;
  float airHumid = 456.456;
  float soilHumid = 789.789;
  float lightIntensity = analogRead(lightPin) / 1023.0 * 100.0;
  bool status = true;

  // 현재 시간 가져오기
  timeClient.update();
  String date = timeClient.getFormattedTime();

  Serial.println(token);

  // 센서 데이터 전송
  sendSensorData(token, airTemp, airHumid, soilHumid, lightIntensity, status, date);
  delay(1800000); // 30분 마다
}

void loginRequest(char* authToken) {
  // HTTPClient 객체 생성
  HTTPClient http;

  // 로그인 요청을 위한 JSON 페이로드 생성
  char payload[100];
  sprintf(payload, "{\"username\":\"%s\", \"password\":\"%s\"}", user_id, user_pw);

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
    StaticJsonDocument<1024> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, jsonResponse);

    // JSON 파싱 오류 확인
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
    else {
      // 토큰 추출
      const char* jwtToken = jsonDoc["token"];

      // 토큰을 authToken에 복사
      strncpy(authToken, jwtToken, 100);
      authToken[99] = '\0'; // 토큰 길이가 100을 초과할 경우를 대비하여 마지막에 null 문자 추가

      // 토큰 출력
      Serial.print("JWT token: ");
      Serial.println(authToken);
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
  http.addHeader("Authorization", "Bearer " + String(authToken));

  // 데이터 전송
  int httpCode = http.POST(payload);

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
  Token = loginRequest();
  cnt = cnt + 1;
  }

  }

}