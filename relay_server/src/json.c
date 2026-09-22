#include "json.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

static int copy_json_string(cJSON *object, const char *name,
                            char *destination, int destination_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

    if (!cJSON_IsString(item) || item->valuestring[0] == '\0' ||
        (int)strlen(item->valuestring) >= destination_size) {
        return -1;
    }
    snprintf(destination, destination_size, "%s", item->valuestring);
    return 0;
}

int parse_sensor_json(const char *json, SensorMessage *message)
{
    cJSON *root;
    cJSON *version;
    cJSON *type;
    cJSON *data;
    cJSON *light;
    cJSON *temperature;
    cJSON *humidity;
    cJSON *sound;
    int result = -1;

    if (json == NULL || message == NULL) return -1;
    memset(message, 0, sizeof(*message));
    root = cJSON_Parse(json);
    if (root == NULL) return -1;

    version = cJSON_GetObjectItemCaseSensitive(root, "version");
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsNumber(version) || version->valueint != 1 ||
        !cJSON_IsString(type) || strcmp(type->valuestring, "sensor") != 0 ||
        !cJSON_IsObject(data) ||
        copy_json_string(root, "device_id", message->device_id,
                         sizeof(message->device_id)) != 0 ||
        copy_json_string(root, "message_id", message->message_id,
                         sizeof(message->message_id)) != 0) {
        goto done;
    }

    light = cJSON_GetObjectItemCaseSensitive(data, "light");
    temperature = cJSON_GetObjectItemCaseSensitive(data, "temperature");
    humidity = cJSON_GetObjectItemCaseSensitive(data, "humidity");
    sound = cJSON_GetObjectItemCaseSensitive(data, "sound");
    if (!cJSON_IsNumber(light) || !cJSON_IsNumber(temperature) ||
        !cJSON_IsNumber(humidity) || !cJSON_IsNumber(sound)) {
        goto done;
    }

    message->light = light->valueint;
    message->temperature = (float)temperature->valuedouble;
    message->humidity = (float)humidity->valuedouble;
    message->sound = sound->valueint;
    result = 0;

done:
    cJSON_Delete(root);
    return result;
}

int create_sensor_json(const SensorMessage *message,
                       char *buffer, int buffer_size)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    int result = -1;

    if (message == NULL || buffer == NULL || buffer_size <= 1) return -1;
    root = cJSON_CreateObject();
    data = cJSON_CreateObject();
    if (root == NULL || data == NULL) goto done;

    if (cJSON_AddNumberToObject(root, "version", 1) != NULL &&
        cJSON_AddStringToObject(root, "type", "sensor") != NULL &&
        cJSON_AddStringToObject(root, "device_id", message->device_id) != NULL &&
        cJSON_AddStringToObject(root, "message_id", message->message_id) != NULL &&
        cJSON_AddStringToObject(root, "timestamp", message->timestamp) != NULL &&
        cJSON_AddNumberToObject(data, "light", message->light) != NULL &&
        cJSON_AddNumberToObject(data, "temperature", message->temperature) != NULL &&
        cJSON_AddNumberToObject(data, "humidity", message->humidity) != NULL &&
        cJSON_AddNumberToObject(data, "sound", message->sound) != NULL) {
        cJSON_AddItemToObject(root, "data", data);
        data = NULL;
        if (cJSON_PrintPreallocated(root, buffer, buffer_size, 0)) result = 0;
    }

done:
    cJSON_Delete(data);
    cJSON_Delete(root);
    return result;
}

int create_ack_json(const char *message_id, const char *status,
                    int duplicate, const char *error_code,
                    char *buffer, int buffer_size)
{
    cJSON *root;
    int result = -1;

    if (message_id == NULL || status == NULL ||
        buffer == NULL || buffer_size <= 1) return -1;
    root = cJSON_CreateObject();
    if (root == NULL) return -1;

    if (cJSON_AddNumberToObject(root, "version", 1) != NULL &&
        cJSON_AddStringToObject(root, "type", "ack") != NULL &&
        cJSON_AddStringToObject(root, "message_id", message_id) != NULL &&
        cJSON_AddStringToObject(root, "status", status) != NULL &&
        (!duplicate || cJSON_AddBoolToObject(root, "duplicate", 1) != NULL) &&
        (error_code == NULL ||
         cJSON_AddStringToObject(root, "error_code", error_code) != NULL) &&
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

    if (json == NULL || expected_message_id == NULL) return -1;
    root = cJSON_Parse(json);
    if (root == NULL) return -1;

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
