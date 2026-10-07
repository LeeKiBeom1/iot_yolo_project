#include "signal_control.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
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

static int samples[SAMPLE_COUNT];
static int sample_index;
static int sample_size;
static int serial_fd = -1;
static int64_t last_sample_ms;
static int64_t phase_deadline_ms;
static int64_t last_command_ms;
static int64_t retry_open_ms;
static SignalPhase phase = SIGNAL_WAIT;

static int64_t monotonic_ms(void)
{
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
    while (read(serial_fd, replies, sizeof(replies)) > 0) {}
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
    int sum = 0;
    for (index = 0; index < sample_size; index++) sum += samples[index];
    return sample_size ? (double)sum / sample_size : 0.0;
}

static void start_green(int64_t now)
{
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
    samples[sample_index] = vehicle_count;
    sample_index = (sample_index + 1) % SAMPLE_COUNT;
    if (sample_size < SAMPLE_COUNT) sample_size++;
    last_sample_ms = now;
    if (phase == SIGNAL_WAIT && sample_size == SAMPLE_COUNT) start_green(now);
}

void signal_control_tick(void)
{
    int64_t now = monotonic_ms();

    if (last_sample_ms == 0 || now - last_sample_ms > DATA_TIMEOUT_MS) {
        if (phase != SIGNAL_WAIT) {
            fprintf(stderr, "signal phase=RED reason=vehicle_count_timeout\n");
            phase = SIGNAL_WAIT;
            sample_index = 0;
            sample_size = 0;
            phase_deadline_ms = 0;
            last_command_ms = 0;
        }
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
    if (now - last_command_ms >= COMMAND_INTERVAL_MS) send_phase(now);
}

void signal_control_close(void)
{
    if (serial_fd >= 0) close(serial_fd);
    serial_fd = -1;
}
