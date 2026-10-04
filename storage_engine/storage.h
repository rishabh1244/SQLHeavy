#ifndef STORAGE
#define STORAGE

#include <string.h>
typedef struct {
  char name[20];
  char col[];
} Table;

typedef struct {
  char name[20];
  Table tables[20];
} Database;

Database *new_db(char name[20]);

#endif
