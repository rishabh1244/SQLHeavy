#include "page_manager.h"
#include "scan.h"
#include "storage.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  uint16_t age;
  char name[23];
} record;

typedef struct {
  int32_t id;
  char name[16];
} product;

static void print_catalogue(const Catalogue *cat) {
  printf("catalogue: %u tables\n", cat->table_count);
  for (uint16_t i = 0; i < cat->table_count; i++) {
    const TableMetadata *t = &cat->tables[i];
    printf("  %s (%u cols)", t->name, t->column_count);
    for (uint16_t c = 0; c < t->column_count; c++) {
      printf(" | %s type=%u off=%u len=%u", t->columns[c].name,
             (unsigned)t->columns[c].type, t->columns[c].offset,
             t->columns[c].length);
    }
    printf("\n");
  }
}

// connects to a separate table, inserts records, fetches them + the catalogue
static int run_products(Database *db) {
  ColumnMetadata columns[] = {
      {"id", TYPE_INT32, 0, sizeof(int32_t)},
      {"name", TYPE_STRING, 4, sizeof(((product *)0)->name)},
  };

  TableMetadata *products = new_table(db, "PRODUCTS", columns, 2);
  if (products == NULL) {
    products = find_table(db, "PRODUCTS"); // already in catalog.dat
  }
  if (products == NULL) {
    printf("no PRODUCTS table\n");
    return -1;
  }

  printf("table %s (%u columns)\n", products->name, products->column_count);

  product p1 = {101, "Keyboard"};
  product p2 = {102, "Mouse"};
  product p3 = {103, "Monitor"};

  if (db_insert_record(db, "PRODUCTS", &p1, sizeof(p1)) != 0 ||
      db_insert_record(db, "PRODUCTS", &p2, sizeof(p2)) != 0 ||
      db_insert_record(db, "PRODUCTS", &p3, sizeof(p3)) != 0) {
    printf("insert into PRODUCTS failed\n");
    return -1;
  }

  ScanBatch batch;
  if (scan_table(db->name, "PRODUCTS", &batch) != 0) {
    printf("fetch from PRODUCTS failed\n");
    return -1;
  }

  printf("PRODUCTS: %u rows\n", batch.count);
  for (uint32_t i = 0; i < batch.count; i++) {
    product p;
    memset(&p, 0, sizeof(p));
    memcpy(&p, batch.records[i].data,
           batch.records[i].len < sizeof(p) ? batch.records[i].len
                                            : sizeof(p));
    printf("  [%u] id=%-4d name=%s\n", i, p.id, p.name);
  }
  scan_batch_free(&batch);

  print_catalogue(db->catalogue);
  return 0;
}

int main(void) {
  Database *db = new_db("TEST_DB");
  if (db == NULL) {
    printf("failed to create database\n");
    return 1;
  }

  ColumnMetadata columns[] = {
      {"age", TYPE_UINT16, (uint16_t)offsetof(record, age), sizeof(uint16_t)},
      {"name", TYPE_STRING, (uint16_t)offsetof(record, name),
       sizeof(((record *)0)->name)},
  };

  TableMetadata *people = new_table(db, "PEOPLE", columns, 2);
  if (people == NULL) {
    people = find_table(db, "PEOPLE"); // already in catalog.dat
  }
  if (people == NULL) {
    printf("no PEOPLE table\n");
    close_db(db);
    return 1;
  }

  printf("table %s (%u columns)\n", people->name, people->column_count);
  for (uint16_t i = 0; i < people->column_count; i++) {
    printf("  %-6s type=%u offset=%u length=%u\n", people->columns[i].name,
           (unsigned)people->columns[i].type, people->columns[i].offset,
           people->columns[i].length);
  }

  record r1 = {12, "Sizuka"};
  record r2 = {25, "Nobita"};
  record r3 = {31, "Gian"};

  db_insert_record(db, "PEOPLE", &r1, sizeof(r1));
  db_insert_record(db, "PEOPLE", &r2, sizeof(r2));
  db_insert_record(db, "PEOPLE", &r3, sizeof(r3));

  set_target(db->name, "PEOPLE");

  PageId page_id = fetch_latest();
  Page page = read_page(page_id);

  printf("latest page id : %u\n", (unsigned)page_id);
  printf("page_id         : %u\n", (unsigned)page.header.page_id);
  printf("num_slots       : %u\n", (unsigned)page.header.num_slots);
  printf("free_space_start: %u\n", (unsigned)page.header.free_space_start);
  printf("free_space_end  : %u\n", (unsigned)page.header.free_space_end);

  printf("records on page %u:\n", (unsigned)page_id);

  for (uint16_t i = 0; i < page.header.num_slots; i++) {
    record r = {};
    uint16_t len = fetch_record(&page, i, &r, sizeof(r));

    if (len == 0) {
      printf("  [%u] <bad slot>\n", (unsigned)i);
      continue;
    }
    printf("  [%u] len=%-3u name=%-10s age=%u\n", (unsigned)i, (unsigned)len,
           r.name, (unsigned)r.age);
  }

  if (run_products(db) != 0) {
    close_db(db);
    return 1;
  }

  close_db(db);
  return 0;
}
