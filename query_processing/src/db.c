#include "database.h"
#include <string.h>

struct Database {
  int dummy;
};

Database *db_open(const char *path) {
  (void)path;

  static Database db;
  return &db;
}

int db_scan(Database *db, ScanRequest *request, Row *rows, size_t capacity,
            size_t *count) {
  (void)db;

  if (strcmp(request->table_name, "users") != 0) {
    return -1;
  }

  if (capacity < 3) {
    return -2;
  }

  static const char *data[] = {"Alice,20", "Bob,25", "Charlie,31"};

  for (int i = 0; i < 3; i++) {
    rows[i].data = data[i];
    rows[i].len = strlen(data[i]);
  }

  *count = 3;

  return 0;
}

void db_close(Database *db) { (void)db; }
