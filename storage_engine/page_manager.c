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

#define PAGE_SIZE 2400
#include <stdint.h>
typedef uint32_t PageId;

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
