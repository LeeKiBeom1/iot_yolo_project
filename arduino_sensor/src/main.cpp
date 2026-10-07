#include <Arduino.h>
#include <DHT.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#include "wifi_config.h"

constexpr uint8_t SOUND_PIN = A0;
constexpr uint8_t LIGHT_PIN = A1;
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t ESP_RX_PIN = 10;
constexpr uint8_t ESP_TX_PIN = 11;
constexpr unsigned long MEASURE_INTERVAL_MS = 5000;
constexpr char DEVICE_ID[] = "arduino-01";
constexpr char RELAY_IP[] = "10.10.16.81";
constexpr uint16_t RELAY_PORT = 5000;

DHT dht(DHT_PIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial esp(ESP_RX_PIN, ESP_TX_PIN);
uint32_t bootId = 0;
uint32_t sequence = 0;
bool wifiConnected = false;
bool tcpConnected = false;

// UNO의 작은 RAM을 위해 응답 전체 대신 기대 문자열과 일치한 길이만 기억한다.
// ESP의 AT 응답이며, 서버가 DB에 저장했다는 application ACK는 아니다.
bool waitForResponse(const char *expected, unsigned long timeoutMs) {
  uint8_t matched = 0;
  const unsigned long startedAt = millis();

  while (millis() - startedAt < timeoutMs) {
    while (esp.available()) {
      const char value = static_cast<char>(esp.read());
      if (value == expected[matched]) {
        matched++;
      } else {
        matched = value == expected[0] ? 1 : 0;
      }
      if (expected[matched] == '\0') {
        return true;
      }
    }
  }
  return false;
}

bool sendCommand(const char *command, const char *expected,
                 unsigned long timeoutMs, bool showCommand = true) {
  while (esp.available()) {
    esp.read();
  }

  if (showCommand) {
    Serial.print("> ");
    Serial.println(command);
  }
  esp.println(command);
  return waitForResponse(expected, timeoutMs);
}

bool startEsp() {
  const unsigned long baudRates[] = {9600, 38400, 57600, 115200};

  for (const unsigned long baudRate : baudRates) {
    Serial.print("Testing ESP baud: ");
    Serial.println(baudRate);
    esp.begin(baudRate);
    delay(300);

    if (sendCommand("AT", "OK", 1500)) {
      Serial.print("ESP baud rate: ");
      Serial.println(baudRate);
      if (!sendCommand("AT+RST", "ready", 5000)) {
        return false;
      }
      delay(1000);
      return sendCommand("AT", "OK", 1500);
    }
    esp.end();
  }
  return false;
}

bool connectWifi() {
  if (!sendCommand("AT+CWMODE=1", "OK", 3000)) {
    return false;
  }

  char joinCommand[64];
  snprintf(joinCommand, sizeof(joinCommand), "AT+CWJAP=\"%s\",\"%s\"",
           WIFI_SSID, WIFI_PASSWORD);
  return sendCommand(joinCommand, "OK", 20000, false);
}

bool connectRelay() {
  sendCommand("AT+CIPMUX=0", "OK", 3000);
  sendCommand("AT+CIPDINFO=0", "OK", 3000);
  char command[64];
  snprintf(command, sizeof(command), "AT+CIPSTART=\"TCP\",\"%s\",%u",
           RELAY_IP, RELAY_PORT);
  tcpConnected = sendCommand(command, "CONNECT", 10000);
  return tcpConnected;
}

// TCP는 메시지 경계가 없으므로 JSON 바이트 수를 big-endian 4바이트로 먼저 보낸다.
bool sendFrame(const char *json) {
  const uint16_t payloadSize = strlen(json);
  const uint16_t frameSize = payloadSize + 4;
  char command[24];
  snprintf(command, sizeof(command), "AT+CIPSEND=%u", frameSize);

  if (!sendCommand(command, ">", 3000)) {
    tcpConnected = false;
    return false;
  }

  const uint8_t header[4] = {
      0, 0, static_cast<uint8_t>(payloadSize >> 8),
      static_cast<uint8_t>(payloadSize & 0xff)};
  esp.write(header, sizeof(header));
  esp.write(reinterpret_cast<const uint8_t *>(json), payloadSize);
  return waitForResponse("SEND OK", 5000);
}

bool ensureConnection() {
  if (!wifiConnected) {
    wifiConnected = connectWifi();
  }
  if (!wifiConnected) {
    return false;
  }
  if (!tcpConnected) {
    tcpConnected = connectRelay();
  }
  return tcpConnected;
}

// 재부팅해도 같은 ID를 만들지 않도록 실행 번호만 EEPROM에 저장한다.
// 매 측정마다 쓰지 않아 EEPROM 쓰기 횟수를 줄인다. sequence는 RAM에서 증가한다.
void initializeMessageId() {
  EEPROM.get(0, bootId);
  if (bootId == UINT32_MAX) {
    bootId = 0;
  }
  bootId++;
  EEPROM.put(0, bootId);
}

bool sendSensorData(int light, float temperature, float humidity, int sound) {
  char messageId[48];
  char temperatureText[8];
  char humidityText[8];
  char json[176];

  sequence++;
  snprintf(messageId, sizeof(messageId), "%s-%06lu-%08lu", DEVICE_ID,
           static_cast<unsigned long>(bootId),
           static_cast<unsigned long>(sequence));
  dtostrf(temperature, 1, 1, temperatureText);
  dtostrf(humidity, 1, 1, humidityText);
  const int jsonLength = snprintf(json, sizeof(json),
           "{\"version\":1,\"type\":\"sensor\",\"device_id\":\"%s\","
           "\"message_id\":\"%s\",\"data\":{\"light\":%d,"
           "\"temperature\":%s,\"humidity\":%s,\"sound\":%d}}",
           DEVICE_ID, messageId, light, temperatureText, humidityText, sound);

  // 버퍼를 무작정 키우지 않고, 잘린 JSON은 송신하지 않는다.
  if (jsonLength < 0 || static_cast<size_t>(jsonLength) >= sizeof(json)) {
    Serial.println("Sensor JSON too large");
    return false;
  }

  Serial.print("Sending: ");
  Serial.println(messageId);

  if (ensureConnection() && sendFrame(json)) {
    return true;
  }

  tcpConnected = false;
  wifiConnected = false; // Wi-Fi 단절도 가능하므로 다음 측정 때 연결부터 다시 확인한다.
  sendCommand("AT+CIPCLOSE", "OK", 2000);
  return false;
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  initializeMessageId();

  lcd.init();
  lcd.backlight();
  lcd.print("Sensor starting");

  delay(2000);

  if (!startEsp()) {
    Serial.println("ESP-01 not responding");
    lcd.clear();
    lcd.print("ESP not found");
    return;
  }

  lcd.clear();
  lcd.print("WiFi connecting");

  if (!connectWifi()) {
    Serial.println("Wi-Fi connection failed");
    lcd.clear();
    lcd.print("WiFi failed");
    return;
  }

  wifiConnected = true;
  Serial.println("Wi-Fi connected");
  lcd.clear();
  lcd.print("WiFi connected");
  delay(1000);

  if (!connectRelay()) {
    Serial.println("Relay connection deferred");
  }
}

void loop() {
  const int sound = analogRead(SOUND_PIN);
  const int light = analogRead(LIGHT_PIN);
  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT11 read failed");
    lcd.clear();
    lcd.print("DHT read failed");
    delay(MEASURE_INTERVAL_MS);
    return;
  }

  Serial.print("light=");
  Serial.print(light);
  Serial.print(", temperature=");
  Serial.print(temperature, 1);
  Serial.print(", humidity=");
  Serial.print(humidity, 1);
  Serial.print(", sound=");
  Serial.println(sound);

  lcd.clear();
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print(" H:");
  lcd.print(humidity, 0);

  lcd.setCursor(0, 1);
  lcd.print("L:");
  lcd.print(light);
  lcd.print(" S:");
  lcd.print(sound);

  const bool sent = sendSensorData(light, temperature, humidity, sound);
  Serial.println(sent ? "Sensor data sent" : "Sensor data send failed");

  // 측정·송신 완료 후 5초 대기한다. 실제 측정 간격은 통신 소요 시간만큼 더 길어진다.
  delay(MEASURE_INTERVAL_MS);
}
