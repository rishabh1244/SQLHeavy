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

  // records are packed back-to-back from page.data[0] up to free_space_start;
  // every record on this page is a `record`, so walk them with a fixed stride
  printf("records on page %u:\n", (unsigned)page_id);

  size_t offset = 0;
  size_t data_end = page.header.free_space_start - sizeof(PageHeader);

  for (uint16_t i = 0; i < page.header.num_slots; i++) {
    if (offset + sizeof(record) > data_end) {
      printf("  [%u] <corrupt: runs past free_space_start>\n", (unsigned)i);
      break;
    }

    record r = {};
    memcpy(&r, page.data + offset, sizeof(r));
    offset += sizeof(r);

    printf("  [%u] name=%-10s age=%u\n", (unsigned)i, r.name,
           (unsigned)r.age);
  }

  return 0;
}
