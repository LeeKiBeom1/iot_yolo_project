#ifndef DATABASE_H
#define DATABASE_H

#include <mysql.h>

#include "json.h"

#define DB_SAVE_ERROR -1
#define DB_SAVE_OK 0
#define DB_SAVE_DUPLICATE 1

MYSQL *database_connect(void);
void database_close(MYSQL *database);
int database_save_sensor(MYSQL *database, const SensorMessage *message);
int database_save_vision(MYSQL *database, const VisionMessage *message);

#endif
