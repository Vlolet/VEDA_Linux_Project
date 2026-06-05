# 🚀 VEDA Linux Remote Control Project

Ubuntu 기반의 클라이언트에서 TCP 통신을 통해 Raspberry Pi 4의 하드웨어를 원격 제어하는 시스템입니다. **데몬(Daemon) 프로세스**, **동적 라이브러리(.so)** 를 이용한 핫스왑(Hot-swap), 그리고 **멀티스레딩** 기반의 안정적인 장치 제어 기능을 제공합니다.

## Based on
- ![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi) ![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white)   
- ![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white) 
- ![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white) ![Bash Script](https://img.shields.io/badge/bash_script-%23121011.svg?style=for-the-badge&logo=gnu-bash&logoColor=white)

## Preview
<div align="center">
  <img width="45%" alt="image" src="https://github.com/user-attachments/assets/b1cb9dbc-1b0b-4266-98e5-8f27f81c0adc" style="max-height: 400px; object-fit: contain; margin-right: 10px;" />
  <img width="45%" alt="image" src="https://github.com/user-attachments/assets/bfc3ef1b-0468-4393-b729-b90cbbdde4f8" style="max-height: 400px; object-fit: contain;" />
</div>

## 📌 Features

*   **Network**: TCP/IPv4 기반의 Server-Client 구조
*   **Architecture (Server)**
    *   백그라운드 서비스 동작을 위한 데몬(Daemon) 프로세스 구현
    *   하드웨어 모듈의 독립성을 위한 동적 라이브러리(.so) 설계
    *   `SIGUSR1` 시그널을 이용한 무중단 라이브러리 리로드(Hot-swapping)
*   **Architecture (Client)**
    *   ANSI Escape Sequence를 활용한 실시간 대시보드 TUI (Terminal UI)
    *   비동기 패킷 수신 스레드와 `SIGWINCH`를 통한 반응형 터미널 렌더링
*   **Hardware Control** (via wiringPi)
    *   `LED`: On/Off 및 `softPwm`을 이용한 0~100단계 밝기 제어
    *   `Buzzer`: On/Off 제어 및 타이머 연동 동작
    *   `CDS (조도센서)`: I2C 통신, 임계값 설정 및 자동(Auto) 모드 시 어두워지면 LED 자동 점등
    *   `7-Segment`: 숫자 표시, a~h Raw 테스트, n초 카운트다운(0초 도달 시 부저 울림)

## 📁 Project Structure
```text
project_linux/
├── build.sh                 # 자동화 빌드 및 RPi 전송 스크립트
├── CMakeLists.txt           # CMake 빌드 설정
├── config/                  
│   └── server_config.ini    # IP, Port, BCM 핀 번호 설정 파일
├── include/                 # 공통 헤더 (common.h, device_interface.h 등)
├── src/
│   ├── client/              # 클라이언트 애플리케이션 (UI, TCP)
│   ├── server/              # 서버 애플리케이션 (Daemon, Network, Device Manager)
│   │   └── handlers/        # 각 장치별 비즈니스 로직 및 워커 스레드
│   └── devices/             # 하드웨어 제어 동적 라이브러리 (led.c, cds.c ...)
└── README.md
```

## ⚙️ Prerequisites

  - Server (Raspberry Pi 4)
      - OS: `Raspberry Pi OS`
      - Library: `wiringPi`
  - Client (Host PC)
      - OS: `Ubuntu Linux`
      - Tools: `cmake`, `gcc`, `arm-linux-gnueabihf-gcc` (Cross-compiler)

## 🛠️ Build & Deploy

> `build.sh` 스크립트를 사용하여 Host용 클라이언트 빌드 및 RPi용 서버 크로스 컴파일을 동시에 수행합니다.
```bash
# 1. 환경 설정 파일 세팅 (config/server_config.ini)
# SERVER_IP, PORT, RPI_IP, RPI_USER 등을 본인 환경에 맞게 수정

# 2. 빌드 및 라즈베리파이로 자동 전송 (rsync 기반)
$ ./build.sh
```
## ▶️ Usage

1. Server Run (Raspberry Pi)
```bash
$ cd ~/veda/project_linux
$ sudo ./server
# syslog를 통해 로그 확인 가능: tail -f /var/log/syslog | grep "Veda"
```
2. Client Run (Ubuntu)
```bash
$ ./client
# 터미널 사이즈를 60x24 이상으로 늘린 후 실행하세요.
```
3. Hot-Swap Test (무중단 모듈 업데이트)

1.  `src/devices/led.c` 등 로직 수정
2.  `./build.sh` 로 `.so` 라이브러리만 재빌드하여 RPi로 전송
3.  RPi 터미널에서 `sudo killall -SIGUSR1 server` 입력
4.  서버 재시작 없이 변경된 로직 즉시 적용!


<br>
<hr>
<br>
