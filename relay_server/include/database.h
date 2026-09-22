#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>

#include "json.h"

#define DB_SAVE_ERROR -1
#define DB_SAVE_OK 0
#define DB_SAVE_DUPLICATE 1
#define DB_SAVE_CONFLICT 2

sqlite3 *database_open(const char *path);
void database_close(sqlite3 *database);
int database_save_sensor(sqlite3 *database, const SensorMessage *message);
int database_get_unsent_sensor(sqlite3 *database, SensorMessage *message);
int database_mark_sensor_sent(sqlite3 *database, const char *message_id);
int database_save_vision(sqlite3 *database, const VisionMessage *message);
int database_get_unsent_vision(sqlite3 *database, VisionMessage *message);
int database_mark_vision_sent(sqlite3 *database, const char *message_id);

#endif
