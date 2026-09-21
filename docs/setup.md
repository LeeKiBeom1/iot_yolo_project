# Development Environment Setup

## 1. 시스템 구성

본 프로젝트는 다음 3개의 주요 시스템으로 구성한다.

* Edge Vision Raspberry Pi
* Relay Raspberry Pi
* Ubuntu VM Server

각 장치는 역할에 따라 별도의 개발 환경과 라이브러리를 사용한다.

---

## 2. Edge Vision Raspberry Pi

### 기본 정보

* Hostname: `raspberrypi`
* IP: `10.10.16.235`
* OS: `Debian GNU/Linux 13 (trixie)`
* Architecture: `64-bit`

### 역할

* 웹캠 영상 입력
* 객체 탐지 및 비전 처리
* 객체 탐지 결과 데이터 생성
* Relay Raspberry Pi로 데이터 전송

### 통신

* Wi-Fi
* TCP Client
* Relay Raspberry Pi Port `5000`

Vision 데이터의 상세 구조는 비전 처리 구현 결과에 따라 추후 확정한다.

---

## 3. Relay Raspberry Pi

### 기본 정보

* Hostname: `pi21`
* IP: `10.10.16.81`
* OS: `Raspbian GNU/Linux 13 (trixie)`
* Architecture: `32-bit`

### 역할

* Edge Vision 데이터 수신
* Arduino 센서 데이터 수신
* Timestamp 생성
* SQLite 1차 저장
* `UNSENT / SENT` 상태 관리
* Ubuntu VM 서버로 데이터 전송
* 네트워크 장애 시 데이터 임시 보관

---

## 4. Relay Raspberry Pi 개발 환경

### GCC

```bash
gcc --version
```

확인된 버전:

```text
gcc 14.2.0
```

---

### SQLite

```bash
sqlite3 --version
```

확인된 버전:

```text
SQLite 3.46.1 (32-bit)
```

---

### SQLite C 개발 라이브러리

설치 확인:

```bash
dpkg -l | grep libsqlite3-dev
```

확인된 패키지:

```text
libsqlite3-dev:armhf
3.46.1-7+deb13u2
```

C 코드에서 다음 헤더를 사용할 수 있다.

```c
#include <sqlite3.h>
```

링크 옵션:

```bash
-lsqlite3
```

---

### cJSON

설치 확인:

```bash
dpkg -l | grep libcjson-dev
```

확인된 패키지:

```text
libcjson-dev:armhf
1.7.18-3.1+deb13u1
```

C 코드에서 사용:

```c
#include <cjson/cJSON.h>
```

링크 옵션:

```bash
-lcjson
```

---

## 5. Relay Raspberry Pi 프로젝트 구조

프로젝트 경로:

```text
/home/pi/iot_project/relay_server
```

폴더 구조:

```text
relay_server/
|-- db/
|   `-- road_monitor.db
|-- include/
|-- logs/
|-- src/
`-- Makefile
```

### 디렉터리 역할

* `src/`

  * C 소스 파일

* `include/`

  * 헤더 파일

* `db/`

  * SQLite 데이터베이스 파일

* `logs/`

  * 서버 로그 파일

---

## 6. Relay Raspberry Pi SQLite 구성

SQLite DB 위치:

```text
/home/pi/iot_project/relay_server/db/road_monitor.db
```

SQLite 실행:

```bash
sqlite3 db/road_monitor.db
```

생성된 테이블:

```text
sensor_data
vision_data
```

### sensor_data

```sql
CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    light INTEGER,
    temperature REAL,
    humidity REAL,
    sound INTEGER,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);
```

Timestamp Index:

```sql
CREATE INDEX idx_sensor_timestamp
ON sensor_data(timestamp);
```

---

### vision_data

```sql
CREATE TABLE vision_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    object TEXT,
    confidence REAL,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);
```

Timestamp Index:

```sql
CREATE INDEX idx_vision_timestamp
ON vision_data(timestamp);
```

---

## 7. Relay Raspberry Pi Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LIBS = -lsqlite3 -lcjson

TARGET = relay_server
SRC = $(wildcard src/*.c)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
```

주요 라이브러리:

* SQLite
* cJSON

---

## 8. Ubuntu VM Server

### 기본 정보

* Hostname: `ubuntu21`
* IP: `10.10.16.51`
* OS: `Ubuntu 24.04.5 LTS (Noble Numbat)`
* Architecture: `64-bit`

### 역할

* Relay Raspberry Pi 데이터 수신
* MariaDB 최종 저장
* 장기 데이터 보관
* 데이터 조회 및 시각화용 데이터 제공

---

## 9. Ubuntu VM 개발 환경

### GCC

```bash
gcc --version
```

확인된 버전:

```text
gcc 13.3.0
```

---

### MariaDB C 개발 라이브러리

설치 확인:

```bash
dpkg -l | grep libmariadb-dev
```

확인된 패키지:

```text
libmariadb-dev
10.11.14
```

C 코드에서 MariaDB 연결 시 사용:

```c
#include <mysql.h>
```

컴파일 옵션은 `mariadb_config`를 이용한다.

```bash
mariadb_config --cflags
mariadb_config --libs
```

---

### cJSON

설치 확인:

```bash
dpkg -l | grep libcjson-dev
```

확인된 패키지:

```text
libcjson-dev:amd64
1.7.17-1
```

C 코드에서 사용:

```c
#include <cjson/cJSON.h>
```

---

## 10. MariaDB Server

설치:

```bash
sudo apt update
sudo apt install mariadb-server
```

서비스 상태 확인:

```bash
systemctl status mariadb
```

현재 상태:

```text
active (running)
```

버전:

```bash
mariadb --version
```

확인된 버전:

```text
10.11.14-MariaDB
```

---

## 11. MariaDB 외부 접속 설정

MariaDB 기본 설정은 외부 접속이 제한되어 있었다.

설정 파일:

```text
/etc/mysql/mariadb.conf.d/50-server.cnf
```

기존 설정:

```ini
bind-address = 127.0.0.1
```

변경:

```ini
bind-address = 0.0.0.0
```

변경 후 재시작:

```bash
sudo systemctl restart mariadb
```

Port 확인:

```bash
ss -lntp | grep 3306
```

정상 상태:

```text
0.0.0.0:3306
```

---

## 12. MariaDB 프로젝트 DB

DB 이름:

```text
road_monitor
```

사용자:

```text
ubuntu
```

비밀번호:

```text
문서에 기록하지 않음
```

실제 비밀번호는 Git 저장소에 기록하지 않고 별도로 관리한다.

DB 생성:

```sql
CREATE DATABASE road_monitor;
```

사용자 생성 예시:

```sql
CREATE USER 'ubuntu'@'%' IDENTIFIED BY '<DB_PASSWORD>';
```

권한 부여:

```sql
GRANT ALL PRIVILEGES ON road_monitor.* TO 'ubuntu'@'%';
```

현재는 개발용 내부 네트워크 환경을 기준으로 설정한다.

---

## 13. MariaDB 테이블 구성

### sensor_data

```sql
CREATE TABLE sensor_data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME NOT NULL,
    light INT,
    temperature FLOAT,
    humidity FLOAT,
    sound INT
);
```

Timestamp Index:

```sql
CREATE INDEX idx_sensor_timestamp
ON sensor_data(timestamp);
```

---

### vision_data

```sql
CREATE TABLE vision_data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME NOT NULL,
    object VARCHAR(50),
    confidence FLOAT
);
```

Timestamp Index:

```sql
CREATE INDEX idx_vision_timestamp
ON vision_data(timestamp);
```

---

##
