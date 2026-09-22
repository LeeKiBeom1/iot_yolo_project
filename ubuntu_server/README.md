# Ubuntu Server

Ubuntu VM에서 실행되는 중앙 서버 코드가 위치합니다.

주요 역할은 Relay Server가 전달한 데이터를 수신하여 MariaDB에 저장하고, 조회 및 시각화에 필요한 데이터를 제공하는 것입니다.

서버 실행 시 MariaDB 접속 정보는 환경변수로 전달합니다. 비밀번호는 소스 코드와 Git에 저장하지 않습니다.

```bash
DB_HOST=localhost DB_USER=ubuntu DB_PASSWORD=<DB_PASSWORD> \
DB_NAME=road_monitor ./ubuntu_server
```

Vision 메시지는 객체 한 건을 `vision_data` 한 행으로 저장합니다. 저장 성공 후 `status: "ok"` ACK를 반환하며, 이미 저장된 `message_id`가 다시 수신되면 새 행을 만들지 않고 `duplicate: true`를 포함한 성공 ACK를 반환합니다. DB 저장 실패 시에는 `status: "error"`, `error_code: "DATABASE_ERROR"` ACK를 반환합니다.

현재 서버는 순차 처리 방식으로 계속 실행됩니다. 한 클라이언트가 연결되어 있는 동안 여러 메시지를 처리하고, 연결이 종료되면 다음 클라이언트 접속을 기다립니다. 다중 클라이언트 동시 처리는 이후 `epoll` 기반으로 확장합니다.

## Database schema

MariaDB 테이블 정의는 `db/schema.sql`에서 관리합니다.

`road_monitor` 데이터베이스에 스키마를 적용할 때 다음 명령을 사용합니다. 비밀번호는 명령줄에 직접 적지 않고 프롬프트에서 입력합니다.

```bash
mariadb -u <DB_USER> -p road_monitor < db/schema.sql
```

기존 DB에 메시지 식별 컬럼과 고유 제약을 추가할 때는 먼저 백업한 뒤 마이그레이션을 적용합니다.

```bash
mariadb-dump -u <DB_USER> -p road_monitor > road_monitor_before_001.sql
mariadb -u <DB_USER> -p road_monitor < db/migrations/001_add_message_identity.sql
```

Vision 탐지 상세 컬럼을 추가할 때는 다음 마이그레이션을 순서대로 적용합니다.

```bash
mariadb -u <DB_USER> -p road_monitor < db/migrations/002_expand_vision_detection.sql
```
