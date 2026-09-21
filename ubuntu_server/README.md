# Ubuntu Server

Ubuntu VM에서 실행되는 중앙 서버 코드가 위치합니다.

주요 역할은 Relay Server가 전달한 데이터를 수신하여 MariaDB에 저장하고, 조회 및 시각화에 필요한 데이터를 제공하는 것입니다.

## Database schema

MariaDB 테이블 정의는 `db/schema.sql`에서 관리합니다.

`road_monitor` 데이터베이스에 스키마를 적용할 때 다음 명령을 사용합니다. 비밀번호는 명령줄에 직접 적지 않고 프롬프트에서 입력합니다.

```bash
mariadb -u <DB_USER> -p road_monitor < db/schema.sql
```
