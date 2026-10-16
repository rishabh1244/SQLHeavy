#include "catalog.h"
#include "scan.h"
#include "storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

ColumnMetadata cols[] = {
    {"name", TYPE_STRING, 0, 20},
    {"age", TYPE_INT32, 20, 4},
    {"salary", TYPE_INT32, 24, 4},
};

typedef struct {
  char name[20];
  int age;
  int salary;
} user_data;

user_data users[] = {
    {"Rahul", 16, 300},
    {"Rohan", 26, 9300},
    {"Raj", 36, 4000},
};

int main(void) {
  Database *db = new_db("TEST_DB");
  if (db == NULL) {
    printf("failed to open database TEST_DB\n");
    return 1;
  }

  new_table(db, "users", cols, sizeof(cols) / sizeof(cols[0]));

  for (size_t i = 0; i < sizeof(users) / sizeof(users[0]); i++) {
    db_insert_record(db, "users", &users[i], sizeof(user_data));
  }
  ScanBatch batch;
  scan_table("TEST_DB", "users", &batch);

  close_db(db);
  return 0;
}
