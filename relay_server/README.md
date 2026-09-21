# Relay Server

Raspberry Pi 4에서 실행되는 중계 서버 코드가 위치합니다.

주요 역할은 센서 및 객체 탐지 데이터를 수신하고, 로컬에 임시 저장한 뒤 Ubuntu 서버로 전달하는 것입니다.

## Database schema

SQLite 테이블 정의는 `db/schema.sql`에서 관리합니다.

새 데이터베이스를 생성할 때 다음 명령을 사용합니다.

```bash
sqlite3 db/road_monitor.db < db/schema.sql
```
