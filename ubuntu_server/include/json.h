#ifndef MESSAGE_JSON_H
#define MESSAGE_JSON_H

#include <stdint.h>

typedef struct {
    char device_id[33];
    char message_id[65];
    uint64_t frame_id;
    int64_t timestamp_ms;
    int class_id;
    char class_name[51];
    float confidence;
    int x;
    int y;
    int width;
    int height;
} VisionMessage;

int parse_vision_json(const char *json, VisionMessage *message);
int create_ack_json(const char *message_id, const char *status,
                    int duplicate, const char *error_code,
                    char *buffer, int buffer_size);

#endif
