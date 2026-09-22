# Database Design

## 1. 데이터베이스 구성 개요

본 프로젝트는 두 단계의 데이터베이스를 사용한다.

### Relay Raspberry Pi

* SQLite 사용
* 수신 데이터를 1차 저장
* 네트워크 장애 시 임시 보관
* `UNSENT / SENT` 상태 관리

### Ubuntu VM

* MariaDB 사용
* 최종 데이터 저장
* 장기 보관
* 데이터 조회 및 시각화에 활용

SQLite와 MariaDB의 기본 데이터 구조는 최대한 동일하게 유지하여 데이터 전송과 동기화를 단순하게 구성한다.

테이블 생성 쿼리는 서버별 스키마 파일로 버전 관리한다.

* Relay SQLite: `relay_server/db/schema.sql`
* Ubuntu MariaDB: `ubuntu_server/db/schema.sql`

문서의 테이블 설명과 실제 스키마가 달라지지 않도록 DB 구조를 변경할 때는 해당 `schema.sql`과 이 문서를 함께 수정한다.

---

## 2. 테이블 구성

센서 데이터와 비전 데이터는 서로 성격이 다르기 때문에 별도의 테이블로 분리한다.

사용 테이블:

* `sensor_data`
* `vision_data`

두 테이블 모두 공통적으로 `id`와 `timestamp`를 가진다.

이를 통해 나중에 시간 기준으로 두 데이터를 함께 조회하고 비교할 수 있도록 한다.

또한 시간 기준 조회가 자주 발생하므로, `sensor_data`와 `vision_data`의 `timestamp` 컬럼에는 인덱스(Index)를 적용한다.


---

## 3. sensor_data

센서 데이터 저장용 테이블이다.

저장 대상:

* 조도
* 온도
* 습도
* 소음 센서값

### 기본 구조

| Column      | Type     | Description              |
| ----------- | -------- | ------------------------ |
| id          | INTEGER  | 데이터 고유 ID                |
| device_id   | TEXT     | 송신 장치 식별자                |
| message_id  | TEXT     | 재전송 중복 방지용 고유 메시지 ID     |
| timestamp   | DATETIME | Relay Raspberry Pi 수신 시간 |
| light       | INTEGER  | 조도 센서값                   |
| temperature | REAL     | 온도                       |
| humidity    | REAL     | 습도                       |
| sound       | INTEGER  | 소음 센서값                   |
| sync_status | TEXT     | SQLite 동기화 상태            |

### Timestamp Index

`sensor_data`는 시간대별 조회와 정렬이 자주 발생하므로 `timestamp` 컬럼에 인덱스를 적용한다.

```sql
CREATE INDEX idx_sensor_timestamp
ON sensor_data(timestamp);
```

이를 통해 특정 시간대의 센서 데이터를 조회하거나 시간 순서로 정렬할 때 검색 성능을 높일 수 있다.


### sync_status

SQLite에서만 사용한다.

가능한 값:

```text
UNSENT
SENT
```

* `UNSENT`

  * 아직 MariaDB에 정상적으로 저장되지 않은 데이터

* `SENT`

  * MariaDB 저장 완료가 확인된 데이터

MariaDB에서는 `sync_status` 컬럼을 사용하지 않아도 된다.

---

## 4. vision_data

객체 탐지 결과 저장용 테이블이다.

객체 한 개를 행 한 개로 저장한다. 동일한 영상 프레임에서 여러 객체가 탐지되면 `frame_id`와 `timestamp_ms`는 같고 `message_id`는 서로 다른 행으로 저장한다.

### 기본 구조

| Column      | Type     | Description              |
| ----------- | -------- | ------------------------ |
| id          | INTEGER  | 데이터 고유 ID                |
| device_id   | TEXT     | 송신 장치 식별자                |
| message_id  | TEXT     | 재전송 중복 방지용 고유 메시지 ID     |
| timestamp   | DATETIME | Relay Raspberry Pi 수신 시간 |
| frame_id    | INTEGER  | 원본 영상 프레임 번호             |
| timestamp_ms | INTEGER | 원본 프레임 획득 Unix 시각(ms)    |
| class_id    | INTEGER  | YOLO 객체 클래스 번호            |
| class_name  | TEXT     | 객체 클래스 이름                 |
| confidence  | REAL     | 객체 탐지 신뢰도                |
| x           | INTEGER  | Bounding Box 좌측 상단 X 좌표    |
| y           | INTEGER  | Bounding Box 좌측 상단 Y 좌표    |
| width       | INTEGER  | Bounding Box 너비              |
| height      | INTEGER  | Bounding Box 높이              |
| sync_status | TEXT     | SQLite 동기화 상태            |

예시:

```text
id: 101
device_id: vision-pi-01
message_id: vision-pi-01-000015-00000427
timestamp: 2026-09-21 15:00:03
frame_id: 62
timestamp_ms: 1789977054740
class_id: 2
class_name: car
confidence: 0.88
x: 100
y: 120
width: 80
height: 50
sync_status: UNSENT
```

`timestamp_ms`는 프레임 획득 시각이고 `timestamp`는 Relay 수신 시각이다. Bounding Box 좌표는 `640 × 480` 원본 프레임을 기준으로 한다.

### Timestamp Index

`vision_data` 역시 시간대별 객체 탐지 결과 조회와 센서 데이터와의 시간 기준 비교를 위해 `timestamp` 컬럼에 인덱스를 적용한다.

```sql
CREATE INDEX idx_vision_timestamp
ON vision_data(timestamp);
```


---

## 5. Timestamp 관리

모든 Timestamp는 Relay Raspberry Pi에서 생성한다.

데이터 흐름:

```text
Edge Device 데이터 전송
        ↓
Relay Raspberry Pi 수신
        ↓
Timestamp 생성
        ↓
SQLite 저장
```

센서 데이터와 비전 데이터 모두 같은 서버에서 Timestamp를 생성하여 시간 기준을 통일한다.

이를 통해 나중에 두 데이터를 같은 시간대 기준으로 함께 조회할 수 있다.

---

## 6. 데이터 ID

각 데이터 행에는 DB 내부 기본 키인 `id`와 장치가 생성하는 `message_id`를 부여한다.

목적:

* 데이터 식별
* 중복 데이터 확인
* 동기화 상태 관리
* 재전송 데이터 식별

`message_id`는 `<device_id>-<boot_id>-<sequence>` 형식을 사용한다. Arduino는 부팅 시 EEPROM의 `boot_id`를 한 번 증가시키고, Vision Pi는 로컬 파일의 `boot_id`를 증가시킨다. 각 장치는 실행 중 `sequence`를 메모리에서 증가시킨다.

송신 장치는 ACK를 받지 못한 데이터를 재전송할 때 같은 `message_id`와 같은 내용을 사용한다. SQLite와 MariaDB의 `message_id`에는 `UNIQUE` 제약을 적용하여 재전송으로 인한 중복 행 생성을 방지한다.

* 같은 ID와 같은 내용: 새 행을 만들지 않고 성공 ACK 반환
* 같은 ID와 다른 내용: `MESSAGE_ID_CONFLICT` 오류 반환
* `id`: 각 DB 내부에서 사용하는 자동 증가 기본 키
* `message_id`: 장치부터 MariaDB까지 유지하는 전송 데이터 식별자

---

## 7. SQLite 저장 정책

Relay Raspberry Pi가 데이터를 수신하면 먼저 SQLite에 저장한다.

기본 처리:

```text
데이터 수신
    ↓
Timestamp 생성
    ↓
SQLite INSERT
    ↓
sync_status = UNSENT
```

SQLite는 최종 저장소가 아니라 로컬 버퍼 및 임시 저장소로 사용한다.

---

## 8. MariaDB 저장 정책

Relay Raspberry Pi는 30초마다 SQLite에서 다음 데이터를 조회한다.

```text
sync_status = UNSENT
```

조회된 데이터를 Ubuntu VM으로 전송한다.

Ubuntu VM에서 MariaDB 저장이 성공하면 해당 `message_id`의 JSON ACK를 반환한다.

Relay Raspberry Pi는 ACK 수신 후 SQLite 상태를 변경한다.

```text
UNSENT → SENT
```

---

## 9. 센서 데이터와 비전 데이터 연결

센서 데이터와 비전 데이터는 별도의 테이블에 저장한다.

두 데이터를 하나의 테이블에 섞지 않고 각각 관리하되, 공통 필드인 `timestamp`를 이용하여 필요할 때 함께 조회한다.

예시:

```text
sensor_data
15:00:05
light = 70
temperature = 25
humidity = 60
sound = 430
```

```text
vision_data
15:00:04
frame_id = 62
timestamp_ms = 1789977054740
class_name = car
confidence = 0.88
```

나중에 같은 시간대 또는 가까운 시간 범위를 기준으로 데이터를 함께 분석할 수 있다.

---

## 10. 데이터 조회 및 분석 방향

MariaDB에 저장된 데이터는 이후 다양한 조건으로 조회한다.

### 시간 기준

* 특정 시간대 데이터 조회
* 시간대별 차량 탐지 수
* 시간대별 센서값 변화
* 일별 / 시간별 데이터 집계

### 센서 기준

* 소음 센서값 높은 순
* 조도 변화
* 온도 변화
* 습도 변화
* 특정 센서값 범위 데이터 조회

### 객체 탐지 기준

* 객체 종류별 조회
* 특정 객체 탐지 횟수
* 시간대별 객체 탐지 수

### 복합 분석

`timestamp`를 기준으로 센서 데이터와 비전 데이터를 같은 시간대에서 함께 조회한다.

이를 통해 예를 들어 다음과 같은 관계를 확인할 수 있다.

* 차량 탐지가 많은 시간대의 소음 변화
* 조도가 낮은 시간대의 객체 탐지 결과
* 특정 환경 조건에서의 교통량 변화

이 분석 항목은 실제 데이터가 축적된 이후 유의미한 관계가 있는지 확인하면서 확장한다.

---

## 11. 전체 데이터 흐름

```text
[Sensor Data]           [Vision Data]
      │                       │
      └──────────┬────────────┘
                 │
                 ▼
        Relay Raspberry Pi
                 │
          Timestamp 생성
                 │
                 ▼
              SQLite
                 │
        sync_status = UNSENT
                 │
                 │ 30초 단위 전송
                 ▼
          Ubuntu VM Server
                 │
                 ▼
              MariaDB
                 │
                 ▼
        데이터 조회 / 가공
                 │
                 ▼
          데이터 시각화
```

---

## 12. 핵심 데이터베이스 정책

* 센서 데이터와 비전 데이터는 별도 테이블로 관리
* 두 테이블 모두 `id`와 `timestamp` 보유
* Timestamp는 Relay Raspberry Pi에서 생성
* SQLite와 MariaDB의 기본 데이터 구조는 최대한 동일하게 유지
* SQLite는 로컬 버퍼 및 임시 저장소 역할
* SQLite에서 `UNSENT / SENT` 상태 관리
* MariaDB는 최종 저장 및 장기 보관
* `30초`마다 `UNSENT` 데이터 전송
* MariaDB 저장 성공 ACK 수신 후 `SENT` 변경
* `timestamp`를 이용하여 센서 데이터와 비전 데이터를 함께 조회 가능
* 실제 데이터 축적 후 시간별·센서별·객체별 분석 및 시각화 수행
* `sensor_data.timestamp`, `vision_data.timestamp`에 인덱스를 적용하여 시간 기준 조회 성능 향상
