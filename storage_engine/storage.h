#ifndef STORAGE
#define STORAGE

#include <stdint.h>

#define MAX_TABLES 20
#define NAME_LEN 20

typedef struct {
  char name[NAME_LEN];
} Table;

typedef struct {
  char name[NAME_LEN];
  Table tables[MAX_TABLES];
  int table_count;
} Database;

Database *new_db(const char *name);
Table *new_table(Database *db, const char *name);
Table *find_table(Database *db, const char *name);
int db_insert_record(Database *db, const char *table_name, const void *data,
                     uint16_t length);

#endif
