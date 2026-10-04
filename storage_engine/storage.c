#include "storage.h"

Database *new_db(char name[20]) {
  Database *db;
  strcpy(db->name, name);
  return db;
}
