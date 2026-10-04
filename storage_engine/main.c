#include "page_manager.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  uint16_t age;
  char name[23];
} record;

int main(void) {
  record r1;
  r1.age = 12;
  strcpy(r1.name, "Sizuka");

  record r2;
  r2.age = 25;
  strcpy(r2.name, "Nobita");

  record r3;
  r3.age = 31;
  strcpy(r3.name, "Gian");

  insert_record(&r1, sizeof(r1));
  insert_record(&r2, sizeof(r2));
  insert_record(&r3, sizeof(r3));

  PageId page_id = fetch_latest();
  Page page = read_page(page_id);

  printf("latest page id : %u\n", (unsigned)page_id);
  printf("page_id         : %u\n", (unsigned)page.header.page_id);
  printf("num_slots       : %u\n", (unsigned)page.header.num_slots);
  printf("free_space_start: %u\n", (unsigned)page.header.free_space_start);
  printf("free_space_end  : %u\n", (unsigned)page.header.free_space_end);

  // fetch records like an array: fetch_record(&page, i, ...)
  printf("records on page %u:\n", (unsigned)page_id);

  // for (uint16_t i = 0; i < page.header.num_slots; i++) {
  int i = 2;
  record r = {};
  uint16_t len = fetch_record(&page, i, &r, sizeof(r));

  if (len == 0) {
    printf("  [%u] <bad slot>\n", (unsigned)i);
  }

  printf("  [%u] len=%-3u name=%-10s age=%u\n", (unsigned)i, (unsigned)len,
         r.name, (unsigned)r.age);
  //}

  return 0;
}
