#ifndef CATALOG_H
#define CATALOG_H

#include <stdint.h>

#define MAX_COLUMNS 32
#define MAX_TABLES 128

typedef enum {
  TYPE_UINT16,
  TYPE_INT32,
  TYPE_INT64,
  TYPE_FLOAT64,
  TYPE_STRING
} DataType;

typedef struct {
  char name[32];
  DataType type;
  uint16_t offset;
  uint16_t length;
} ColumnMetadata;

typedef struct {
  char name[64];
  uint16_t column_count;
  ColumnMetadata columns[MAX_COLUMNS];
} TableMetadata;

typedef struct {
  uint16_t table_count;
  TableMetadata tables[MAX_TABLES];
} Catalogue;

// data/<db_name>/catalog.dat — one TableMetadata record per table,
// stored with the normal page/slot record format

// creates the database folder and an empty catalog.dat
int catalogue_create(const char *db_name);

// appends one table record, -1 when the table is already catalogued
int catalogue_put(const char *db_name, const TableMetadata *meta);

// copies the stored record of one table into out, -1 when not found
int catalogue_get(const char *db_name, const char *table_name,
                  TableMetadata *out);

// reads every record of catalog.dat (malloc, release with catalogue_free)
Catalogue *catalogue_load(const char *db_name);
void catalogue_free(Catalogue *catalogue);

#endif
