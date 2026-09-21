# Communication Design

## 1. 통신 구조 개요

본 프로젝트는 모든 장치 간 데이터 전송에 **TCP 통신**을 사용한다.

전체 통신 구조는 다음과 같다.

```text
Edge Vision Raspberry Pi
        │
        │ TCP / Wi-Fi
        ▼
Relay Raspberry Pi
        │
        │ SQLite 저장
        ▼
   30초 단위 전송
        │
        │ TCP
        ▼
Ubuntu VM Server
        │
        ▼
     MariaDB
```

Arduino UNO의 센서 데이터 역시 ESP-01을 통해 Wi-Fi로 Relay Raspberry Pi에 전달된다.

---

## 2. 장치별 통신 역할

### Edge Vision Raspberry Pi

역할:

* 객체 탐지 결과 데이터 생성
* 객체가 탐지될 때마다 실시간 전송
* Wi-Fi를 통해 Relay Raspberry Pi에 연결

통신 방식:

* TCP
* Client 역할
* Relay Raspberry Pi Port `5000`

---

### Arduino UNO + ESP-01

역할:

* 조도, 온습도, 소음 센서값 수집
* 센서 데이터를 5초 주기로 전송
* ESP-01을 이용한 Wi-Fi 통신

통신 방식:

* TCP
* Client 역할
* Relay Raspberry Pi Port `5000`

---

### Relay Raspberry Pi

역할:

* Edge Vision 데이터 수신
* 센서 데이터 수신
* 수신 데이터에 Timestamp 생성
* SQLite에 1차 저장
* Ubuntu VM 서버로 데이터 전송
* 데이터 재전송 및 동기화 관리

통신 역할:

* Edge 장치 기준: TCP Server
* Ubuntu VM 기준: TCP Client

사용 포트:

```text
Relay Raspberry Pi Server Port : 5000
```

---

### Ubuntu VM Server

역할:

* Relay Raspberry Pi에서 전달된 데이터 수신
* MariaDB에 최종 저장
* 정상 저장 완료 후 ACK 반환
* 저장된 데이터를 시각화 시스템에서 활용

통신 방식:

* TCP
* Server 역할

사용 포트:

```text
Ubuntu VM Server Port : 5001
```

---

## 3. 포트 구성

| 장치                 | 역할             | Port |
| ------------------ | -------------- | ---: |
| Relay Raspberry Pi | Edge 데이터 수신 서버 | 5000 |
| Ubuntu VM          | 중앙 데이터 수신 서버   | 5001 |

Edge Vision Raspberry Pi와 Arduino UNO는 동일한 Relay Server의 Port `5000`으로 접속한다.

각 데이터는 JSON의 `type` 값을 이용하여 구분한다.

---

## 4. 데이터 형식

데이터 전송 형식은 **JSON**을 사용한다.

JSON을 사용하여 센서 데이터와 객체 탐지 데이터를 명확하게 구분하고, 향후 데이터 항목이 추가될 경우에도 구조를 쉽게 확장할 수 있도록 한다.

### TCP 메시지 프레임

TCP는 송신 측의 전송 단위를 수신 측에 그대로 보존하지 않으므로 각 JSON 메시지 앞에 본문 길이를 나타내는 고정 크기 헤더를 붙인다.

```text
[4바이트 payload_length][UTF-8 JSON payload]
```

프레임 규칙:

* `payload_length`는 JSON 본문의 바이트 수이다.
* 길이는 unsigned 32-bit 정수로 표현한다.
* 바이트 순서는 big-endian(network byte order)을 사용한다.
* JSON 본문은 UTF-8로 인코딩한다.
* 문자열 종료 문자 `\0`과 줄바꿈 문자는 프레임에 포함하지 않는다.
* 수신 측은 4바이트 헤더와 지정된 길이의 본문을 각각 모두 받을 때까지 반복해서 수신한다.
* 허용된 최대 메시지 크기를 초과하거나 본문 수신이 완료되기 전에 연결이 종료되면 해당 프레임을 폐기한다.

동일한 프레임 규칙을 다음 구간에 모두 적용한다.

* Arduino UNO + ESP-01 → Relay Raspberry Pi
* Edge Vision Raspberry Pi → Relay Raspberry Pi
* Relay Raspberry Pi → Ubuntu VM Server
* 각 구간의 ACK 응답

---

## 5. 센서 데이터 형식

센서 데이터는 Arduino UNO에서 5초마다 생성하여 Relay Raspberry Pi로 전송한다.

예시:

```json
{
  "version": 1,
  "type": "sensor",
  "device_id": "arduino-01",
  "message_id": "arduino-01-000042-00000123",
  "data": {
    "light": 72,
    "temperature": 25.4,
    "humidity": 61,
    "sound": 438
  }
}
```

Timestamp는 Arduino에서 생성하지 않는다.

Relay Raspberry Pi가 데이터를 수신한 시점에 Timestamp를 추가한다.

예시:

```json
{
  "version": 1,
  "type": "sensor",
  "device_id": "arduino-01",
  "message_id": "arduino-01-000042-00000123",
  "timestamp": "2026-09-21T15:00:05",
  "data": {
    "light": 72,
    "temperature": 25.4,
    "humidity": 61,
    "sound": 438
  }
}
```

`message_id`는 송신 장치가 `<device_id>-<boot_id>-<sequence>` 형식으로 생성한다. ACK를 받지 못해 재전송할 때는 같은 ID와 같은 데이터를 사용한다.

---

## 6. Vision 데이터 형식

Vision 데이터는 객체가 탐지될 때마다 Relay Raspberry Pi로 전송한다.

현재 Vision 데이터의 상세 구조는 확정되지 않았으며, 아래 형식은 예시로 사용한다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000015-00000427",
  "data": {
    "object": "car",
    "confidence": 0.91
  }
}
```

Relay Raspberry Pi 수신 후:

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000015-00000427",
  "timestamp": "2026-09-21T15:00:03",
  "data": {
    "object": "car",
    "confidence": 0.91
  }
}
```

Vision 데이터의 상세 필드는 Edge Vision 구현 결과에 따라 추후 수정한다.

---

## 7. Timestamp 처리

Timestamp는 **Relay Raspberry Pi에서 생성한다.**

Edge Vision Raspberry Pi와 Arduino UNO에서는 데이터 생성과 전송 기능에 집중하고, 시간 기록과 데이터 관리 기능은 Relay Raspberry Pi에서 처리한다.

목적:

* Edge Vision Raspberry Pi의 부가 연산 최소화
* 데이터 시간 기록 방식 통일
* 센서 데이터와 객체 탐지 데이터를 동일한 시간 기준으로 관리

---

## 8. SQLite 1차 저장

Relay Raspberry Pi에서 수신한 모든 데이터는 Ubuntu VM으로 바로 전달하지 않고 먼저 SQLite에 저장한다.

저장 시 기본 동기화 상태는 다음과 같다.

```text
sync_status = UNSENT
```

SQLite는 최종 저장소가 아니라 **네트워크 장애 시 데이터 유실을 방지하기 위한 로컬 버퍼 및 임시 저장소**로 사용한다.

---

## 9. Ubuntu VM 전송 주기

Relay Raspberry Pi는 SQLite에 저장된 데이터를 **30초 단위로 전송**한다.

30초마다 SQLite에서 다음 조건의 데이터를 조회한다.

```text
sync_status = UNSENT
```

조회된 데이터를 Ubuntu VM Server의 Port `5001`로 전송한다.

기본 흐름:

```text
데이터 수신
    ↓
Timestamp 생성
    ↓
SQLite 저장
    ↓
sync_status = UNSENT
    ↓
30초 대기
    ↓
UNSENT 데이터 조회
    ↓
Ubuntu VM 전송
```

---

## 10. ACK 처리

본 시스템에서는 **두 구간 모두 ACK를 사용**한다.

### 1차 ACK — Edge Device → Relay Raspberry Pi

Edge Vision Raspberry Pi 또는 Arduino UNO가 데이터를 Relay Raspberry Pi로 전송하면, Relay Raspberry Pi는 데이터를 수신한 뒤 SQLite에 저장한다.

SQLite 저장이 정상적으로 완료되면 클라이언트에 길이 헤더가 포함된 다음 JSON 응답을 반환한다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "arduino-01-000042-00000123",
  "status": "ok"
}
```

의미:

```text
데이터 수신 성공
    ↓
SQLite 저장 성공
    ↓
    Relay Raspberry Pi → ACK 반환
```

즉, 성공 ACK는 단순히 데이터를 수신했다는 의미가 아니라 **해당 `message_id`가 SQLite에 정상적으로 저장되었다는 의미**로 사용한다.

---

### 2차 ACK — Relay Raspberry Pi → Ubuntu VM

Relay Raspberry Pi가 `UNSENT` 데이터를 Ubuntu VM으로 전송하면, Ubuntu VM은 해당 데이터를 MariaDB에 저장한다.

MariaDB 저장이 정상적으로 완료되면 Relay Raspberry Pi에 해당 `message_id`의 성공 ACK를 반환한다.

Relay Raspberry Pi는 성공 ACK를 받은 경우에만 SQLite의 동기화 상태를 변경한다.

```text
UNSENT → SENT
```

ACK를 받지 못한 경우에는 다음과 같이 처리한다.

```text
ACK 미수신 또는 오류 ACK
    ↓
UNSENT 상태 유지
    ↓
다음 전송 주기에 재전송
```

따라서 각 ACK의 의미는 다음과 같다.

* **Relay Raspberry Pi의 성공 ACK**

  * SQLite 저장 완료

* **Ubuntu VM의 성공 ACK**

  * MariaDB 저장 완료

같은 `message_id`와 같은 내용이 재전송된 경우에는 새 행을 추가하지 않고 다음과 같이 성공 ACK를 반환한다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "arduino-01-000042-00000123",
  "status": "ok",
  "duplicate": true
}
```

같은 `message_id`에 다른 내용이 들어오면 `MESSAGE_ID_CONFLICT` 오류로 처리한다.

이를 통해 각 통신 구간에서 데이터가 실제 저장소에 정상적으로 기록되었는지 확인한다.



---

## 11. 재전송 처리

네트워크 장애 또는 Ubuntu VM 서버 오류로 인해 데이터 전송에 실패하면 해당 데이터는 SQLite에서 삭제하지 않는다.

```text
전송 실패
    ↓
UNSENT 상태 유지
    ↓
다음 30초 전송 주기
    ↓
재전송
```

이를 통해 일시적인 네트워크 장애가 발생하더라도 데이터가 유실되지 않도록 한다.

---

## 12. 재접속 정책

TCP 연결이 끊긴 경우 **5초 간격으로 재접속을 시도**한다.

```text
연결 실패
    ↓
5초 대기
    ↓
재접속 시도
    ↓
실패 시 반복
```

연결이 복구된 이후에는 기존에 SQLite에 저장된 `UNSENT` 데이터를 다시 전송한다.

---

## 13. 전체 통신 흐름

```text
[Edge Vision Raspberry Pi]          [Arduino UNO + ESP-01]
          │                                  │
          │ 객체 탐지 시 전송                 │ 5초 주기 전송
          │ TCP / Wi-Fi                      │ TCP / Wi-Fi
          └───────────────┬──────────────────┘
                          │
                          ▼
               [Relay Raspberry Pi :5000]
               - 데이터 수신
               - Timestamp 생성
               - SQLite 저장
               - sync_status = UNSENT
                          │
                          │ 30초마다 UNSENT 데이터 조회
                          ▼
               [Ubuntu VM Server :5001]
                          │
                          ▼
                       MariaDB
                          │
                          ▼
                  데이터 최종 저장
```

### ACK 흐름

```text
Edge Vision Pi ───────▶ Relay Pi
               ◀─────── JSON ACK

Arduino + ESP-01 ─────▶ Relay Pi
               ◀─────── JSON ACK

Relay Pi ─────────────▶ Ubuntu VM
         ◀───────────── JSON ACK
```

* Relay Pi의 성공 ACK = 해당 `message_id`의 SQLite 저장 완료
* Ubuntu VM의 성공 ACK = 해당 `message_id`의 MariaDB 저장 완료


---

## 14. 핵심 통신 정책

* 모든 장치 간 통신은 TCP 사용
* Edge 장치는 Wi-Fi를 이용하여 Relay Raspberry Pi에 연결
* Relay Raspberry Pi 수신 Port는 `5000`
* Ubuntu VM 수신 Port는 `5001`
* 센서 데이터 전송 주기는 `5초`
* Vision 데이터는 객체 탐지 시 즉시 전송
* 데이터 형식은 JSON 사용
* TCP 메시지는 `4바이트 big-endian 길이 헤더 + UTF-8 JSON 본문` 형식 사용
* Timestamp는 Relay Raspberry Pi에서 데이터 수신 시 생성
* 모든 데이터는 SQLite에 우선 저장
* 초기 동기화 상태는 `UNSENT`
* `30초`마다 `UNSENT` 데이터 전송
* MariaDB 저장 성공 시 Ubuntu VM이 해당 `message_id`의 JSON ACK 반환
* 성공 ACK 수신 후 해당 데이터의 상태를 `SENT`로 변경
* 전송 실패 시 `UNSENT` 상태를 유지하고 다음 전송 주기에 재전송
* TCP 연결 실패 시 `5초` 간격으로 재접속
