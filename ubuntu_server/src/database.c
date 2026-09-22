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
        result = DB_SAVE_DUPLICATE;
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
        "VALUES (?, ?, FROM_UNIXTIME(? / 1000.0), ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    MYSQL_STMT *statement;
    MYSQL_BIND bind[12];
    unsigned long device_id_length;
    unsigned long message_id_length;
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
    BIND_VALUE(2, MYSQL_TYPE_LONGLONG, timestamp_ms);
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
        result = DB_SAVE_DUPLICATE;
    } else {
        fprintf(stderr, "MariaDB insert failed: %s\n", mysql_stmt_error(statement));
    }

    mysql_stmt_close(statement);
    return result;
}
