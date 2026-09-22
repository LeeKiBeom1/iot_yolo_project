#ifndef MESSAGE_JSON_H
#define MESSAGE_JSON_H

typedef struct {
    char device_id[33];
    char message_id[65];
    char timestamp[20];
    int light;
    float temperature;
    float humidity;
    int sound;
} SensorMessage;

int parse_sensor_json(const char *json, SensorMessage *message);
int create_sensor_json(const SensorMessage *message,
                       char *buffer, int buffer_size);
int create_ack_json(const char *message_id, const char *status,
                    int duplicate, const char *error_code,
                    char *buffer, int buffer_size);
int validate_ack_json(const char *json, const char *expected_message_id);

#endif
