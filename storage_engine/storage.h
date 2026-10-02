#ifndef STORAGE
#define STORAGE

typedef struct {
  char name;
  char col;
} Table;

typedef struct {
  char name[12];
  Table tables[20];
} Database;

#endif
