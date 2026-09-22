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
   저장 직후 즉시 전송
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
* Vision–Relay 및 Relay–Ubuntu 구간의 ACK 응답
* Arduino–Relay 구간은 송신 전용

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

Vision 데이터는 탐지된 객체 한 개마다 JSON 메시지 한 건으로 Relay Raspberry Pi에 전송한다. 같은 프레임에서 여러 객체가 탐지되면 `frame_id`와 `timestamp_ms`는 같고 `message_id`는 서로 다른 메시지를 생성한다. 탐지 객체가 없는 프레임은 전송하지 않으므로 `frame_id`가 연속적이지 않아도 정상이다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000015-00000427",
  "data": {
    "frame_id": 62,
    "timestamp_ms": 1789977054740,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.88,
    "bbox": {
      "x": 100,
      "y": 120,
      "width": 80,
      "height": 50
    }
  }
}
```

Vision `message_id`는 `<device_id>-<boot_id>-<sequence>` 형식을 사용한다.

* `device_id`는 장치별 고정값이다.
* `boot_id`는 프로그램 시작 시 로컬 영구 저장값을 1 증가시킨 값이다.
* `sequence`는 새 객체 메시지를 만들 때마다 1 증가한다.
* 재전송할 때는 최초 JSON과 `message_id`를 그대로 사용한다.
* `frame_id`는 메시지 ID 생성에 사용하지 않고 원본 영상 프레임을 식별하는 데이터로만 사용한다.

`timestamp_ms`는 YOLO 추론 완료 시간이 아니라 카메라에서 원본 프레임을 획득한 시각이다. Unix timestamp millisecond 정수로 전송하고 저장한다.

Bounding Box는 `640 × 480` 원본 프레임의 pixel 좌표를 사용한다. `x`, `y`는 좌측 상단이고 `width`, `height`는 너비와 높이이다.

전송 대상 클래스는 다음과 같다.

| class_id | class_name |
| -------: | ---------- |
| 2        | car        |
| 3        | motorcycle |
| 5        | bus        |
| 7        | truck      |

Relay Raspberry Pi는 수신 시각을 별도로 추가하여 SQLite에 저장하고 Ubuntu VM에 전달한다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000015-00000427",
  "timestamp": "2026-09-21T15:00:03",
  "data": {
    "frame_id": 62,
    "timestamp_ms": 1789977054740,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.88,
    "bbox": {
      "x": 100,
      "y": 120,
      "width": 80,
      "height": 50
    }
  }
}
```

---

## 7. Timestamp 처리

센서 데이터의 Timestamp와 모든 데이터의 수신 시각은 **Relay Raspberry Pi에서 생성한다.** Vision 데이터의 `timestamp_ms`는 Edge Vision Pi가 프레임 획득 직후 생성하며, Relay 수신 시각과 별도로 보관한다.

Arduino UNO는 Timestamp를 생성하지 않고 Relay가 센서 데이터 수신 시각을 기록한다. Edge Vision Pi는 프레임 획득 시각만 `timestamp_ms`로 생성하며, Relay는 Vision 데이터의 수신 시각을 별도로 기록한다.

목적:

* 데이터 시간 기록 방식 통일
* 센서 데이터와 객체 탐지 데이터를 동일한 시간 기준으로 관리
* Vision 프레임 획득 시각과 네트워크 수신 시각을 구분

---

## 8. SQLite 1차 저장

Relay Raspberry Pi에서 수신한 모든 데이터는 Ubuntu VM으로 바로 전달하지 않고 먼저 SQLite에 저장한다.

저장 시 기본 동기화 상태는 다음과 같다.

```text
sync_status = UNSENT
```

SQLite는 최종 저장소가 아니라 **네트워크 장애 시 데이터 유실을 방지하기 위한 로컬 버퍼 및 임시 저장소**로 사용한다.

---

## 9. Ubuntu VM 전송 방식

Relay Raspberry Pi는 데이터를 SQLite에 저장한 직후 Ubuntu VM으로 전송한다.

전송할 때는 SQLite에서 다음 조건의 데이터를 오래된 순서대로 조회한다.

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
UNSENT 데이터 조회
    ↓
Ubuntu VM 전송
```

---

## 10. ACK 처리

Vision Client와 Relay–Ubuntu 구간은 ACK를 사용한다. Arduino UNO는 메모리 제약으로 ACK를 처리하지 않는 송신 전용 장치로 운용한다.

### 1차 ACK — Edge Device → Relay Raspberry Pi

Edge Vision Raspberry Pi가 데이터를 Relay Raspberry Pi로 전송하면, Relay Raspberry Pi는 데이터를 수신한 뒤 SQLite에 저장한다.

SQLite 저장이 정상적으로 완료되면 클라이언트에 길이 헤더가 포함된 다음 JSON 응답을 반환한다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000015-00000427",
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
다음 데이터 수신 또는 Relay 서버 재시작 시 재전송
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

처리에 실패하면 다음 형식의 오류 ACK를 반환한다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000015-00000427",
  "status": "error",
  "error_code": "DATABASE_ERROR"
}
```

Vision Client는 ACK의 `message_id`가 전송한 메시지와 같은지 확인한다. `status`가 `ok`이면 완료 처리하고, 제한 시간 안에 ACK를 받지 못하면 같은 JSON과 같은 `message_id`로 재전송한다.

Arduino UNO는 ESP-01의 `SEND OK`까지만 확인하고 다음 측정을 진행한다. 이는 TCP 송신 완료를 의미하며 Relay SQLite 저장을 보장하지 않는다. Arduino–Relay 장애 시 일부 센서 데이터가 유실될 수 있으며, 현재 MVP에서는 5초마다 새 데이터가 생성되는 특성을 고려해 이를 허용한다.

이를 통해 각 통신 구간에서 데이터가 실제 저장소에 정상적으로 기록되었는지 확인한다.



---

## 11. 재전송 처리

네트워크 장애 또는 Ubuntu VM 서버 오류로 인해 데이터 전송에 실패하면 해당 데이터는 SQLite에서 삭제하지 않는다.

```text
전송 실패
    ↓
UNSENT 상태 유지
    ↓
다음 데이터 수신 또는 Relay 서버 재시작
    ↓
UNSENT 데이터 재전송
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
                          │ 저장 직후 UNSENT 데이터 전송
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

Arduino + ESP-01 ─────▶ Relay Pi (송신 전용)

Relay Pi ─────────────▶ Ubuntu VM
         ◀───────────── JSON ACK
```

* Relay Pi의 성공 ACK = 해당 `message_id`의 SQLite 저장 완료
* Ubuntu VM의 성공 ACK = 해당 `message_id`의 MariaDB 저장 완료
* Arduino UNO는 Relay ACK를 수신하거나 재전송하지 않음


---

## 14. 핵심 통신 정책

* 모든 장치 간 통신은 TCP 사용
* Edge 장치는 Wi-Fi를 이용하여 Relay Raspberry Pi에 연결
* Relay Raspberry Pi 수신 Port는 `5000`
* Ubuntu VM 수신 Port는 `5001`
* 센서 데이터 전송 주기는 `5초`
* Arduino UNO는 송신 전용이며 ESP-01의 `SEND OK`까지만 확인
* Vision 데이터는 객체 탐지 시 즉시 전송
* 데이터 형식은 JSON 사용
* TCP 메시지는 `4바이트 big-endian 길이 헤더 + UTF-8 JSON 본문` 형식 사용
* Timestamp는 Relay Raspberry Pi에서 데이터 수신 시 생성
* 모든 데이터는 SQLite에 우선 저장
* 초기 동기화 상태는 `UNSENT`
* SQLite 저장 직후 `UNSENT` 데이터 즉시 전송
* MariaDB 저장 성공 시 Ubuntu VM이 해당 `message_id`의 JSON ACK 반환
* 성공 ACK 수신 후 해당 데이터의 상태를 `SENT`로 변경
* 전송 실패 시 `UNSENT` 상태를 유지하고 다음 데이터 수신 또는 Relay 서버 재시작 시 재전송
* TCP 연결 실패 시 `5초` 간격으로 재접속


# MQTT (Message Queuing Telemetry Transport) 개요

MQTT는 현업 스마트팩토리, 스마트홈, IoT 플랫폼 구축에서 사실상 '업계 표준(De Facto Standard)'으로 널리 쓰이는 실무적인 통신 기술입니다.

## 1. 현업에서 MQTT가 대세인 이유
* **가벼운 프로토콜 구조 (Lightweight)**: TCP 기반이지만 헤더 크기가 최소 2바이트로 매우 작아 네트워크 대역폭이 좁고 불안정한 현장에서 부하를 극도로 줄여줍니다.
* **Pub/Sub (발행/구독) 아키텍처**: 센서는 브로커에 데이터를 발행(Publish)하고, 서버는 필요한 주제(Topic)를 구독(Subscribe)하므로 시스템 결합도가 낮고 확장성이 뛰어납니다.
* **내장된 신뢰성 (QoS Level)**:
  * **QoS 0**: 최대 1회 전송 (유실 가능)
  * **QoS 1**: 최소 1회 전송 (ACK 기반 재전송 ➔ 본 프로젝트의 신뢰성 목표와 일치)
  * **QoS 2**: 정확히 1회 전송 (오버헤드 큼)
* **LWT (Last Will and Testament, 유언 기능)**: 장비가 비정상 종료 시 브로커가 오프라인 상태를 대신 전파해 줍니다.

## 2. 직접 구현하는 TCP 소켓 vs MQTT 비교
* **C 기반 직접 구현한 TCP 소켓 (학습/심화 목적)**:
  * 네트워크 프로토콜 원리, 바이트 오더, 버퍼 관리, 비동기 I/O(`epoll`)를 깊이 있게 익히는 최고의 시스템 엔지니어링 학습 방식입니다.
* **MQTT 도입 (실무 프로덕션 방식)**:
  * 검증된 브로커(Mosquitto 등)를 활용해 통신 레이어의 안정성을 쉽게 확보하지만, 별도의 인프라 운영 및 보안(ACL) 지식이 요구됩니다.

  이번 프로젝트에서는 TCP소켓으로 진행한다.
