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
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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

  // give incrementing id to each page
  Page page = {};
  fileHeader file_head = {};

  if (fread(&file_head, sizeof(fileHeader), 1, file) != 1 ||
      file_head.page_size != PAGE_SIZE) {
    file_head.page_count = 0;
    file_head.page_size = PAGE_SIZE;
  }

  if (file_head.page_count < page_id + 1) {
    file_head.page_count = page_id + 1;
  }

  page.header.page_id = page_id;
  page.header.num_slots = 0;
  page.header.free_space_start = sizeof(PageHeader);
  page.header.free_space_end = PAGE_SIZE;

  if (fseek(file, (long)page_id * (long)PAGE_SIZE + (long)sizeof(fileHeader),
            SEEK_SET) != 0) {
    perror("fseek");
    fclose(file);
    return;
  }
  fwrite(&page, sizeof(Page), 1, file);

  rewind(file);
  fwrite(&file_head, sizeof(fileHeader), 1, file);
  fclose(file);
}

Page read_page(PageId page_id) {
  char fileName[256];

  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);
  FILE *file = fopen(fileName, "rb");

  Page page = {};
  if (file == NULL) {
    perror("file not present");
    return page;
  }

  if (fseek(file, (long)page_id * (long)PAGE_SIZE + sizeof(fileHeader),
            SEEK_SET) != 0) {
    perror("fseek");
    fclose(file);
    return page;
  }

  // seek header
  //

  if (fread(&page, sizeof(Page), 1, file) != 1) {
    perror("fread");
    fclose(file);
    return page;
  }
  // printf("page_id         : %u\n", (unsigned)page.header.page_id);
  // printf("num_slots       : %u\n", (unsigned)page.header.num_slots);
  // printf("free_space_start: %u\n", (unsigned)page.header.free_space_start);
  // printf("free_space_end  : %u\n", (unsigned)page.header.free_space_end);

  fclose(file);
  return page;
}

void write_page(PageId page_id, const Page *page) {
  char fileName[256];

  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);
  FILE *file = fopen(fileName, "r+b");

  if (file == NULL) {
    perror("file not present");
    return;
  }

  if (fseek(file, (long)page_id * (long)PAGE_SIZE + (long)sizeof(fileHeader),
            SEEK_SET) != 0) {
    perror("fseek");
    fclose(file);
    return;
  }

  if (fwrite(page, sizeof(Page), 1, file) != 1) {
    perror("fwrite");
    fclose(file);
    return;
  }

  fclose(file);
}

// can be used to check how many pages are present ,also size of each page
// returns the id of the newest page, (PageId)-1 when there is no page yet
PageId fetch_latest() {
  // reads file header
  char fileName[256];

  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);

  FILE *file = fopen(fileName, "rb");
  if (file == NULL) {
    perror("file not present");
    return (PageId)-1;
  }
  fileHeader header = {};
  if (fread(&header, sizeof(fileHeader), 1, file) != 1) {
    perror("fread");
    fclose(file);
    return (PageId)-1;
  }
  fclose(file);

  if (header.page_count == 0) {
    return (PageId)-1;
  }
  return header.page_count - 1;
}

void insert_data(uint64_t record) {
  PageId page_id = fetch_latest();

  if (page_id == (PageId)-1) {
    // no page in the file yet, start with page 0
    allocate_page(0);
    page_id = 0;
  }

  Page page = read_page(page_id);
  if (page.header.page_id != page_id) {
    fprintf(stderr, "failed to read page %u\n", (unsigned)page_id);
    return;
  }

  if ((size_t)page.header.free_space_start + sizeof(record) >
      (size_t)page.header.free_space_end) {
    // newest page is full, keep appending on a fresh page
    allocate_page(page_id + 1);
    page_id = page_id + 1;
    page = read_page(page_id);
    if (page.header.page_id != page_id) {
      fprintf(stderr, "failed to read page %u\n", (unsigned)page_id);
      return;
    }
  }

  memcpy((uint8_t *)&page + page.header.free_space_start, &record,
         sizeof(record));
  page.header.free_space_start += sizeof(record);
  page.header.num_slots += 1;

  write_page(page_id, &page);
}

// int insert_record(Page *page, const uint8_t *data, uint16_t length) {}
int main() {
  insert_data(111);
  insert_data(222);

  PageId latest = fetch_latest();
  printf("latest page id: %u\n", (unsigned)latest);

  Page page = read_page(latest);
  printf("page_id         : %u\n", (unsigned)page.header.page_id);
  printf("num_slots       : %u\n", (unsigned)page.header.num_slots);
  printf("free_space_start: %u\n", (unsigned)page.header.free_space_start);
  printf("free_space_end  : %u\n", (unsigned)page.header.free_space_end);

  for (uint16_t i = 0; i < page.header.num_slots; i++) {
    uint64_t record = 0;
    memcpy(&record, (uint8_t *)&page + sizeof(PageHeader) + i * sizeof(record),
           sizeof(record));
    printf("record[%u]       : %llu\n", (unsigned)i, (unsigned long long)record);
  }
  return 0;
}
