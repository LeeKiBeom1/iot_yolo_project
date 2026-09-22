#include "json.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

int parse_message_id(const char *json, char *message_id, int message_id_size)
{
    cJSON *root;
    cJSON *version;
    cJSON *type;
    cJSON *id;
    int result = -1;

    if (json == NULL || message_id == NULL || message_id_size <= 1) {
        return -1;
    }

    root = cJSON_Parse(json);
    if (root == NULL) {
        return -1;
    }

    version = cJSON_GetObjectItemCaseSensitive(root, "version");
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    id = cJSON_GetObjectItemCaseSensitive(root, "message_id");

    if (cJSON_IsNumber(version) && version->valueint == 1 &&
        cJSON_IsString(type) &&
        (strcmp(type->valuestring, "sensor") == 0 ||
         strcmp(type->valuestring, "vision") == 0) &&
        cJSON_IsString(id) && id->valuestring[0] != '\0' &&
        (int)strlen(id->valuestring) < message_id_size) {
        snprintf(message_id, message_id_size, "%s", id->valuestring);
        result = 0;
    }

    cJSON_Delete(root);
    return result;
}

int create_ack_json(const char *message_id, char *buffer, int buffer_size)
{
    cJSON *root;
    int result = -1;

    if (message_id == NULL || buffer == NULL || buffer_size <= 1) {
        return -1;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }

    if (cJSON_AddNumberToObject(root, "version", 1) != NULL &&
        cJSON_AddStringToObject(root, "type", "ack") != NULL &&
        cJSON_AddStringToObject(root, "message_id", message_id) != NULL &&
        cJSON_AddStringToObject(root, "status", "ok") != NULL &&
        cJSON_PrintPreallocated(root, buffer, buffer_size, 0)) {
        result = 0;
    }

    cJSON_Delete(root);
    return result;
}

int validate_ack_json(const char *json, const char *expected_message_id)
{
    cJSON *root;
    cJSON *version;
    cJSON *type;
    cJSON *id;
    cJSON *status;
    int result = -1;

    if (json == NULL || expected_message_id == NULL) {
        return -1;
    }

    root = cJSON_Parse(json);
    if (root == NULL) {
        return -1;
    }

    version = cJSON_GetObjectItemCaseSensitive(root, "version");
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    id = cJSON_GetObjectItemCaseSensitive(root, "message_id");
    status = cJSON_GetObjectItemCaseSensitive(root, "status");

    if (cJSON_IsNumber(version) && version->valueint == 1 &&
        cJSON_IsString(type) && strcmp(type->valuestring, "ack") == 0 &&
        cJSON_IsString(id) && strcmp(id->valuestring, expected_message_id) == 0 &&
        cJSON_IsString(status) && strcmp(status->valuestring, "ok") == 0) {
        result = 0;
    }

    cJSON_Delete(root);
    return result;
}
