#include "sqlite3.h"
#include "time.h"

int open_db(const char *filename, sqlite3 **db);
int db_exec(sqlite3 *db, const char *sql);
int create_table();
int insert_db(time_t time, int seq);
void show_logs();