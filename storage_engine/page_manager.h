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

// page_id,num_slots,free_space_start,free_space_end\n
#define HEADER_STRING_MAX 64

void allocate_page(PageId page_id);
Page read_page(PageId page_id);
void write_page(PageId page_id, const Page *page);
PageId fetch_latest(void);
void insert_record(const void *data, uint16_t length);

#endif
