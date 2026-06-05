/** RPI Server code */
/* 데몬 프로세스 구현, 시그널 핸들링, 메인 루프*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>

#include "common.h"
#include "config.h"
#include "device_interface.h"

void daemonize();
void handle_sigint(int sig);
void handle_sigterm(int sig);
void handle_sigusr1(int sig);

volatile sig_atomic_t g_reload_req = 0;
volatile sig_atomic_t g_running = 1;

int main(){
    SYSTEM_LOG("########## SERVER START ###########");

    // init working directory
    init_base_path();

    // load config (IP, Port, Threshold, ...)
    char abs_config_path[MAX_PATH];
    MAKE_ABSOLUTE_PATH(abs_config_path, sizeof(abs_config_path), "server_config.ini");

    if(load_config(abs_config_path) < 0){
        SYSTEM_LOG("FAILED: load_config(%s)", abs_config_path);
        exit(EXIT_FAILURE);
    }

    // set daemon
    daemonize();

    // device manager init (dlopen...)
    if(init_device_manager() < 0){
        SYSTEM_LOG("FAILED: init_device_manager");
        exit(EXIT_FAILURE);
    }

    // server start
    if(start_network_server() < 0){
        SYSTEM_LOG("FAILED: start_network_server");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}

void daemonize(){
    struct sigaction sa; /* 시그널 처리를 위한 시그널 액션 */
    struct rlimit rl;
    int fd0, fd1, fd2, i;
    pid_t pid;

    /* 파일 생성을 위한 마스크를 0으로 설정 */
    umask(0);

    /* 사용할 수 있는 최대의 파일 디스크립터 수 얻기 */
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("getlimit()");
    }

    if((pid = fork()) < 0) {
        perror("error()");
    } else if(pid != 0) { /* 부모 프로세스는 종료한다. */
        exit(0);
    }

    /* 터미널을 제어할 수 없도록 세션의 리더가 된다. */
    setsid();

    /* 터미널 제어와 관련된 시그널을 무시한다. */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction() : Can't ignore SIGHUP");
    }
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigterm);
    signal(SIGUSR1, handle_sigusr1);

    /* 프로세스의 워킹 디렉터리를 ‘/’로 설정한다. */
    if(chdir("/") < 0) {
        perror("cd()");
    }

    /* 프로세스의 모든 파일 디스크립터를 닫는다. */
    if(rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }

    for(i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    /* 파일 디스크립터 0, 1과 2를 /dev/null로 연결한다. */
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    SYSTEM_LOG("SUCCESS:Daemon process Started (PID: %d)", getpid());
    return;
}

void handle_sigint(int sig){
    SYSTEM_LOG("Server Interrupting...");
    g_running = 0;
    cleanup_devices();
    exit(0);
}

void handle_sigterm(int sig){
    SYSTEM_LOG("Server terminating...");
    g_running = 0;
    cleanup_devices();
    SYSTEM_LOG("########## SERVER STOP (main) ###########");
    exit(0);
}

void handle_sigusr1(int sig){
    g_reload_req = 1;
    return;
}