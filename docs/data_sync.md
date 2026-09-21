# 데이터 안정성 및 동기화 설계

## 1. 로컬 버퍼 및 임시 저장소

Raspberry Pi 4는 분석 결과 데이터와 센서 데이터를 수신한 뒤, Ubuntu VM 서버로 바로 전달하지 않고 **SQLite에 1차 저장**한다.

SQLite는 영구 저장을 위한 메인 DB가 아니라, **네트워크 장애 시 데이터 유실을 방지하기 위한 로컬 버퍼 및 임시 저장소**로 사용한다.

### 동작 목적

* Ubuntu VM 서버 연결 장애 시 데이터 유실 방지
* Wi-Fi 및 네트워크 일시 장애 대응
* 서버 장애 시 데이터 임시 보관
* 네트워크 복구 후 미전송 데이터 재전송

### 기본 흐름

```text
분석 결과 데이터 / 센서 데이터
        ↓
Raspberry Pi 4
        ↓
SQLite 1차 저장
        ↓
Ubuntu VM 연결 확인
        ↓
   ┌───────────────┐
   │               │
연결 정상        연결 실패
   │               │
   ↓               ↓
MariaDB 전송    SQLite 유지
                   ↓
              네트워크 복구
                   ↓
               재전송
```

---

## 2. SQLite → MariaDB 동기화

SQLite에 저장된 데이터가 MariaDB에 정상적으로 저장되었는지 확인하기 위해 **동기화 상태를 관리**한다.

각 데이터에는 고유 ID와 전송 상태를 저장한다.

### 데이터 예시

|  id | timestamp | data_type | data | sync_status |
| --: | --------- | --------- | ---- | ----------- |
| 101 | 10:30:01  | SENSOR    | ...  | UNSENT      |
| 102 | 10:30:02  | VISION    | ...  | SENT        |

### 동기화 상태

* `UNSENT`

  * MariaDB에 아직 정상적으로 저장되지 않은 데이터

* `SENT`

  * MariaDB 저장이 정상적으로 완료된 데이터

---

## 3. 동기화 처리 흐름

### 1. 데이터 수신

Raspberry Pi 4가 분석 결과 데이터 또는 센서 데이터를 수신한다.

### 2. SQLite 저장

수신 데이터를 SQLite에 먼저 저장한다.

```text
sync_status = UNSENT
```

### 3. MariaDB 전송

Raspberry Pi 4가 SQLite에 저장된 데이터를 Ubuntu VM 서버로 전송한다.

### 4. 저장 성공 확인

Ubuntu VM에서 MariaDB 저장이 정상적으로 완료되었는지 확인한다.

### 5. 상태 변경

저장이 정상적으로 완료되면 SQLite의 상태를 변경한다.

```text
UNSENT → SENT
```

### 6. 실패 데이터 재전송

네트워크 장애 또는 서버 오류로 전송에 실패한 데이터는 `UNSENT` 상태로 유지한다.

연결이 복구되면 `UNSENT` 데이터를 다시 조회하여 MariaDB로 재전송한다.

---

## 4. 전체 처리 흐름

```text
데이터 생성
    ↓
Raspberry Pi 4
    ↓
SQLite 저장
(sync_status = UNSENT)
    ↓
Ubuntu VM 전송
    ↓
MariaDB 저장
    ↓
저장 성공 확인
    ↓
sync_status = SENT
```

전송 실패 시:

```text
전송 실패
    ↓
UNSENT 상태 유지
    ↓
SQLite 임시 보관
    ↓
네트워크 복구
    ↓
UNSENT 데이터 재전송
```

---

## 핵심 설계

```text
데이터 생성
    ↓
Raspberry Pi 4
    ↓
SQLite
[로컬 버퍼 / 1차 저장]
    ↓
동기화 및 재전송
    ↓
Ubuntu VM
    ↓
MariaDB
[중앙 저장 / 최종 저장]
```

Raspberry Pi 4의 SQLite를 **로컬 버퍼로 활용하고**, SQLite와 MariaDB 사이에 **동기화 상태 관리 및 재전송 로직**을 적용하여 네트워크 장애 상황에서도 데이터가 유실되지 않도록 설계한다.
