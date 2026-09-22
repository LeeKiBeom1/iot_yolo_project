# Relay Server

Raspberry Pi 4에서 실행되는 중계 서버 코드가 위치합니다.

주요 역할은 센서 및 객체 탐지 데이터를 수신하고, 로컬에 임시 저장한 뒤 Ubuntu 서버로 전달하는 것입니다.

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
