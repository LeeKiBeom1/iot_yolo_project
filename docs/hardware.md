# Hardware Configuration

## 1. 하드웨어 구성 개요

본 프로젝트는 다음과 같은 하드웨어로 구성한다.

* Raspberry Pi 4 × 2
* Arduino UNO × 1
* Pleomax W210 USB Webcam
* ESP-01 Wi-Fi Module
* KY-037 Sound Sensor
* Photoresistor Sensor Module
* DHT11 Temperature & Humidity Sensor
* 16x2 I2C LCD
* Wi-Fi Router
* 각 보드용 전원 어댑터

---

## 2. Raspberry Pi 4

### Raspberry Pi 4 #1 — Edge Vision

역할:

* 웹캠 영상 입력
* 객체 탐지 및 비전 처리
* 분석 결과 데이터 생성
* 분석 결과를 Wi-Fi를 통해 중계 서버로 전송

연결 장치:

* Pleomax W210 USB Webcam
* Wi-Fi Network

카메라 입력 해상도:

```text
640 × 480 pixels
```

객체 탐지 Bounding Box는 이 원본 프레임의 좌측 상단을 `(0, 0)`으로 하는 pixel 좌표를 사용한다.

---

### Raspberry Pi 4 #2 — Relay Server

역할:

* 객체 탐지 데이터 수신
* 센서 데이터 수신
* SQLite 기반 1차 저장
* 네트워크 장애 시 임시 데이터 보관
* Ubuntu VM 서버로 데이터 전달

주요 소프트웨어:

* SQLite

통신:

* Wi-Fi / Network

---

## 3. Arduino UNO

Arduino UNO는 환경 센서 데이터를 수집하고 ESP-01을 통해 중계 서버로 전송한다.

또한 16x2 I2C LCD를 통해 데이터 송신 상태 및 시스템 상태를 확인할 수 있도록 구성한다.

### 주요 역할

* 소음 센서값 측정
* 조도 센서값 측정
* 온도 / 습도 측정
* 센서 데이터 Wi-Fi 전송
* LCD 상태 표시

---

## 4. 센서 및 주변 장치

### KY-037 소음 센서

용도:

* 주변 소음 센서값 측정

사용 방식:

* 아날로그 출력 사용
* 디지털 출력은 사용하지 않음

Arduino 연결:

* Analog OUT → `A0`
* VCC
* GND

---

### 조도 센서

형태:

* 포토레지스터 기반 3핀 센서 모듈

용도:

* 주변 밝기 측정

Arduino 연결:

* Signal → `A1`
* VCC
* GND

---

### DHT11

용도:

* 온도 측정
* 습도 측정

Arduino 연결:

* DATA → `D4`
* VCC
* GND

---

### 16x2 I2C LCD

용도:

* 센서 데이터 송신 상태 확인
* 시스템 정상 동작 여부 확인
* 필요 시 센서값 및 상태 정보 표시

Arduino 연결:

* SDA → `A4`
* SCL → `A5`
* VCC
* GND

I2C Address:

```text
확인 예정
예상 주소: 0x27
```

---

## 5. ESP-01 Wi-Fi Module

Arduino UNO의 센서 데이터를 Wi-Fi를 통해 Raspberry Pi 4 중계 서버로 전송한다.

ESP-01은 전압 변환 PCB와 함께 사용하며, 5V 입력을 받아 PCB에서 3.3V로 변환한다.

### 연결

| ESP-01 | Arduino UNO |
| ------ | ----------- |
| TX     | D10 (RX)    |
| RX     | D11 (TX)    |
| VCC    | 5V / 변환 PCB |
| GND    | GND         |

Arduino에서는 SoftwareSerial을 사용하여 통신한다.

```text
Arduino D10 = RX
Arduino D11 = TX
```

통신 방향:

```text
ESP-01 TX → Arduino D10 (RX)

ESP-01 RX ← Arduino D11 (TX)
```

---

## 6. Arduino UNO Pin Map

| Arduino Pin | 연결 장치        | 기능                |
| ----------- | ------------ | ----------------- |
| A0          | KY-037       | 소음 센서값 입력         |
| A1          | 조도 센서        | 조도값 입력            |
| A4          | 16x2 I2C LCD | SDA               |
| A5          | 16x2 I2C LCD | SCL               |
| D4          | DHT11        | 온습도 데이터           |
| D10         | ESP-01 TX    | SoftwareSerial RX |
| D11         | ESP-01 RX    | SoftwareSerial TX |

---

## 7. 네트워크 구성

객체 탐지 Raspberry Pi와 Arduino UNO는 모두 Wi-Fi를 이용하여 데이터를 전송한다.

```text
Raspberry Pi 4 (Edge Vision)
          │
          │ Wi-Fi
          ▼
Raspberry Pi 4 (Relay Server)
```

```text
Arduino UNO
    │
 ESP-01
    │
    │ Wi-Fi
    ▼
Raspberry Pi 4 (Relay Server)
```

중계 서버에 수집된 데이터는 이후 Ubuntu VM 서버로 전달된다.

---

## 8. 전원 구성

각 보드는 별도의 전원 어댑터를 사용한다.

### Raspberry Pi 4

* 전용 어댑터 사용

### Arduino UNO

* 외부 어댑터 사용

### ESP-01

* Arduino 측 전원 사용
* ESP-01 변환 PCB를 통해 5V → 3.3V 변환

### 센서 및 LCD

* Arduino UNO의 전원을 사용

---

## 9. 확정된 하드웨어 목록

| 구분                | 모델 / 장치                    | 수량 | 상태 |
| ----------------- | -------------------------- | -: | -- |
| Edge Vision       | Raspberry Pi 4             |  1 | 확정 |
| Relay Server      | Raspberry Pi 4             |  1 | 확정 |
| Sensor Controller | Arduino UNO                |  1 | 확정 |
| Webcam            | Pleomax W210 (300K pixels) |  1 | 확정 |
| Wi-Fi             | ESP-01 + 전압 변환 PCB         |  1 | 확정 |
| Sound Sensor      | KY-037                     |  1 | 확정 |
| Light Sensor      | Photoresistor 3-pin Module |  1 | 확정 |
| Temp/Humidity     | DHT11                      |  1 | 확정 |
| Display           | 16x2 I2C LCD               |  1 | 확정 |
| Network           | Wi-Fi Router               |  1 | 보유 |

---

## 10. 추가 확인 사항

현재 하드웨어 구성은 확정된 상태이며, 다음 항목만 실제 개발 과정에서 확인한다.

* 16x2 I2C LCD 실제 주소 확인

  * 예상: `0x27`
* 각 센서의 실제 측정값 범위 확인
* 센서 데이터 측정 주기 확정
* LCD에 표시할 상태 정보 정의
