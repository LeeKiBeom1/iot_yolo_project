#include "database.h"

#include <stdio.h>
#include <string.h>

sqlite3 *database_open(const char *path)
{
    sqlite3 *database = NULL;

    if (sqlite3_open(path, &database) != SQLITE_OK) {
        fprintf(stderr, "SQLite open failed: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return NULL;
    }
    return database;
}

void database_close(sqlite3 *database)
{
    if (database != NULL) sqlite3_close(database);
}

int database_save_sensor(sqlite3 *database, const SensorMessage *message)
{
    static const char sql[] =
        "INSERT INTO sensor_data "
        "(device_id,message_id,timestamp,light,temperature,humidity,sound) "
        "VALUES(?,?,datetime('now','localtime'),?,?,?,?)";
    sqlite3_stmt *statement;
    int step_result;

    if (database == NULL || message == NULL) return DB_SAVE_ERROR;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(database));
        return DB_SAVE_ERROR;
    }

    sqlite3_bind_text(statement, 1, message->device_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, message->message_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, message->light);
    sqlite3_bind_double(statement, 4, message->temperature);
    sqlite3_bind_double(statement, 5, message->humidity);
    sqlite3_bind_int(statement, 6, message->sound);
    step_result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (step_result == SQLITE_DONE) return DB_SAVE_OK;
    if (step_result == SQLITE_CONSTRAINT) return DB_SAVE_DUPLICATE;
    fprintf(stderr, "SQLite insert failed: %s\n", sqlite3_errmsg(database));
    return DB_SAVE_ERROR;
}

int database_get_unsent_sensor(sqlite3 *database, SensorMessage *message)
{
    static const char sql[] =
        "SELECT device_id,message_id,timestamp,light,temperature,humidity,sound "
        "FROM sensor_data WHERE sync_status='UNSENT' ORDER BY id LIMIT 1";
    sqlite3_stmt *statement;
    int step_result;

    if (database == NULL || message == NULL) return -1;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(database));
        return -1;
    }

    step_result = sqlite3_step(statement);
    if (step_result == SQLITE_ROW) {
        memset(message, 0, sizeof(*message));
        snprintf(message->device_id, sizeof(message->device_id), "%s",
                 (const char *)sqlite3_column_text(statement, 0));
        snprintf(message->message_id, sizeof(message->message_id), "%s",
                 (const char *)sqlite3_column_text(statement, 1));
        snprintf(message->timestamp, sizeof(message->timestamp), "%s",
                 (const char *)sqlite3_column_text(statement, 2));
        if (strlen(message->timestamp) == 19) message->timestamp[10] = 'T';
        message->light = sqlite3_column_int(statement, 3);
        message->temperature = (float)sqlite3_column_double(statement, 4);
        message->humidity = (float)sqlite3_column_double(statement, 5);
        message->sound = sqlite3_column_int(statement, 6);
        sqlite3_finalize(statement);
        return 1;
    }

    sqlite3_finalize(statement);
    return step_result == SQLITE_DONE ? 0 : -1;
}

int database_mark_sensor_sent(sqlite3 *database, const char *message_id)
{
    static const char sql[] =
        "UPDATE sensor_data SET sync_status='SENT' WHERE message_id=?";
    sqlite3_stmt *statement;
    int result;

    if (database == NULL || message_id == NULL) return -1;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(statement, 1, message_id, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(statement);
    return result;
}
