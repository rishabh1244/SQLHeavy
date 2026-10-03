#include "page_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

PageId allocate_page(void) {
  char dirName[256];
  char fileName[256];

  if (mkdir("../data", 0755) != 0 && errno != EEXIST) {
    perror("mkdir ../data");
    return (PageId)-1;
  }

  snprintf(dirName, sizeof(dirName), "../data/%s", DB_NAME);

  if (mkdir(dirName, 0755) != 0 && errno != EEXIST) {
    perror("mkdir");
    return (PageId)-1;
  }

  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);

  FILE *file = fopen(fileName, "r+b");

  if (file == NULL) {
    file = fopen(fileName, "w+b");
  }

  if (file == NULL) {
    perror("fopen");
    return (PageId)-1;
  }

  Page page = {};
  fileHeader file_head = {};

  if (fread(&file_head, sizeof(fileHeader), 1, file) != 1 ||
      file_head.page_size != PAGE_SIZE) {
    // no file header yet (or stale layout): start from an empty file
    file_head.page_count = 0;
    file_head.page_size = PAGE_SIZE;
  }

  // the new page always goes at the end: id = current page_count
  PageId page_id = file_head.page_count;
  file_head.page_count = page_id + 1;

  page.header.page_id = page_id;
  page.header.num_slots = 0;
  page.header.free_space_start = sizeof(PageHeader);
  page.header.free_space_end = PAGE_SIZE;

  if (fseek(file, (long)page_id * (long)PAGE_SIZE + (long)sizeof(fileHeader),
            SEEK_SET) != 0) {
    perror("fseek");
    fclose(file);
    return (PageId)-1;
  }
  if (fwrite(&page, sizeof(Page), 1, file) != 1) {
    perror("fwrite");
    fclose(file);
    return (PageId)-1;
  }

  rewind(file);
  if (fwrite(&file_head, sizeof(fileHeader), 1, file) != 1) {
    perror("fwrite");
    fclose(file);
    return (PageId)-1;
  }

  fclose(file);
  return page_id;
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
  Page page = {};

  if (page_id != (PageId)-1) {
    page = read_page(page_id);
  }

  // no page yet, unreadable page, or page full -> append a fresh page
  if (page_id == (PageId)-1 || page.header.page_id != page_id ||
      (size_t)page.header.free_space_start + length >
          (size_t)page.header.free_space_end) {
    page_id = allocate_page();
    if (page_id == (PageId)-1) {
      fprintf(stderr, "failed to allocate page\n");
      return;
    }
    page = read_page(page_id);
  }

  if (page.header.page_id != page_id) {
    fprintf(stderr, "failed to read page %u\n", (unsigned)page_id);
    return;
  }

  memcpy((uint8_t *)&page + page.header.free_space_start, data, length);

  page.header.free_space_start += length;
  page.header.num_slots += 1;

  char fileName[256];
  snprintf(fileName, sizeof(fileName), "../data/%s/%s.dat", DB_NAME,
           TABLE_NAME);

  FILE *file = fopen(fileName, "r+b");
  if (file == NULL) {
    perror("file not present");
    return;
  }

  if (fseek(file, (long)page_id * (long)PAGE_SIZE + (long)sizeof(fileHeader),
            SEEK_SET) != 0 ||
      fwrite(&page, sizeof(Page), 1, file) != 1) {
    perror("write page");
  }

  fclose(file);
}
