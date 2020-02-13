/*
    This example opens Sqlite3 databases from SD Card and
    retrieves data from them.
    Before running please copy following files to SD Card:
    examples/sqlite3_sdmmc/data/mdr512.db
    examples/sqlite3_sdmmc/data/census2000names.db
    Connections:
     * SD Card | ESP32
     *  DAT2       -
     *  DAT3       SS (D5)
     *  CMD        MOSI (D23)
     *  VSS        GND
     *  VDD        3.3V
     *  CLK        SCK (D18)
     *  DAT0       MISO (D19)
     *  DAT1       -
*/

#include "sqlite.h"
#include <M5Stack.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
const char *FILE_NAME_DB = "/sd/test_with_user_name_name.db";

const char *data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName) {
  int i;
  M5.Lcd.printf("%s: ", (const char *)data);
  for (i = 0; i < argc; i++) {
    M5.Lcd.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  M5.Lcd.printf("\n");
  return 0;
}

int open_db(const char *filename, sqlite3 **db) {
  int rc = sqlite3_open(filename, db);
  if (rc) {
    M5.Lcd.printf("Can't open database: %s (>_<)\n", sqlite3_errmsg(*db));
    return rc;
  } else {
    M5.Lcd.printf("Opened database successfully (^_^)\n");
  }
  return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
  M5.Lcd.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    M5.Lcd.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    M5.Lcd.printf("Operation done successfully (^_^)\n");
    M5.Lcd.printf(zErrMsg);
  }
  M5.Lcd.print(F("Time taken:"));
  M5.Lcd.println(micros() - start);
  return rc;
}

int create_table() {
  sqlite3 *db_sd;
  open_db(FILE_NAME_DB, &db_sd);
  char *msg_err = NULL;
  char *querry =
      "CREATE TABLE IF NOT EXISTS LOGS"
      "(id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_name TEXT,"
      "seq INTEGER, "
      "time datetime NOT NULL);";
  int err = sqlite3_exec(db_sd, querry, NULL, NULL, &msg_err);
  if (err != SQLITE_OK) {
    M5.Lcd.printf("f%s >_<\n", msg_err);

    return err;
  }
  M5.Lcd.printf("create table success (^_^)\n");
  sqlite3_close(db_sd);
  return err;
}

int insert_db(std::string user_name, time_t time, int seq) {
  sqlite3_stmt *statement = NULL;

  char *msg_error = NULL;
  char *querry =
      "INSERT INTO LOGS(user_name,seq,time) VALUES(:user_name, :seq, :time);";
  int error = 0;
  sqlite3 *db_sd;
  open_db(FILE_NAME_DB, &db_sd);

  sqlite3_prepare_v2(db_sd, querry, 128, &statement, NULL);
  if (error != SQLITE_OK) {
    M5.Lcd.printf("faild insert. >_<\n");
    return error;
  }
  sqlite3_bind_text(statement,
                    sqlite3_bind_parameter_index(statement, ":user_name"),
                    user_name.c_str(), user_name.length(), SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement,
                     sqlite3_bind_parameter_index(statement, ":time"), time);
  sqlite3_bind_int64(statement, sqlite3_bind_parameter_index(statement, ":seq"),
                     seq);
  while (SQLITE_DONE != sqlite3_step(statement)) {
  }
  sqlite3_finalize(statement);
  M5.Lcd.printf("insert done!");
  sqlite3_close(db_sd);

  return error;
}

int drop_table() {
  sqlite3 *db_sd;
  open_db(FILE_NAME_DB, &db_sd);
  char *msg_err = NULL;
  char *querry = "DROP TABLE LOGS;";
  int err = sqlite3_exec(db_sd, querry, NULL, NULL, &msg_err);
  if (err != SQLITE_OK) {
    M5.Lcd.printf("f%s >_<\n", msg_err);

    return err;
  }
  M5.Lcd.printf("drop table success (^_^)\n");
  sqlite3_close(db_sd);
  return err;
}

void show_logs() {
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  sqlite3_stmt *statement = NULL;

  char *msg_error = NULL;
  int error = 0;
  sqlite3 *db_sd;
  const char *querry =
      "SELECT id,user_name,seq,time "
      "FROM LOGS order by id DESC limit 10;";
  open_db(FILE_NAME_DB, &db_sd);
  error = sqlite3_prepare_v2(db_sd, querry, 64, &statement, NULL);

  if (error != SQLITE_OK) {
    M5.Lcd.printf("select failed (>_<)\n");
  }

  while (SQLITE_ROW == (error = sqlite3_step(statement))) {
    int id = sqlite3_column_int(statement, 0);
    std::string user_name = (const char *)sqlite3_column_text(statement, 1);
    int seq = sqlite3_column_int(statement, 2);
    time_t time = sqlite3_column_int(statement, 3);
    struct tm *show_tm = localtime(&time);
    char str_time[128] = {0};
    strftime(str_time, sizeof(str_time), "%Y/%m/%d %H:%M:%S", show_tm);
    M5.Lcd.printf("%s | %s\n", user_name.c_str(), str_time);
  }

  if (error != SQLITE_DONE) {
    M5.Lcd.printf("select not done (>_<)\n");
  }
  sqlite3_close(db_sd);
}
