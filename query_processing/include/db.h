
// database.h

typedef struct Database Database;

typedef struct {
  const char *table_name;
} ScanRequest;

typedef struct {
  const char *data;
  size_t len;
} Row;

Database *db_open(const char *path);

int db_scan(Database *db, ScanRequest *request, Row *rows, size_t capacity,
            size_t *count);

void db_close(Database *db);
