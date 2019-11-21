#include "sqlite3.h"
#include "time.h"

static int callback(void *data, int argc, char **argv, char **azColName);
int open_db(const char *filename, sqlite3 **db);
int db_exec(sqlite3 *db, const char *sql);
int create_table();
int insert_db(time_t time);
void show_logs();