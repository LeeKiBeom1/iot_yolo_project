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

int parse_vision_json(const char *json, VisionMessage *message)
{
    cJSON *root;
    cJSON *version;
    cJSON *type;
    cJSON *data;
    cJSON *bbox;
    cJSON *frame_id;
    cJSON *timestamp_ms;
    cJSON *class_id;
    cJSON *confidence;
    cJSON *x;
    cJSON *y;
    cJSON *width;
    cJSON *height;
    int result = -1;

    if (json == NULL || message == NULL) return -1;
    memset(message, 0, sizeof(*message));

    root = cJSON_Parse(json);
    if (root == NULL) return -1;

    version = cJSON_GetObjectItemCaseSensitive(root, "version");
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    bbox = cJSON_IsObject(data)
        ? cJSON_GetObjectItemCaseSensitive(data, "bbox") : NULL;

    if (!cJSON_IsNumber(version) || version->valueint != 1 ||
        !cJSON_IsString(type) || strcmp(type->valuestring, "vision") != 0 ||
        !cJSON_IsObject(data) || !cJSON_IsObject(bbox) ||
        copy_json_string(root, "device_id", message->device_id,
                         sizeof(message->device_id)) != 0 ||
        copy_json_string(root, "message_id", message->message_id,
                         sizeof(message->message_id)) != 0 ||
        copy_json_string(data, "class_name", message->class_name,
                         sizeof(message->class_name)) != 0) {
        goto done;
    }

    frame_id = cJSON_GetObjectItemCaseSensitive(data, "frame_id");
    timestamp_ms = cJSON_GetObjectItemCaseSensitive(data, "timestamp_ms");
    class_id = cJSON_GetObjectItemCaseSensitive(data, "class_id");
    confidence = cJSON_GetObjectItemCaseSensitive(data, "confidence");
    x = cJSON_GetObjectItemCaseSensitive(bbox, "x");
    y = cJSON_GetObjectItemCaseSensitive(bbox, "y");
    width = cJSON_GetObjectItemCaseSensitive(bbox, "width");
    height = cJSON_GetObjectItemCaseSensitive(bbox, "height");

    if (!cJSON_IsNumber(frame_id) || frame_id->valuedouble < 0 ||
        !cJSON_IsNumber(timestamp_ms) || timestamp_ms->valuedouble < 0 ||
        !cJSON_IsNumber(class_id) ||
        !cJSON_IsNumber(confidence) || confidence->valuedouble < 0.0 ||
        confidence->valuedouble > 1.0 ||
        !cJSON_IsNumber(x) || x->valueint < 0 ||
        !cJSON_IsNumber(y) || y->valueint < 0 ||
        !cJSON_IsNumber(width) || width->valueint <= 0 ||
        !cJSON_IsNumber(height) || height->valueint <= 0) {
        goto done;
    }

    message->frame_id = (uint64_t)frame_id->valuedouble;
    message->timestamp_ms = (int64_t)timestamp_ms->valuedouble;
    message->class_id = class_id->valueint;
    message->confidence = (float)confidence->valuedouble;
    message->x = x->valueint;
    message->y = y->valueint;
    message->width = width->valueint;
    message->height = height->valueint;
    result = 0;

done:
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
        buffer == NULL || buffer_size <= 1) {
        return -1;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }

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
