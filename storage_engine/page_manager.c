#include "page_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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

  if (fread(&page, sizeof(Page), 1, file) != 1) {
    perror("fread");
    fclose(file);
    return page;
  }

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
PageId fetch_latest(void) {
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

void insert_record(const void *data, uint16_t length) {
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

  if ((size_t)page.header.free_space_start + length >
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

  memcpy((uint8_t *)&page + page.header.free_space_start, data, length);

  page.header.free_space_start += length;
  page.header.num_slots += 1;

  write_page(page_id, &page);
}
