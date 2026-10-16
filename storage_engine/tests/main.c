#include "catalog.h"
#include "storage.h"

#include <stdio.h>

ColumnMetadata cols[] = {
    {"name", TYPE_STRING, 0, 32},
    {"age", TYPE_UINT16, 32, 2},
    {"salary", TYPE_INT64, 34, 8},
};

int main(void) {
  Database *db = new_db("TEST_DB");
  if (db == NULL) {
    printf("failed to open database TEST_DB\n");
    return 1;
  }

  new_table(db, "users", cols, sizeof(cols) / sizeof(cols[0]));

  close_db(db);
  return 0;
}
