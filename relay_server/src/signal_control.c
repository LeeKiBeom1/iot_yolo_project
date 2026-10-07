#include "signal_control.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define SAMPLE_COUNT 5
#define LOW_MAX 6
#define NORMAL_MAX 11
#define GREEN_LOW_MS 5000
#define GREEN_NORMAL_MS 10000
#define GREEN_HIGH_MS 15000
#define YELLOW_MS 2000
#define RED_MS 5000
#define COMMAND_INTERVAL_MS 1000
#define DATA_TIMEOUT_MS 10000

typedef enum {
    SIGNAL_WAIT,
    SIGNAL_GREEN,
    SIGNAL_YELLOW,
    SIGNAL_RED
} SignalPhase;

/* Vision은 표본만 추가하고, 제어 스레드가 신호 판단과 Bluetooth를 전담한다.
   공유하는 표본/수신 시각만 mutex로 보호한다. Bluetooth 대기 중에는 잠금을 잡지 않는다.
   정상 1초 송신 기준 최근 5개 ≈ 5초이며, 정확한 시간 가중 평균은 아니다. */
static pthread_mutex_t sample_mutex = PTHREAD_MUTEX_INITIALIZER;
static int samples[SAMPLE_COUNT];
static int sample_index;
static int sample_size;
static int samples_reset;
static int serial_fd = -1;
static int64_t last_sample_ms;
static int64_t phase_deadline_ms;
static int64_t last_command_ms;
static int64_t retry_open_ms;
static SignalPhase phase = SIGNAL_WAIT;

static int64_t monotonic_ms(void)
{
    /* 시스템 시계가 보정돼도 신호 유지 시간이 변하지 않는 경과 시간 시계. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static const char *phase_name(SignalPhase value)
{
    if (value == SIGNAL_GREEN) return "GREEN";
    if (value == SIGNAL_YELLOW) return "YELLOW";
    return "RED";
}

static int open_serial(int64_t now)
{
    const char *path = getenv("SIGNAL_DEVICE");
    struct termios settings;

    if (serial_fd >= 0) return 0;
    if (now < retry_open_ms) return -1;
    if (path == NULL || *path == '\0') path = "/dev/rfcomm0";
    serial_fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) {
        fprintf(stderr, "signal controller: cannot open %s: %s\n",
                path, strerror(errno));
        retry_open_ms = now + 5000;
        return -1;
    }
    if (tcgetattr(serial_fd, &settings) < 0) goto failed;
    cfmakeraw(&settings);
    cfsetispeed(&settings, B9600);
    cfsetospeed(&settings, B9600);
    settings.c_cflag |= CLOCAL | CREAD;
    if (tcsetattr(serial_fd, TCSANOW, &settings) < 0) goto failed;
    fprintf(stderr, "signal controller: connected %s at 9600 baud\n", path);
    return 0;

failed:
    fprintf(stderr, "signal controller: serial setup failed: %s\n",
            strerror(errno));
    close(serial_fd);
    serial_fd = -1;
    retry_open_ms = now + 5000;
    return -1;
}

static void send_phase(int64_t now)
{
    char command[32];
    char replies[128];
    int length;

    if (open_serial(now) < 0) return;
    /* 장치 열기가 오래 걸렸다면 이전에 결정한 색을 보내지 않는다.
       다음 제어 반복에서 최신 표본과 타임아웃으로 신호를 다시 판단한다. */
    if (monotonic_ms() - now >= COMMAND_INTERVAL_MS) return;
    /* STM32 응답은 버퍼가 쌓이지 않도록 비운다. 현재는 ACK 내용 검증을 하지 않는다.
       한 번만 읽어 응답이 계속 들어와도 네트워크 처리 시간을 독점하지 않는다. */
    (void)read(serial_fd, replies, sizeof(replies));
    length = snprintf(command, sizeof(command), "SET,1,%s\n",
                      phase_name(phase));
    if (write(serial_fd, command, (size_t)length) != length) {
        fprintf(stderr, "signal controller: write failed: %s\n",
                strerror(errno));
        close(serial_fd);
        serial_fd = -1;
        retry_open_ms = now + 1000;
        return;
    }
    last_command_ms = now;
}

static double sample_average(void)
{
    int index;
    int64_t sum = 0; /* JSON은 INT_MAX까지 허용하므로 5개 합은 64비트로 계산한다. */
    for (index = 0; index < sample_size; index++) sum += samples[index];
    return sample_size ? (double)sum / sample_size : 0.0;
}

static void start_green(int64_t now)
{
    /* 초록 시작 시 길이를 확정한다. 진행 중에는 새 표본으로 시간을 바꾸지 않는다. */
    double average = sample_average();
    int duration = average <= LOW_MAX ? GREEN_LOW_MS :
                   average <= NORMAL_MAX ? GREEN_NORMAL_MS : GREEN_HIGH_MS;
    phase = SIGNAL_GREEN;
    phase_deadline_ms = now + duration;
    last_command_ms = 0;
    fprintf(stderr, "signal phase=GREEN average=%.2f duration=%d\n",
            average, duration / 1000);
}

void signal_control_add_sample(int vehicle_count)
{
    int64_t now = monotonic_ms();
    pthread_mutex_lock(&sample_mutex);
    /* 재연결 직후 오래된 표본과 새 표본이 섞이지 않도록 한다. */
    if (last_sample_ms && now - last_sample_ms > DATA_TIMEOUT_MS) {
        sample_index = 0;
        sample_size = 0;
        samples_reset = 1;
    }
    samples[sample_index] = vehicle_count;
    sample_index = (sample_index + 1) % SAMPLE_COUNT;
    if (sample_size < SAMPLE_COUNT) sample_size++;
    last_sample_ms = now;
    pthread_mutex_unlock(&sample_mutex);
}

static void signal_control_tick(void)
{
    int64_t now = monotonic_ms();

    pthread_mutex_lock(&sample_mutex);
    /* Vision 스레드는 phase를 직접 바꾸지 않는다. 새 세션 표본으로 전환된 경우
       제어 스레드가 여기서 이전 신호 주기를 끝내고 표본 준비 상태를 확인한다. */
    if (samples_reset) {
        phase = SIGNAL_WAIT;
        phase_deadline_ms = 0;
        last_command_ms = 0;
        samples_reset = 0;
    }
    if (last_sample_ms == 0 || now - last_sample_ms > DATA_TIMEOUT_MS) {
        if (phase != SIGNAL_WAIT || sample_size) {
            fprintf(stderr, "signal phase=RED reason=vehicle_count_timeout\n");
            phase = SIGNAL_WAIT;
            sample_index = 0;
            sample_size = 0;
            phase_deadline_ms = 0;
            last_command_ms = 0;
        }
    } else if (phase == SIGNAL_WAIT && sample_size == SAMPLE_COUNT) {
        start_green(now);
    } else if (phase_deadline_ms && now >= phase_deadline_ms) {
        if (phase == SIGNAL_GREEN) {
            phase = SIGNAL_YELLOW;
            phase_deadline_ms = now + YELLOW_MS;
        } else if (phase == SIGNAL_YELLOW) {
            phase = SIGNAL_RED;
            phase_deadline_ms = now + RED_MS;
        } else if (phase == SIGNAL_RED) {
            start_green(now);
        }
        last_command_ms = 0;
        if (phase != SIGNAL_GREEN)
            fprintf(stderr, "signal phase=%s duration=%d\n",
                    phase_name(phase),
                    phase == SIGNAL_YELLOW ? YELLOW_MS / 1000 : RED_MS / 1000);
    }
    pthread_mutex_unlock(&sample_mutex);
    /* 현재 색을 1초마다 보내 STM32가 통신 생존 여부를 확인할 수 있게 한다.
       차량 데이터 장애는 정상 RED 명령, Bluetooth 명령 장애는 STM32 점멸로 구분된다. */
    if (now - last_command_ms >= COMMAND_INTERVAL_MS) send_phase(now);
}

void *signal_control_service(void *unused)
{
    (void)unused;
    fprintf(stderr, "signal controller: dedicated worker started\n");
    for (;;) {
        struct timespec interval = {0, 100000000}; /* 100ms마다 신호 시간 확인 */
        signal_control_tick();
        nanosleep(&interval, NULL);
    }
    return NULL;
}
