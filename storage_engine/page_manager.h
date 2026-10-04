/*
  Purpose
-------
Provide low-level access to database pages.

Responsibilities:

    - open database files
    - allocate a new page (writes the file header when the file is new)
    - read a page by PageId
    - insert a record into the latest page
    - fetch the latest page id
    - identify page offsets
    - manage file growth

    allocate_page()            -> id of the new page (page_count, +1 after)
    read_page(page_id)
    fetch_latest()
    insert_record(data, length)

  The rest of the database should not repeatedly call:
    fopen()
    fread() etc.

*/

#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <stdint.h>

typedef uint32_t PageId;

#define DB_NAME "TEST_DB"
#define TABLE_NAME "TEST_TABLE"

// will contain as a folder/file
// data/DB_NAME/TABLE_NAME.dat

typedef struct {
  PageId page_id;
  uint16_t num_slots;
  uint16_t free_space_start;
  uint16_t free_space_end;
} PageHeader;

typedef struct {
  uint32_t page_count;
  uint32_t page_size;
} fileHeader;

// total page size = data area + page header
#define PAGE_SIZE (2400 + sizeof(PageHeader))

typedef struct {
  PageHeader header;
  uint8_t data[PAGE_SIZE - sizeof(PageHeader)];
} Page;
typedef struct {
  uint16_t offset;
  uint16_t length;
} Slot;

// slot i lives at the tail of the page; it knows where record i is
static inline Slot get_slot(const Page *page, uint16_t i) {
  const Slot *s = (const Slot *)((const uint8_t *)page + PAGE_SIZE -
                                 (i + 1) * sizeof(Slot));
  return *s;
}

// page_id,num_slots,free_space_start,free_space_end\n
#define HEADER_STRING_MAX 64

PageId allocate_page(void);
Page read_page(PageId page_id);
PageId fetch_latest(void);
void insert_record(const void *data, uint16_t length);
uint16_t fetch_record(const Page *page, uint16_t index, void *out,
                      uint16_t max);

#endif
