#include "page_manager.h"
#include "storage.h"

#include <stddef.h>
#include <stdio.h>

typedef struct {
  uint16_t age;
  char name[23];
} record;

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

  close_db(db);
  return 0;
}
