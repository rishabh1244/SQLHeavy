#include "page_manager.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  uint16_t age;
  char name[23];
} record;

int main(void) {
  int value = 112;

  record r1;
  r1.age = 12;
  strcpy(r1.name, "Sizuka2");

  // insert_record(&value, sizeof(value));
  // insert_record(&r1, sizeof(r1));

  PageId latest = fetch_latest();
  Page page = read_page(latest);

  printf("latest page id : %u\n", (unsigned)latest);
  printf("page_id         : %u\n", (unsigned)page.header.page_id);
  printf("num_slots       : %u\n", (unsigned)page.header.num_slots);
  printf("free_space_start: %u\n", (unsigned)page.header.free_space_start);
  printf("free_space_end  : %u\n", (unsigned)page.header.free_space_end);

  // records are packed back-to-back starting at page.data[0]
  // (= (uint8_t *)&page + sizeof(PageHeader)); walk them by size
  size_t offset = 3;

  int out_value = 0;
  memcpy(&out_value, page.data + offset, sizeof(out_value));
  offset += sizeof(out_value);

  record out_record = {};
  memcpy(&out_record, page.data + offset, sizeof(out_record));

  printf("value: %d\n", out_value);
  printf("name : %s\n", out_record.name);
  printf("age  : %u\n", (unsigned)out_record.age);

  return 0;
}
