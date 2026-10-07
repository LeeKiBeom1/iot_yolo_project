# 화면 전체 차량 수 — 2026-10-06 합의

## 의미와 전송 주기

- 기존 객체별 `vision` 형식은 유지하며, 송신 주기는 1초로 변경한다.
- `vehicle_count`도 1초마다 최신 탐지 완료 프레임 기준으로 보낸다.
- 기존 vision은 객체 1개당 JSON 1개다. 따라서 1초 주기로 선택한 프레임에 객체가 여러 개면 해당 주기에 여러 메시지가 올 수 있다. 프레임 전체를 JSON 하나에 묶는 배열 형식은 이번 변경에 포함하지 않는다.
- vehicle_count는 화면 전체의 car/motorcycle/bus/truck NMS 후 합계다. 누적 탐지 건수, 고유 통과 대수, 혼잡 확정값이 아니다.
- 정상 프레임에서 0대이면 0을 보낸다. 영상 장애/새 프레임 없음은 0대나 이전 값 재사용으로 표현하지 않는다.
- 서버는 수신 주기를 강제하거나 1초 내 추가 메시지를 버리지 않는다. 송신 주기는 Vision 측에서 적용한다.

## JSON

```json
{
  "version": 1,
  "type": "vehicle_count",
  "device_id": "jetson-01",
  "message_id": "jetson-01-000042-00000500",
  "data": {
    "frame_id": 1234,
    "timestamp_ms": 1791255600000,
    "vehicle_count": 18
  }
}
```

장치 ID는 실제 기존 설정을 사용한다. message_id는 vision과 같은 순번 흐름에서 중복 없이 생성한다.
timestamp_ms는 Client의 해당 프레임 획득 시각(Unix ms)이며, 원본 CCTV 촬영 시각을 모르면 촬영 시각이라고 해석하지 않는다.
frame_id는 0 이상 정수, timestamp_ms는 양의 정수이며 현재 JSON 숫자 처리의 정확한 범위인 2^53-1 이하로 제한한다.
vehicle_count는 0~2147483647 정수만 허용한다. ID 길이 제한은 기존과 같이 device_id 32 / message_id 64바이트다.

## 경로와 저장

`Jetson → Pi:5002 → Ubuntu:5003 → MariaDB road_monitor.vehicle_count`

- 두 타입 모두 기존 4바이트 unsigned big-endian 길이 + UTF-8 JSON, 최대 payload 4096바이트.
- 기존 PAUSE/RESUME 정책 유지. 메시지별 ACK/retry와 Pi SQLite 적재는 추가하지 않는다.
- Pi는 타입/필드를 검증하고 원본 JSON을 전달한다. 센서 SQLite/동기화 경로는 유지한다.
- Ubuntu는 타입에 따라 vision_data 또는 vehicle_count에 저장한다.
- vehicle_count 테이블: id, device_id, message_id(UNIQUE), timestamp(DB 저장 시각), frame_id, timestamp_ms(Client 획득 시각), vehicle_count.
- 동일 ID와 동일 원본 필드는 중복 저장하지 않는다. 같은 ID인데 값이 다르면 conflict 로그를 남기고 버린다.
- DB 오류/연결 단절 시 두 Vision 계열 타입을 함께 PAUSE한다. 전송 중/큐 데이터는 기존 정책대로 유실될 수 있다.
- Ubuntu는 vision_data와 vehicle_count 테이블 조회가 가능해야 RESUME한다.
- 폐기된 기준선 통과 집계용 `traffic_count` 경로와 테이블은 제거한다.

## 적용

1. Ubuntu의 실제 road_monitor DB에 `ubuntu_server/db/migrations/003_add_vehicle_count.sql`을 적용한다. 기존 데이터 삭제 없음.
2. 두 서버 코드와 common 헤더를 배포하고 각 디렉터리에서 make 후 서버를 재시작한다.
3. Metabase에서 DB 스키마 동기화를 실행하면 새 테이블을 탐색할 수 있다.
4. 그래프에는 timestamp_ms 기준 vehicle_count 값을 사용한다. 차량 수를 시간에 따라 누적 합산하면 안 된다.

혼잡 판정과 LED 제어는 [signal_control.md](signal_control.md)를 따른다.

## 검증 및 배포 상태 (2026-10-06)

- Pi 실제 환경의 임시 경로에서 relay_server 빌드 성공(경고 없음).
- Pi에서 양쪽 JSON 파서에 대해 0대/18대 수용, 음수/소수/필수 필드 누락 거부 확인.
- Ubuntu 접속 복구 후 실제 Ubuntu/Pi에서 각각 전체 빌드 성공(경고 없음).
- 별도 테스트 DB/계정과 포트 15002→15003에서 E2E 검증: PAUSE→RESUME, 분할 framing, 차량 수 0/18 저장, 동일 메시지 중복 방지, 같은 ID의 다른 값 충돌 처리, vision 동시 저장 통과.
- 검증 후 테스트 프로세스/DB/계정은 정리했다. 운영 DB에 테스트 행을 넣지 않았다.
- 운영 MariaDB road_monitor에 vehicle_count 테이블 생성 완료.
- 양쪽 운영 코드 배포 및 실행 완료. Ubuntu 5001/5003, Pi 5000/5002 리스닝 확인.
- 로그: 각 서버의 logs/vehicle-count-20261006-110646.log.
- 코드/실행파일 백업: /home/ubuntu/ubuntu_server-before-vehicle-20261006-110646.tar.gz, /home/pi/relay_server-before-vehicle-20261006-110646.tar.gz.
- 실제 Jetson의 1초 송신 주기 및 차량 수 정확도는 Vision 측 연결 후 확인해야 한다.

## 가변 CCTV 해상도

CCTV 영상마다 해상도가 달라질 수 있으므로 서버는 640×480 등 특정 해상도를 하드코딩하지 않는다.
frame_width/frame_height 필드는 추가하지 않으며, Vision Client가 보낸 x/y/width/height를 그대로 저장한다.
서버는 x/y가 0 이상이고 width/height가 양수인지만 검증한다.

## Ubuntu 터미널 수신 로그

Ubuntu Vision 경로는 메시지마다 JSON을 출력하지 않는다. 새 데이터가 들어온 경우에만 5초마다 한 줄로 요약한다.

```text
data 5s vision_rx=12 vehicle_count_rx=5 vision_saved=12 vehicle_count_saved=5 dropped=0 queue=0 db_ms=1 ready=1 latest_vehicle_count=18 frame_id=1234 timestamp_ms=1791255600000
```

- `*_rx`: 최근 5초 동안 수신한 타입별 메시지 수.
- `*_saved`: 최근 5초 동안 MariaDB에 새로 저장한 타입별 행 수.
- `latest_vehicle_count`: 가장 최근 수신한 화면 전체 차량 수.
- `dropped`: 최근 5초 동안 버린 메시지 수. 정상 상태에서는 0이어야 한다.
- `ready=1`: MariaDB에 저장할 수 있는 상태.

5초 동안 새 메시지가 하나도 없으면 요약 로그를 출력하지 않는다.
