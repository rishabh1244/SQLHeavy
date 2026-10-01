/*
  Purpose
-------
Provide low-level access to database pages.

Responsibilities:

    - open database files
    - close database files
    - read a page
    - write a page
    - allocate a new page
    - identify page offsets
    - manage file growth

    read_page(page_id, buffer)
    write_page(page_id, buffer)
    allocate_page()

  The rest of the database should not repeatedly call:
    fopen()
    fread() etc.


*/

#include <alloca.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef uint32_t PageId;

#define PAGE_SIZE 2400

#define DB_NAME "TEST_DB"
#define TABLE_NAME "TEST_TABLE"

// will contain as a folder/file
// data/DB_NAME/TABLE_NAME.dat

typedef struct {
  char name[23];
  int32_t age;
} Record;

typedef struct {
  uint16_t offset;
  uint16_t length;
} Slot;

typedef struct {
  PageId page_id;
  uint16_t num_slots;
  uint16_t free_space_start;
  uint16_t free_space_end;
} PageHeader;

typedef struct {
  PageHeader header;
  uint8_t data[PAGE_SIZE - sizeof(PageHeader)];
} Page;

// page_id,num_slots,free_space_start,free_space_end\n
#define HEADER_STRING_MAX 64

void allocate_page(PageId page_id) {
  char dirName[256];
  char fileName[256];

  if (mkdir("../data", 0755) != 0 && errno != EEXIST) {
    perror("mkdir ../data");
    return;
  }

  snprintf(dirName, sizeof(dirName), "../data/%s", DB_NAME);

  if (mkdir(dirName, 0755) != 0 && errno != EEXIST) {
    perror("mkdir");
    return;
  }

  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);

  FILE *file = fopen(fileName, "r+b");

  if (file == NULL) {
    file = fopen(fileName, "w+b");
  }

  if (file == NULL) {
    perror("fopen");
    return;
  }

  Page page = {};
  page.header.page_id = page_id;
  page.header.num_slots = 0;
  page.header.free_space_start = sizeof(PageHeader);
  page.header.free_space_end = PAGE_SIZE;

  fwrite(page.data, PAGE_SIZE, 1, file);
}

int main() { allocate_page(0); }
