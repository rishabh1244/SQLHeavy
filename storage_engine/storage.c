#include "storage.h"

#include "page_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Database *new_db(const char *name) {
  Database *db = calloc(1, sizeof(Database));
  if (db == NULL) {
    return NULL;
  }

  snprintf(db->name, sizeof(db->name), "%s", name ? name : "");
  return db;
}

Table *new_table(Database *db, const char *name) {
  if (db == NULL || db->table_count >= MAX_TABLES) {
    return NULL;
  }

  Table *table = &db->tables[db->table_count];
  snprintf(table->name, sizeof(table->name), "%s", name ? name : "");
  db->table_count++;
  return table;
}

Table *find_table(Database *db, const char *name) {
  if (db == NULL || name == NULL) {
    return NULL;
  }

  for (int i = 0; i < db->table_count; i++) {
    if (strcmp(db->tables[i].name, name) == 0) {
      return &db->tables[i];
    }
  }
  return NULL;
}

// points page_manager at data/<db>/<table>.dat, then appends the record
int db_insert_record(Database *db, const char *table_name, const void *data,
                     uint16_t length) {
  if (find_table(db, table_name) == NULL) {
    return -1;
  }

  set_target(db->name, table_name);
  insert_record(data, length);
  return 0;
}
