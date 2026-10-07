# Jetson 연속 Vision 연동

작성일: 2026-10-01

## 구현 및 배포 상태

소스 구현과 실제 Pi·Ubuntu의 격리된 검증 경로에서 빌드/통신 확인을 완료했다. 2026-10-01 운영 경로에 백업 후 적용하고 양쪽 서버를 시작했다. 운영 Pi 5002에서 최초 PAUSE → RESUME까지 확인했으며 이 점검에서는 Vision 데이터를 보내지 않았다. 실제 Jetson 연결 및 장시간 처리량 검증은 남아 있다.

배포 전 소스·실행 파일 백업:

- Ubuntu: `/home/ubuntu/ubuntu_server-before-vision-20261001-154619.tar.gz`
- Pi: `/home/pi/relay_server-before-vision-20261001-154619.tar.gz`

현재 실행 로그는 각 서버 폴더의 `logs/vision-stream-20261001-154619.log`에 기록한다. 검증용 프로세스와 임시 MariaDB 계정·DB는 정리했으며, 운영 데이터는 삭제하지 않았다.

## 포트와 실행 설정

| 경로 | 기본 포트 | 환경변수 |
| --- | ---: | --- |
| Arduino → Pi 기존 수신 | 5000 | Pi `SENSOR_PORT` |
| Pi → Ubuntu 기존 저장/ACK | 5001 | Pi `SENSOR_FINAL_PORT`, Ubuntu `SENSOR_PORT` |
| Jetson → Pi 새 Vision | 5002 | Pi `VISION_PORT` |
| Pi → Ubuntu 새 Vision | 5003 | Pi `VISION_FINAL_PORT`, Ubuntu `VISION_PORT` |

Pi의 최종 서버 주소는 `UBUNTU_SERVER_IP`이며 기본값은 `10.10.16.51`이다. Jetson endpoint는 Pi IP의 **5002**로 설정한다. Jetson의 프로그램 실행 인자 이름은 해당 담당자의 실행 방법을 따른다.

기존 5000 포트는 최초 Control을 보내지 않는다. 따라서 새 Jetson을 기존 포트에 연결하면 RESUME을 기다리는 상태가 지속될 수 있다. 두 방식의 첫 메시지가 달라 전용 포트로 구분했다.

두 프로젝트 모두 `common/`을 함께 복사해야 빌드된다. 각 서버 디렉터리에서 `make`를 실행한다. Ubuntu DB 인증 환경변수는 기존 `DB_HOST`, `DB_USER`, `DB_PASSWORD`, `DB_NAME`을 사용한다. 비밀번호는 문서나 Git에 기록하지 않는다.

## 새 Vision 경로

`Jetson → Pi 메모리 Queue → Ubuntu 메모리 Queue → MariaDB vision_data`

- UTF-8 JSON 앞에 4바이트 big-endian 길이. payload 최대 4096바이트.
- Vision마다 ACK를 보내지 않으며 Pi SQLite에 신규 Vision을 적재하지 않는다.
- Pi는 원본 JSON과 message_id, timestamp_ms를 유지한다.
- Ubuntu의 기존 `vision_data` 및 UNIQUE message_id 검사를 사용한다.
- timestamp_ms는 촬영 시각이다. 새 경로의 별도 `timestamp`는 Ubuntu 수신 시각이다. 기존 경로의 Relay 수신 시각과 구분해야 한다.
- CCTV 영상마다 해상도가 달라질 수 있으므로 서버는 고정 해상도 상한을 검사하지 않는다. bbox는 `x >= 0`, `y >= 0`, `width > 0`, `height > 0`만 검증하고 Client가 보낸 좌표를 그대로 저장한다.
- 기존 SQLite의 과거 Vision UNSENT 행은 삭제하지 않지만 시작 시 자동 재전송하지 않는다. 이전 프로토콜은 레거시 포트에 남아 있으므로 새 Jetson은 전용 포트를 사용한다.
- 센서는 기존 SQLite 저장·Ubuntu ACK 동기화를 유지한다. Arduino에 불필요한 ACK를 보내지 않도록 조정했다.

## Control과 장애

- Pi는 Jetson 연결 직후 `pause/server_state_pending`을 보낸다.
- Ubuntu가 DB 연결 및 테이블 조회 가능 상태를 확인하면 `resume/server_ready`를 보낸다. Pi가 이를 Jetson에 전달한다.
- Ubuntu DB 연결/쓰기 실패 시 `pause/database_unavailable`, 최종 Queue 포화 시 `pause/queue_overload`를 보낸다.
- Pi의 최종 서버 연결 오류 시 `pause/final_connection_lost`를 보낸다.
- 최종 서버 재연결 후 최초 상태를 받기 전에는 재개하지 않는다. 5초 이내 Control이 없으면 연결을 새로 시도한다.
- Queue 대기 데이터는 PAUSE/연결 종료 시 폐기하며 의도적인 replay는 없다. 이미 소켓/DB 처리 중인 데이터는 완료될 수 있다.
- DB 복구와 Queue 과부하 상태를 별도로 평가한다. Queue 과부하는 1초 이상 대기하고 Queue가 비며 DB 연결 상태가 정상일 때 재시도한다. 지속 과부하 시 반복 PAUSE가 가능하므로 운영 처리량 검증이 필요하다.
- 현재 상태/오류는 stderr 로그에 기록한다. `system_events` 테이블 저장 및 전체 장애 이력 복원은 이번 구현에 포함하지 않았다.
- Relay가 지원하지 않거나 필드 검증에 실패한 Vision JSON을 받으면 타입과 payload 앞부분(최대 512자)을 stderr에 남기고 해당 연결을 종료한다.

## 자원과 제한

- Pi/Ubuntu Queue 각각 64개. Pi 포화 시 가장 오래된 대기 데이터 폐기. Ubuntu 포화 시 대기 Queue 폐기 및 PAUSE.
- 수신과 DB 작업은 별도 스레드이며 Vision DB 연결은 센서와 공유하지 않는다.
- Pi는 단일 이벤트 루프가 양쪽 소켓의 수신/송신을 소유한다. 송신 프레임 중간에 Control이 끼어들지 않는다.
- 전체 프레임 I/O deadline 1초, 연결 시도 최대 1초, 재접속 간격 1초.
- keepalive idle 10초/interval 3초/probes 3, TCP_USER_TIMEOUT 20초. 실제 장애 감지 시각은 OS/상황에 따라 달라진다.
- MariaDB 연결·읽기·쓰기 timeout 각 3초. 일부 서버 측 DB 작업은 클라이언트 timeout 후에도 완료될 수 있다.
- 새 Vision 서비스는 동시 활성 Jetson 한 대/최종 연결 한 개 기준이다.
- Ubuntu는 새 데이터가 들어온 구간에만 5초마다 vision/vehicle_count의 수신·신규 저장 건수, dropped, Queue 깊이, 최근 DB 작업 지연과 최신 차량 수를 한 줄로 남긴다. saved는 신규 INSERT 건수로 중복 수신 건수와 다를 수 있다.
- 프로세스 강제 종료 시 미저장 Vision은 폐기된다. 전체 프로세스의 신호 기반 graceful shutdown/로그 영속화는 별도 운영 보완 항목이다.

## 확인한 내용

실제 Pi 및 Ubuntu의 `/tmp` 빌드와 전용 테스트 포트 15000~15003, 별도 MariaDB 검증 DB를 사용했다. 기존 운영 DB는 수정하지 않았다.

- 양쪽 `make`: 경고 없이 통과
- 가상 Jetson의 Vision 40건: MariaDB 40건 저장
- header/payload 분할 전송: 정상 수신
- 정상 Vision별 ACK 없음 확인
- 검증 DB의 Vision 테이블을 일시 변경해 쓰기 실패 유도: PAUSE 확인
- 검증 테이블 복구: RESUME 및 신규 Vision 저장 확인
- Vision DB 장애 중 Arduino 형식 메시지: 별도 센서 경로로 최종 저장 확인

남은 확인: 실제 Jetson 연동, 장시간 msg/s·drop·Queue 추이, 물리 네트워크 단절 및 keepalive, 부하/강제 종료 시 처리. 검증 결과만으로 무손실이나 무제한 처리량을 보장하지 않는다.
<!-- vehicle-count-update -->
> 2026-10-06 추가: `vision`과 `vehicle_count`는 1초 송신 주기를 사용합니다. 새 차량 수 메시지는 Pi 5002 → Ubuntu 5003 → MariaDB `vehicle_count`로 처리합니다. Pi SQLite에는 저장하지 않습니다. 상세 규격: [vehicle_count.md](vehicle_count.md). 기존 객체별 vision 형식과 센서 경로는 유지합니다.
