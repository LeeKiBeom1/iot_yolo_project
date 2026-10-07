# 서버 실행·종료·상태 확인

Relay Raspberry Pi와 Ubuntu VM에서 직접 서버를 실행할 때 사용하는 명령이다. 서버 로그를 터미널에서 보려면 백그라운드 실행 명령인 `nohup`을 사용하지 않는다.

## Relay Server (Raspberry Pi)

### 실행 상태 확인

```bash
pgrep -a -x relay_server
sudo ss -ltnp | grep -E ':5000|:5002'
```

- `pgrep` 결과에 PID와 `./relay_server`가 나오면 프로세스가 실행 중이다.
- `5000`은 Arduino 센서 수신, `5002`는 Jetson Vision 수신 포트다.
- 두 명령 모두 결과가 없으면 Relay Server가 실행 중이 아니다.

### 종료

직접 실행한 터미널에서는 `Ctrl+C`로 종료한다. 다른 터미널에서 종료할 때는 다음 명령을 사용한다.

```bash
pkill -x relay_server
```

### 직접 실행

Ubuntu Server를 먼저 실행한 뒤 Relay Server를 실행한다.

```bash
cd /home/pi/iot_project/relay_server
./relay_server
```

터미널을 닫으면 서버도 종료된다.

## Ubuntu Server

### 실행 상태 확인

```bash
pgrep -a -x ubuntu_server
sudo ss -ltnp | grep -E ':5001|:5003'
```

- `pgrep` 결과에 PID와 `./ubuntu_server`가 나오면 프로세스가 실행 중이다.
- `5001`은 Relay 센서 데이터 수신, `5003`은 Relay Vision 데이터 수신 포트다.
- 두 명령 모두 결과가 없으면 Ubuntu Server가 실행 중이 아니다.

### 종료

직접 실행한 터미널에서는 `Ctrl+C`로 종료한다. 다른 터미널에서 종료할 때는 다음 명령을 사용한다.

```bash
pkill -x ubuntu_server
```

### 직접 실행

비밀번호가 터미널 명령 기록에 남지 않도록 먼저 입력받은 뒤 실행한다.

```bash
cd /home/ubuntu/iot_project/ubuntu_server
read -s -p "MariaDB password: " DB_PASSWORD; echo
export DB_PASSWORD
DB_HOST=localhost DB_USER=ubuntu DB_NAME=road_monitor ./ubuntu_server
```

터미널을 닫으면 서버도 종료된다.

## 권장 실행 순서

1. Ubuntu Server를 실행한다.
2. 다른 터미널에서 Relay Server를 실행한다.
3. Jetson과 Arduino를 실행한다.

Ubuntu에서 새 Vision 데이터가 들어오면 새 데이터가 존재하는 구간에만 5초마다 다음 형식의 로그가 출력된다.

```text
data 5s vision_rx=10 vehicle_count_rx=5 vision_saved=10 vehicle_count_saved=5 dropped=0 queue=0 db_ms=1 ready=1 latest_vehicle_count=18 frame_id=1234 timestamp_ms=1791255600000
```

`dropped=0`, `ready=1`이면 정상이다. 5초 동안 새 메시지가 없으면 이 로그는 출력되지 않는다.

## HC-06 연결

Relay Server를 실행하기 전에 페어링된 HC-06을 RFCOMM 장치로 연결한다.

```bash
sudo rfcomm bind 0 98:D3:C1:FD:4B:B6 1
sudo chown root:dialout /dev/rfcomm0
sudo chmod 660 /dev/rfcomm0
```

연결 상태는 `rfcomm`과 `bluetoothctl info 98:D3:C1:FD:4B:B6`으로 확인한다. 재부팅하면 바인딩을 다시 실행해야 한다. 신호 제어 규칙은 [signal_control.md](signal_control.md)를 참고한다.

## 빌드가 필요한 경우

각 서버 디렉터리에서 실행한다.

```bash
make clean
make
```

빌드 후 실행 중인 서버를 종료하고 다시 실행해야 새 실행파일이 적용된다.
