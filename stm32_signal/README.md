# STM32 신호등 점등 확인

NUCLEO-F411RE / CubeMX CMake-GCC 프로젝트.

- 빨강 PB0(A3), 노랑 PA1(A1), 초록 PA4(A2).
- 각 GPIO → 330Ω 저항 → LED 양극, LED 음극 → GND.
- HC-06: RXD→PA9(D8), TXD→PA10(D2), GND 공통. VCC는 확인된 모듈 지원 전압 사용.
- USART1: 9600 / 8N1. HC-06으로 받은 줄 단위 명령으로 LED를 제어한다.
- 시작 시 빨강. 유효 명령을 5초 이상 받지 못하면 빨강으로 돌아간다.
- Nucleo의 ST-LINK USB 커넥터를 PC에 연결한다.

저장소 루트 PowerShell에서:

```powershell
# 빌드
.\stm32_signal\build.ps1
# 빌드 후 ST-LINK 업로드, 검증 및 리셋
.\stm32_signal\build.ps1 -Flash
```

스크립트는 PATH 또는 C:\ST 아래 CubeIDE 설치 도구를 찾아 현재 실행에만 사용한다.
PowerShell 실행 정책으로 차단된다면 `powershell -NoProfile -ExecutionPolicy Bypass -File .\stm32_signal\build.ps1 -Flash`를 사용한다.

VS Code에서 stm32_signal 폴더를 열면 Ctrl+Shift+B로 빌드하고, Terminal → Run Task → STM32: Build and Flash로 업로드할 수 있다.
빌드 결과는 build/Debug/stm32_signal.elf이며 build 폴더는 Git에서 제외한다.
점등 코드는 main.c의 USER CODE 블록에 있어 CubeMX 재생성 시 보존된다.

## Bluetooth 명령 (수동 통신 확인 단계)

ASCII 명령 끝에는 LF(개행)를 붙인다. CRLF도 허용한다.

| 명령 | 응답 |
| --- | --- |
| SET,1,RED | OK,1,RED |
| SET,1,YELLOW | OK,1,YELLOW |
| SET,1,GREEN | OK,1,GREEN |

지원하지 않는 명령/ID는 ERROR,1,COMMAND로 응답하고 LED를 변경하지 않는다.
명령은 하나씩 보내고 응답을 확인한다. 색상을 유지하려면 같은 명령을 1초마다 보낸다.
연결 상태는 Bluetooth 무선 상태 대신 유효 명령 수신 시각으로 판단한다.
5초 미수신 시 STATE,1,RED,TIMEOUT 응답을 보내며 빨강이 유지된다.
최대 명령 길이는 개행 제외 31바이트, 불완전한 명령은 1초 미수신 시 폐기한다.

현재 단계는 원격 개별 점등 확인이다. 차량 수 임계값, 최소 녹색 30초/최대 60초, 황색 전환 시간은 아직 적용하지 않았다.
PA9→HC-06 RXD, PA10←HC-06 TXD 연결을 확인한다. USB COM7은 USART2라 Bluetooth 명령 입력 포트가 아니다.

## 실제 모듈 연결 확인

- 전원을 켰다/껐다 비교하여 사용자 모듈을 식별했다.
- Bluetooth 이름: bt28 / MAC: 98:D3:C1:FD:4B:B6.
- Pi 페어링과 신뢰 등록 완료. RFCOMM 채널 1 연결 및 명령 송신 성공.
- 교체 보드 ST-LINK ID 066DFF3134584B3043195748에 펌웨어 업로드/검증 완료.
- UART 9600에서 RED/YELLOW/GREEN 명령별 OK 응답과 5초 미수신 STATE,1,RED,TIMEOUT 응답을 실제 확인했다.
- Bluetooth 수신은 응답 하나가 여러 조각으로 나올 수 있으므로 개행까지 누적해서 읽어야 한다.
