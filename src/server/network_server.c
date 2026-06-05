/** TCP 소켓 관리 및 클라이언트 통신 스레드 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "config.h"
#include "device_interface.h"

extern volatile sig_atomic_t g_reload_req;
extern volatile sig_atomic_t g_running;

void* client_handler_thread(void *arg);

int g_client_fd = -1;

int start_network_server() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    int port = g_cfg.net.port;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        SYSTEM_LOG("[network] Socket creation failed: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        SYSTEM_LOG("[network] setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    memset(&(address.sin_zero), '\0', 8);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        SYSTEM_LOG("[network] Bind failed on port %d: %s", port, strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, TCP_BACKLOG_LEN) < 0) {
        SYSTEM_LOG("[network] Listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    SYSTEM_LOG("[network] Server listening on port %d...", port);

    while (g_running) {
        // how-swap 요청
        if (g_reload_req) {
            reload_devices();
            g_reload_req = 0;
        }

        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                // 시그널(SIGUSR1, SIGINT 등)에 의해 블로킹이 깨짐
                // 루프 처음으로 돌아가 플래그(g_reload_req, g_running) 체크
                continue;
            }
            SYSTEM_LOG("[network] Accept failed: %s", strerror(errno));
            // 치명적이지 않은 소켓 에러라면 잠시 대기 후 루프 유지
            usleep(100000); 
            continue;
        }

        // 새 클라이언트가 연결되었을 때 기존 연결이 존재하면 정리 (나중에 수정)
        if (g_client_fd > 0) {
            SYSTEM_LOG("[network] Closing previous client connection to accept new client.");
            close(g_client_fd);
            g_client_fd = -1;
        }

        int *pclient = malloc(sizeof(int));
        if (pclient == NULL) {
            SYSTEM_LOG("[network] Memory allocation failed for client_fd");
            close(client_fd);
            continue;
        }
        *pclient = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler_thread, pclient) != 0) {
            SYSTEM_LOG("[network] Failed to create client handler thread");
            close(client_fd);
            free(pclient);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    SYSTEM_LOG("[network] Server socket closed.");
    return 0;
}

 void* client_handler_thread(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    g_client_fd = client_fd;
    SYSTEM_LOG("[network] Client connected.");

    // 연결 시점 세팅
    led_handler_control(CMD_LED_OFF, 0);
    buzzer_handler_control(CMD_BUZZER_OFF);
    // 연결 시점부터 조도 센서 자동 모니터링 시작
    cds_handler_control(CMD_CDS_AUTO_ON, 0);
    segment_handler_control(CMD_SEGMENT_CLEAR, 0);

    ControlPacket pkt;
    while (1) {
        // 클라이언트로부터 패킷 대기 (블로킹 수신)
        int n = read(client_fd, &pkt, sizeof(ControlPacket));
        if (n <= 0) {
            // 접속 끊김
            break;
        }

        switch (pkt.cmd) {
            case CMD_LED_ON:
            case CMD_LED_OFF:
            case CMD_LED_SET_VALUE:
                led_handler_control(pkt.cmd, pkt.val);
                break;
            case CMD_BUZZER_ON:
            case CMD_BUZZER_OFF:
                buzzer_handler_control(pkt.cmd);
                break;
            case CMD_CDS_AUTO_ON:
            case CMD_CDS_AUTO_OFF:
            case CMD_CDS_SET_THRESHOLD:
                cds_handler_control(pkt.cmd, pkt.val);
                break;
            case CMD_SEGMENT_SET_DIGIT:
            case CMD_SEGMENT_CLEAR:
            case CMD_SEGMENT_SET_RAW:
            case CMD_SEGMENT_COUNTDOWN:
            case CMD_SEGMENT_TEST_RAW:
                segment_handler_control(pkt.cmd, pkt.val);
                break;
            // 추가 장치 명령 분기 처리...
            default:
                SYSTEM_LOG("[network] Unknown command: %d", pkt.cmd);
                break;
        }
    }

    // 정리 작업
    cds_handler_control(CMD_CDS_AUTO_OFF, 0);
    close(client_fd);
    g_client_fd = -1;
    SYSTEM_LOG("[network] Client disconnected.");
    return NULL;
}