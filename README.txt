VEDA 4기 심화 실습 평가2(리눅스 프로그래밍)
7회차 이유준

자세한 내용은 링크를 참고하세요.
https://github.com/Vlolet/VEDA_Linux_Project

[프로젝트 개요]
본 프로젝트는 Ubuntu 리눅스(Client)에서 Raspberry Pi 4(Server)에 연결된 하드웨어 장치들을 TCP 통신을 통해 원격으로 제어하는 프로그램입니다.

[빌드 전 확인 사항]
1. 라즈베리파이에 wiringPi가 설치되어 있어야 합니다. (https://github.com/WiringPi/WiringPi)
2. 호스트(Ubuntu)에 라즈베리파이용 크로스 컴파일러가 세팅되어 있어야 합니다.
3. config/server_config.ini 파일에 RPI_IP와 SERVER_IP, 포트, 핀 번호(BCM 기준)를 알맞게 기입하세요. (제출본에는 기본값 세팅됨)

[빌드 및 배포]
$ ./build.sh
- 클라이언트는 Host용으로 빌드되고, 서버용 실행파일과 .so 파일들은 크로스 컴파일되어 라즈베리파이 지정 경로로 자동 전송(rsync)됩니다.

[실행 방법]
1. 라즈베리파이 (서버)
   $ cd ~/veda/project_linux  # RPI_DEST 경로
   $ ./server
   * 서버는 데몬(Daemon) 프로세스로 백그라운드에서 실행되며 syslog에 로그를 남깁니다.

2. 우분투 (클라이언트)
   $ ./client
   * 터미널 창을 충분히 키운 후 실행하세요 (최소 60x24 해상도 권장).

[동적 라이브러리(Hot-Swap) 업데이트 테스트]
1. 호스트에서 하드웨어 소스코드(예: src/devices/led.c) 수정
2. ./build.sh 로 다시 빌드 및 라즈베리파이로 전송 (.so 파일 업데이트됨)
3. 라즈베리파이 터미널에서 서버 데몬에 SIGUSR1 시그널 전송
   $ sudo killall -SIGUSR1 server
4. 서버 재시작 없이 업데이트된 장치 모듈이 즉시 적용됩니다.