/** 사용자 입력 처리 및 TCP 통신 */

// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pthread.h>

// 서버와 일치시켜야 하는 패킷 규격 및 커맨드
#define CMD_LED_ON          101
#define CMD_LED_OFF         102
#define CMD_LED_SET_VALUE   103

#define CMD_BUZZER_ON       201
#define CMD_BUZZER_OFF      202

#define CMD_CDS_AUTO_ON     301
#define CMD_CDS_AUTO_OFF    302
#define CMD_CDS_SET_THRESHOLD 303
#define CMD_SENSOR_DATA     304

#define    CMD_SEGMENT_SET_DIGIT   401
#define    CMD_SEGMENT_CLEAR       402
#define    CMD_SEGMENT_SET_RAW     403
#define    CMD_SEGMENT_COUNTDOWN  404
#define    CMD_SEGMENT_TEST_RAW    405

typedef struct {
    int cmd;
    int val;
} ControlPacket;

char g_led_state[16] = "OFF";
int g_led_pwm = 70;
char g_buzzer_state[16] = "OFF";
char g_cds_mode[16] = "ON ";
int g_cds_threshold = 180;
int g_last_cds_val = -1;
int g_countdown_val = -1;

volatile sig_atomic_t g_win_resized = 0;
void update_led_state_on_panel(const char* state);

void handle_sigwinch(int sig) {
    g_win_resized = 1;
}

bool load_client_config(const char *path, char *ip, int *port) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // 개행 및 캐리지 리턴 제거
        line[strcspn(line, "\r\n")] = '\0';
        
        char *key = strtok(line, "=");
        if (key) {
            char *val = strtok(NULL, "=");
            if (val) {
                if (strcmp(key, "SERVER_IP") == 0) {
                    strcpy(ip, val);
                } else if (strcmp(key, "SERVER_PORT") == 0) {
                    *port = atoi(val);
                }
            }
        }
    }
    fclose(fp);
    return (strlen(ip) > 0 && *port > 0);
}

void update_countdown_on_panel(int val) {
    g_countdown_val = val;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        // 17행 42열(대시보드 상의 Countdown 표시부) 우측 좌표로 오버라이팅
        printf("\033[s\033[17;42H");
        if (val >= 0) {
            printf("\033[1;32m%-2d\033[0m", val);
        } else {
            printf("  ");
        }
        printf("\033[u");
        fflush(stdout);
    }
}

// CDS 값 수신 스레드
void* receive_thread(void *arg) {
    int sock = *(int*)arg;
    free(arg);
    ControlPacket pkt;

    while (1) {
        int n = read(sock, &pkt, sizeof(ControlPacket));
        if (n <= 0) {
            printf("\n[Disconnected from server]\n");
            close(sock);
            exit(0);
        }

        if (pkt.cmd == CMD_SENSOR_DATA) {
            g_last_cds_val = pkt.val;
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if(24 <= w.ws_row && w.ws_col >= 60){
                if (strcmp(g_cds_mode, "ON ") == 0) {
                    printf("\033[s\033[2;42H\033[1;33m[CDS: %d/%d]   \033[0m\033[u", pkt.val, g_cds_threshold);
                } else {
                    printf("\033[s\033[2;42H\033[1;30m[CDS: OFF]     \033[0m\033[u");
                }
                fflush(stdout);
            }
        }

        else if (pkt.cmd == CMD_SEGMENT_COUNTDOWN){
            update_countdown_on_panel(pkt.val);
        }
        else if (pkt.cmd == CMD_LED_ON) {
            update_led_state_on_panel("ON ");
        }
        else if (pkt.cmd == CMD_LED_OFF) {
            update_led_state_on_panel("OFF");
        }
    }

    close(sock);
    exit(0);
}

void init_terminal() {
    // \033[2J : 화면 전체 지우기, \033[H : 1행 1열로 커서 이동
    printf("\033[2J\033[H");
    fflush(stdout);
}

void cleanup_terminal() {
    printf("\033[2J\033[H\033[?1049l\033[?25h");
    fflush(stdout);
}

void draw_dashboard() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    // 최소 해상도 가용성 체크
    if (w.ws_row < 24 || w.ws_col < 60) {
        printf("\033[2J\033[H");
        printf("\033[1;31mPlease resize your terminal larger to display UI.\033[0m\n");
        printf("Current Terminal Size: %d columns x %d rows\n", w.ws_col, w.ws_row);
        printf("Minimum Requirement  : 60 columns x 24 rows\n");
        fflush(stdout);
        return;
    }

    printf("\033[H"); // 화면 홈으로 이동
    printf("=======================================================\n");
    printf("   VEDA Remote Control Client Panel                    \n"); // 우측 42열에 CDS 값 출력
    printf("=======================================================\n");
    printf(" [LED CONTROL]   (State: %-3s)                         \n", g_led_state);
    printf("   1. LED ON (Current PWM)\n");
    printf("   2. LED OFF\n");
    printf("   3. LED PWM Set (Current: \033[1;32m%-3d\033[0m)                  \n", g_led_pwm);
    printf(" [BUZZER CONTROL] (State: %-3s)                        \n", g_buzzer_state);
    printf("   4. Buzzer ON (10-sec Sequence Test)\n");
    printf("   5. Buzzer OFF\n");
    printf(" [CDS SENSOR CONTROL] (Auto Mode: %-3s)                \n", g_cds_mode);
    printf("   6. Enable CDS Auto Light Control\n");
    printf("   7. Disable CDS Auto Light Control\n");
    printf("   8. Set CDS Threshold (Current: \033[1;32m%-3d\033[0m)            \n", g_cds_threshold);
    printf(" [7-SEGMENT CONTROL]\n");
    printf("   9. Test 7-Segment Raw Sequence (a~h)\n");
    printf("  10. Test 7-Segment Countdown (Current: ");
    if (g_countdown_val >= 0) {
        printf("\033[1;32m%-2d\033[0m)                  \n", g_countdown_val);
    } else {
        printf("  )                  \n"); 
    }
    printf("  11. Clear 7-Segment\n");
    printf(" -------------------------------------------------------\n");
    printf("   q. Exit Client\n");
    printf("=======================================================\n");
    printf("Select menu > \033[K");
    
    // 잔여 임시 입력 프롬프트 / 송신 로그 지우기
    printf("\033[23;1H\033[K\033[24;1H\033[K");

    // 상단 2행 42열 CDS 표시부 갱신
    if (strcmp(g_cds_mode, "ON ") == 0) {
        if (g_last_cds_val != -1) {
            printf("\033[2;42H\033[1;33m[CDS: %d/%d]   \033[0m", g_last_cds_val, g_cds_threshold);
        } else {
            printf("\033[2;42H\033[1;33m[CDS: --/%d]   \033[0m", g_cds_threshold);
        }
    } else {
        printf("\033[2;42H\033[1;30m[CDS: OFF]     \033[0m");
    }
    
    // 최종 사용자 포커스를 고정 입력 칸으로 복귀
    printf("\033[22;15H");
    fflush(stdout);
}

void reset_input_line() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[22;15H\033[K");
        fflush(stdout);
    }
}

void update_led_state_on_panel(const char* state) {
    strncpy(g_led_state, state, sizeof(g_led_state) - 1);
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[s\033[4;18H\033[1;32m(State: %s)\033[0m \033[u", state);
        fflush(stdout);
    }
}

void update_led_pwm_on_panel(int val) {
    g_led_pwm = val;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[s\033[7;29H\033[1;32m%-3d\033[0m\033[u", val);
        fflush(stdout);
    }
}

void update_buzzer_state_on_panel(const char* state) {
    strncpy(g_buzzer_state, state, sizeof(g_buzzer_state) - 1);
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[s\033[8;19H\033[1;32m(State: %s)\033[0m \033[u", state);
        fflush(stdout);
    }
}

void update_cds_mode_on_panel(const char* mode) {
    strncpy(g_cds_mode, mode, sizeof(g_cds_mode) - 1);
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[s\033[11;23H\033[1;36m(Auto Mode: %s)\033[0m \033[u", mode);

        if (strcmp(mode, "ON ") == 0) {
            if (g_last_cds_val != -1) {
                printf("\033[s\033[2;42H\033[1;33m[CDS: %d/%d]   \033[0m\033[u", g_last_cds_val, g_cds_threshold);
            } else {
                printf("\033[s\033[2;42H\033[1;33m[CDS: --/%d]   \033[0m\033[u", g_cds_threshold);
            }
        } else {
            printf("\033[s\033[2;42H\033[1;30m[CDS: OFF]     \033[0m\033[u");
        }

        fflush(stdout);
    }
}

void update_threshold_on_panel(int val) {
    g_cds_threshold = val;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_row >= 24 && w.ws_col >= 60) {
        printf("\033[s\033[14;35H\033[1;32m%-3d\033[0m\033[u", val);
        if (strcmp(g_cds_mode, "ON ") == 0) {
            if (g_last_cds_val != -1) {
                printf("\033[s\033[2;42H\033[1;33m[CDS: %d/%d]   \033[0m\033[u", g_last_cds_val, g_cds_threshold);
            } else {
                printf("\033[s\033[2;42H\033[1;33m[CDS: --/%d]   \033[0m\033[u", g_cds_threshold);
            }
        }
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    char ip[64] = "";
    int port = 0;

    // 인자값이 부족할 때, 로컬에 저장된 server_config.ini에서 자동 로드
    if (argc >= 3) {
        strncpy(ip, argv[1], sizeof(ip) - 1);
        port = atoi(argv[2]);
    } else {
        if (load_client_config("config/server_config.ini", ip, &port)) {
            printf("[Config] Loaded target from 'config/server_config.ini': %s:%d\n", ip, port);
            sleep(1);
        } else {
            printf("Usage: %s <Server IP> <Port>\n", argv[0]);
            printf("Or make sure 'config/server_config.ini' exists in your working directory.\n");
            return -1;
        }
    }

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGWINCH);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) < 0){
        perror("sigprocmask failed");
        exit(EXIT_FAILURE);
    }

    atexit(cleanup_terminal);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return -1;
    }

    printf("Connecting to %s:%d...\n", ip, port);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }
    printf("Successfully connected to server.\n");
    sleep(1);

    // \033[?1049h : 독자적 대체 버퍼로 전환, \033[?25h : 디폴트 커서 사용 보증
    printf("\033[?1049h\033[?25h");
    fflush(stdout);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &sa, NULL);

    init_terminal();
    draw_dashboard();

    // 수신 전용 백그라운드 스레드 생성
    pthread_t r_tid;
    int *psock = malloc(sizeof(int));
    if (psock) {
        *psock = sock;
        if (pthread_create(&r_tid, NULL, receive_thread, psock) != 0) {
            perror("Failed to create receiver thread");
            free(psock);
        } else {
            pthread_detach(r_tid);
        }
    }

    char choice[32];
    ControlPacket pkt;

    while (1) {
        if (g_win_resized) {
            g_win_resized = 0;
            init_terminal();
            draw_dashboard();
            printf("#2\n");
        }

        reset_input_line();

        if (fgets(choice, sizeof(choice), stdin) == NULL){
            if (feof(stdin)){
                clearerr(stdin);
                continue;
            }

            if (errno == EINTR){
                clearerr(stdin);
                continue;
            }
            break;
        }
        
        choice[strcspn(choice, "\n")] = '\0'; // 개행 문자 제거

        // if (strlen(choice) == 0){
        //     continue;
        // }

        if (strcmp(choice, "q") == 0) {
            break;
        }

        if (strcmp(choice, "1") == 0 || strcmp(choice, "2") == 0 || strcmp(choice, "3") == 0) {
            if (strcmp(g_cds_mode, "ON ") == 0) {
                struct winsize w;
                ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                if (w.ws_row >= 24 && w.ws_col >= 60) {
                    printf("\033[24;1H\033[K\033[1;31m>> Access Denied: Locked in CDS AUTO Mode!\033[0m");
                    fflush(stdout);
                }
                usleep(500000);
                continue;
            }
        }

        memset(&pkt, 0, sizeof(ControlPacket));

        if (strcmp(choice, "1") == 0) {
            pkt.cmd = CMD_LED_ON;
            pkt.val = g_led_pwm;
            update_led_state_on_panel("ON ");
        } else if (strcmp(choice, "2") == 0) {
            pkt.cmd = CMD_LED_OFF;
            pkt.val = 0;
            update_led_state_on_panel("OFF");
        } else if (strcmp(choice, "3") == 0) {
            int custom_led_value = 70;
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[23;1H\033[KEnter LED PWM Value (0 ~ 100) [default: 70]: ");
                fflush(stdout);
            }
            if (scanf("%d", &custom_led_value) == 1) {
                pkt.cmd = CMD_LED_SET_VALUE;
                pkt.val = custom_led_value;
                update_led_pwm_on_panel(custom_led_value);
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // scanf 버퍼 비우기
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[23;1H\033[K"); // 입력 라인 지우기
                fflush(stdout);
            }
        } else if (strcmp(choice, "4") == 0) {
            pkt.cmd = CMD_BUZZER_ON;
            pkt.val = 0;
            update_buzzer_state_on_panel("ON ");
        } else if (strcmp(choice, "5") == 0) {
            pkt.cmd = CMD_BUZZER_OFF;
            pkt.val = 0;
            update_buzzer_state_on_panel("OFF");
        } else if (strcmp(choice, "6") == 0) {
            pkt.cmd = CMD_CDS_AUTO_ON;
            pkt.val = 0;
            update_cds_mode_on_panel("ON ");
        } else if (strcmp(choice, "7") == 0) {
            pkt.cmd = CMD_CDS_AUTO_OFF;
            pkt.val = 0;
            update_cds_mode_on_panel("OFF");
        } else if (strcmp(choice, "8") == 0) {
            int custom_threshold = 180;
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[23;1H\033[KEnter CDS Threshold (0 ~ 255) [default: 180]: ");
                fflush(stdout);
            }
            if (scanf("%d", &custom_threshold) == 1) {
                pkt.cmd = CMD_CDS_SET_THRESHOLD;
                pkt.val = custom_threshold;
                update_threshold_on_panel(custom_threshold);
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // scanf 버퍼 비우기
            if(w.ws_row >= 24 && w.ws_col >= 60){
                printf("\033[23;1H\033[K"); // 입력 라인 지우기
                fflush(stdout);
            }
            
        } else if (strcmp(choice, "9") == 0) {
            pkt.cmd = CMD_SEGMENT_TEST_RAW;
            pkt.val = 0;
        } else if (strcmp(choice, "10") == 0) {
            int start_val = 7;
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[23;1H\033[KEnter Countdown Start (1 ~ 9) [default: 7]: ");
                fflush(stdout);
            }
            if (scanf("%d", &start_val) == 1) {
                if (start_val < 1 || start_val > 9) start_val = 9;
                pkt.cmd = CMD_SEGMENT_COUNTDOWN;
                pkt.val = start_val;
                update_countdown_on_panel(start_val);
            } else {
                pkt.cmd = CMD_SEGMENT_COUNTDOWN;
                pkt.val = 7;
                update_countdown_on_panel(7);
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF); 
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[23;1H\033[K"); 
                fflush(stdout);
            }
        } else if (strcmp(choice, "11") == 0) {
            pkt.cmd = CMD_SEGMENT_CLEAR;
            pkt.val = 0;
            update_countdown_on_panel(-1);
        } else {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[24;1H\033[K\033[1;31m>> Invalid Selection: %s\033[0m", choice);
                fflush(stdout);
                usleep(300000);
            }
            continue;
        }

        if (pkt.cmd > 0) {
            if (write(sock, &pkt, sizeof(ControlPacket)) < 0) {
                perror("Transmission failed");
                break;
            }
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if (w.ws_row >= 24 && w.ws_col >= 60) {
                printf("\033[24;1H\033[K>> Command sent: %d (val: %d)", pkt.cmd, pkt.val);
                fflush(stdout);
            }
            usleep(200000);
        }
    }

    close(sock);
    printf("\nClient terminated.\n");
    return 0;
}