#include "database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MYSQL *database_connect(void)
{
    const char *host = getenv("DB_HOST");
    const char *user = getenv("DB_USER");
    const char *password = getenv("DB_PASSWORD");
    const char *name = getenv("DB_NAME");
    MYSQL *database = mysql_init(NULL);

    if (database == NULL) {
        return NULL;
    }

    if (host == NULL) host = "localhost";
    if (user == NULL) user = "ubuntu";
    if (password == NULL) password = "";
    if (name == NULL) name = "road_monitor";

    if (mysql_real_connect(database, host, user, password, name,
                           0, NULL, 0) == NULL) {
        fprintf(stderr, "MariaDB connection failed: %s\n",
                mysql_error(database));
        mysql_close(database);
        return NULL;
    }

    return database;
}

void database_close(MYSQL *database)
{
    if (database != NULL) {
        mysql_close(database);
    }
}

static int check_sensor_duplicate(MYSQL *database,
                                  const SensorMessage *message)
{
    static const char sql[] =
        "SELECT device_id,DATE_FORMAT(timestamp,'%Y-%m-%dT%H:%i:%s'),"
        "light,temperature,humidity,sound FROM sensor_data WHERE message_id=?";
    MYSQL_STMT *statement;
    MYSQL_BIND parameter[1];
    MYSQL_BIND result_bind[6];
    char device_id[33] = {0};
    char timestamp[20] = {0};
    unsigned long message_id_length;
    unsigned long device_id_length;
    unsigned long timestamp_length;
    int light;
    float temperature;
    float humidity;
    int sound;
    float temperature_difference;
    float humidity_difference;
    int result = DB_SAVE_ERROR;

    statement = mysql_stmt_init(database);
    if (statement == NULL ||
        mysql_stmt_prepare(statement, sql, (unsigned long)strlen(sql)) != 0) {
        if (statement != NULL) mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(parameter, 0, sizeof(parameter));
    message_id_length = (unsigned long)strlen(message->message_id);
    parameter[0].buffer_type = MYSQL_TYPE_STRING;
    parameter[0].buffer = (void *)message->message_id;
    parameter[0].buffer_length = message_id_length;
    parameter[0].length = &message_id_length;
    if (mysql_stmt_bind_param(statement, parameter) != 0 ||
        mysql_stmt_execute(statement) != 0) {
        mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(result_bind, 0, sizeof(result_bind));
    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = device_id;
    result_bind[0].buffer_length = sizeof(device_id);
    result_bind[0].length = &device_id_length;
    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = timestamp;
    result_bind[1].buffer_length = sizeof(timestamp);
    result_bind[1].length = &timestamp_length;
    result_bind[2].buffer_type = MYSQL_TYPE_LONG;
    result_bind[2].buffer = &light;
    result_bind[3].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[3].buffer = &temperature;
    result_bind[4].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[4].buffer = &humidity;
    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = &sound;

    if (mysql_stmt_bind_result(statement, result_bind) == 0 &&
        mysql_stmt_store_result(statement) == 0 &&
        mysql_stmt_fetch(statement) == 0) {
        temperature_difference = temperature - message->temperature;
        humidity_difference = humidity - message->humidity;
        if (temperature_difference < 0) temperature_difference *= -1;
        if (humidity_difference < 0) humidity_difference *= -1;

        if (strcmp(device_id, message->device_id) == 0 &&
            strcmp(timestamp, message->timestamp) == 0 &&
            light == message->light &&
            temperature_difference < 0.0001f &&
            humidity_difference < 0.0001f &&
            sound == message->sound) {
            result = DB_SAVE_DUPLICATE;
        } else {
            result = DB_SAVE_CONFLICT;
        }
    }

    mysql_stmt_close(statement);
    return result;
}

static int check_vision_duplicate(MYSQL *database,
                                  const VisionMessage *message)
{
    static const char sql[] =
        "SELECT device_id,frame_id,timestamp_ms,class_id,class_name,confidence,"
        "x,y,width,height FROM vision_data WHERE message_id=?";
    MYSQL_STMT *statement;
    MYSQL_BIND parameter[1];
    MYSQL_BIND result_bind[10];
    char device_id[33] = {0};
    char class_name[51] = {0};
    unsigned long message_id_length;
    unsigned long device_id_length;
    unsigned long class_name_length;
    unsigned long long frame_id;
    long long timestamp_ms;
    int class_id;
    float confidence;
    int x, y, width, height;
    float confidence_difference;
    int result = DB_SAVE_ERROR;

    statement = mysql_stmt_init(database);
    if (statement == NULL ||
        mysql_stmt_prepare(statement, sql, (unsigned long)strlen(sql)) != 0) {
        if (statement != NULL) mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(parameter, 0, sizeof(parameter));
    message_id_length = (unsigned long)strlen(message->message_id);
    parameter[0].buffer_type = MYSQL_TYPE_STRING;
    parameter[0].buffer = (void *)message->message_id;
    parameter[0].buffer_length = message_id_length;
    parameter[0].length = &message_id_length;
    if (mysql_stmt_bind_param(statement, parameter) != 0 ||
        mysql_stmt_execute(statement) != 0) {
        mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(result_bind, 0, sizeof(result_bind));
    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = device_id;
    result_bind[0].buffer_length = sizeof(device_id);
    result_bind[0].length = &device_id_length;
    result_bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bind[1].buffer = &frame_id;
    result_bind[1].is_unsigned = 1;
    result_bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bind[2].buffer = &timestamp_ms;
    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &class_id;
    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = class_name;
    result_bind[4].buffer_length = sizeof(class_name);
    result_bind[4].length = &class_name_length;
    result_bind[5].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[5].buffer = &confidence;
    result_bind[6].buffer_type = MYSQL_TYPE_LONG;
    result_bind[6].buffer = &x;
    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = &y;
    result_bind[8].buffer_type = MYSQL_TYPE_LONG;
    result_bind[8].buffer = &width;
    result_bind[9].buffer_type = MYSQL_TYPE_LONG;
    result_bind[9].buffer = &height;

    if (mysql_stmt_bind_result(statement, result_bind) == 0 &&
        mysql_stmt_store_result(statement) == 0 &&
        mysql_stmt_fetch(statement) == 0) {
        confidence_difference = confidence - message->confidence;
        if (confidence_difference < 0) confidence_difference *= -1;

        if (strcmp(device_id, message->device_id) == 0 &&
            frame_id == message->frame_id &&
            timestamp_ms == message->timestamp_ms &&
            class_id == message->class_id &&
            strcmp(class_name, message->class_name) == 0 &&
            confidence_difference < 0.0001f &&
            x == message->x && y == message->y &&
            width == message->width && height == message->height) {
            result = DB_SAVE_DUPLICATE;
        } else {
            result = DB_SAVE_CONFLICT;
        }
    }

    mysql_stmt_close(statement);
    return result;
}

int database_save_sensor(MYSQL *database, const SensorMessage *message)
{
    static const char sql[] =
        "INSERT INTO sensor_data "
        "(device_id, message_id, timestamp, light, temperature, humidity, sound) "
        "VALUES (?, ?, STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%s'), ?, ?, ?, ?)";
    MYSQL_STMT *statement;
    MYSQL_BIND bind[7];
    unsigned long device_id_length;
    unsigned long message_id_length;
    unsigned long timestamp_length;
    int result = DB_SAVE_ERROR;

    if (database == NULL || message == NULL) return DB_SAVE_ERROR;

    statement = mysql_stmt_init(database);
    if (statement == NULL ||
        mysql_stmt_prepare(statement, sql, (unsigned long)strlen(sql)) != 0) {
        fprintf(stderr, "MariaDB prepare failed: %s\n", mysql_error(database));
        if (statement != NULL) mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(bind, 0, sizeof(bind));
    device_id_length = (unsigned long)strlen(message->device_id);
    message_id_length = (unsigned long)strlen(message->message_id);
    timestamp_length = (unsigned long)strlen(message->timestamp);

#define BIND_STRING(index, value, length_value) \
    bind[index].buffer_type = MYSQL_TYPE_STRING; \
    bind[index].buffer = (void *)(value); \
    bind[index].buffer_length = (length_value); \
    bind[index].length = &(length_value)
#define BIND_VALUE(index, type, value) \
    bind[index].buffer_type = (type); \
    bind[index].buffer = (void *)&(value)

    BIND_STRING(0, message->device_id, device_id_length);
    BIND_STRING(1, message->message_id, message_id_length);
    BIND_STRING(2, message->timestamp, timestamp_length);
    BIND_VALUE(3, MYSQL_TYPE_LONG, message->light);
    BIND_VALUE(4, MYSQL_TYPE_FLOAT, message->temperature);
    BIND_VALUE(5, MYSQL_TYPE_FLOAT, message->humidity);
    BIND_VALUE(6, MYSQL_TYPE_LONG, message->sound);

#undef BIND_STRING
#undef BIND_VALUE

    if (mysql_stmt_bind_param(statement, bind) != 0) {
        fprintf(stderr, "MariaDB bind failed: %s\n", mysql_stmt_error(statement));
    } else if (mysql_stmt_execute(statement) == 0) {
        result = DB_SAVE_OK;
    } else if (mysql_stmt_errno(statement) == 1062) {
        result = check_sensor_duplicate(database, message);
    } else {
        fprintf(stderr, "MariaDB insert failed: %s\n", mysql_stmt_error(statement));
    }

    mysql_stmt_close(statement);
    return result;
}

int database_save_vision(MYSQL *database, const VisionMessage *message)
{
    static const char sql[] =
        "INSERT INTO vision_data "
        "(device_id, message_id, timestamp, frame_id, timestamp_ms, "
        "class_id, class_name, confidence, x, y, width, height) "
        "VALUES (?, ?, STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%s'), "
        "?, ?, ?, ?, ?, ?, ?, ?, ?)";
    MYSQL_STMT *statement;
    MYSQL_BIND bind[12];
    unsigned long device_id_length;
    unsigned long message_id_length;
    unsigned long timestamp_length;
    unsigned long class_name_length;
    unsigned long long frame_id;
    long long timestamp_ms;
    int result = DB_SAVE_ERROR;

    if (database == NULL || message == NULL) {
        return DB_SAVE_ERROR;
    }

    statement = mysql_stmt_init(database);
    if (statement == NULL ||
        mysql_stmt_prepare(statement, sql, (unsigned long)strlen(sql)) != 0) {
        fprintf(stderr, "MariaDB prepare failed: %s\n", mysql_error(database));
        if (statement != NULL) mysql_stmt_close(statement);
        return DB_SAVE_ERROR;
    }

    memset(bind, 0, sizeof(bind));
    device_id_length = (unsigned long)strlen(message->device_id);
    message_id_length = (unsigned long)strlen(message->message_id);
    timestamp_length = (unsigned long)strlen(message->timestamp);
    class_name_length = (unsigned long)strlen(message->class_name);
    frame_id = message->frame_id;
    timestamp_ms = message->timestamp_ms;

#define BIND_STRING(index, value, length_value) \
    bind[index].buffer_type = MYSQL_TYPE_STRING; \
    bind[index].buffer = (void *)(value); \
    bind[index].buffer_length = (length_value); \
    bind[index].length = &(length_value)
#define BIND_VALUE(index, type, value) \
    bind[index].buffer_type = (type); \
    bind[index].buffer = (void *)&(value)

    BIND_STRING(0, message->device_id, device_id_length);
    BIND_STRING(1, message->message_id, message_id_length);
    BIND_STRING(2, message->timestamp, timestamp_length);
    BIND_VALUE(3, MYSQL_TYPE_LONGLONG, frame_id);
    bind[3].is_unsigned = 1;
    BIND_VALUE(4, MYSQL_TYPE_LONGLONG, timestamp_ms);
    BIND_VALUE(5, MYSQL_TYPE_LONG, message->class_id);
    BIND_STRING(6, message->class_name, class_name_length);
    BIND_VALUE(7, MYSQL_TYPE_FLOAT, message->confidence);
    BIND_VALUE(8, MYSQL_TYPE_LONG, message->x);
    BIND_VALUE(9, MYSQL_TYPE_LONG, message->y);
    BIND_VALUE(10, MYSQL_TYPE_LONG, message->width);
    BIND_VALUE(11, MYSQL_TYPE_LONG, message->height);

#undef BIND_STRING
#undef BIND_VALUE

    if (mysql_stmt_bind_param(statement, bind) != 0) {
        fprintf(stderr, "MariaDB bind failed: %s\n", mysql_stmt_error(statement));
    } else if (mysql_stmt_execute(statement) == 0) {
        result = DB_SAVE_OK;
    } else if (mysql_stmt_errno(statement) == 1062) {
        result = check_vision_duplicate(database, message);
    } else {
        fprintf(stderr, "MariaDB insert failed: %s\n", mysql_stmt_error(statement));
    }

    mysql_stmt_close(statement);
    return result;
}
