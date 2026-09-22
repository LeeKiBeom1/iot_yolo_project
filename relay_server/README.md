# Relay Server

Raspberry Pi 4에서 실행되는 중계 서버 코드가 위치합니다.

주요 역할은 센서 및 객체 탐지 데이터를 수신하고, 로컬에 임시 저장한 뒤 Ubuntu 서버로 전달하는 것입니다.

## 현재 센서 처리 흐름

현재 최소 구현은 TCP 포트 `5000`에서 Arduino 센서 JSON을 순차적으로 수신합니다.

```text
센서 JSON 수신
→ Relay 수신 시각 생성
→ SQLite에 UNSENT로 저장
→ 저장 성공 ACK
→ Ubuntu 서버로 미전송 센서 데이터 전송
→ Ubuntu 성공 ACK 수신
→ SQLite 상태를 SENT로 변경
```

Ubuntu 연결에 실패하면 `UNSENT` 상태를 유지하고 다음 센서 메시지를 수신하거나 Relay 서버가 다시 시작될 때 재전송합니다. 고정 주기 타이머는 사용하지 않으며, 다중 클라이언트 처리는 이후 `epoll` 이벤트 루프에서 구현합니다.

동일한 `message_id`와 동일한 내용은 새 행으로 저장하지 않고 `duplicate: true` ACK를 반환합니다. 동일한 `message_id`로 다른 센서값이 들어오면 `MESSAGE_ID_CONFLICT` 오류 ACK를 반환합니다.

## Database schema

SQLite 테이블 정의는 `db/schema.sql`에서 관리합니다.

새 데이터베이스를 생성할 때 다음 명령을 사용합니다.

```bash
sqlite3 db/road_monitor.db < db/schema.sql
```

기존 DB에 메시지 식별 컬럼과 고유 제약을 추가하기 전에는 DB를 백업한 뒤 마이그레이션을 적용합니다.

```bash
cp db/road_monitor.db db/road_monitor.db.bak
sqlite3 db/road_monitor.db < db/migrations/001_add_message_identity.sql
```

Vision 탐지 상세 컬럼을 추가할 때는 다음 마이그레이션을 순서대로 적용합니다.

```bash
sqlite3 db/road_monitor.db < db/migrations/002_expand_vision_detection.sql
```
