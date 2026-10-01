#ifndef STORAGE
#define STORAGE

typedef struct {
  char name[12];
  Table tables[20];
} Database;

typedef struct {
  char name;
  char col;
} Table;

#endif
