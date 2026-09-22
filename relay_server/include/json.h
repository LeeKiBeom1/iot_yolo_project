#ifndef MESSAGE_JSON_H
#define MESSAGE_JSON_H

int parse_message_id(const char *json, char *message_id, int message_id_size);
int create_ack_json(const char *message_id, char *buffer, int buffer_size);
int validate_ack_json(const char *json, const char *expected_message_id);

#endif
