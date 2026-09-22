#ifndef SYNC_H
#define SYNC_H

#include <sqlite3.h>

int sync_unsent_sensors(sqlite3 *database,
                        const char *server_ip, int server_port);

#endif
