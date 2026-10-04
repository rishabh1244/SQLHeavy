#include "page_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char g_db_name[64];
static char g_path[256];

// ../data/<db_name>/tables/<table_name>.dat
void set_target(const char *db_name, const char *table_name) {
  snprintf(g_db_name, sizeof(g_db_name), "%s", db_name ? db_name : "");
  snprintf(g_path, sizeof(g_path), "../data/%s/tables/%s.dat", g_db_name,
           table_name ? table_name : "");
}

// ../data/<db_name>/catalog.dat
void set_catalogue_target(const char *db_name) {
  snprintf(g_db_name, sizeof(g_db_name), "%s", db_name ? db_name : "");
  snprintf(g_path, sizeof(g_path), "../data/%s/catalog.dat", g_db_name);
}

int target_exists(void) { return access(g_path, F_OK) == 0; }

// ../data, ../data/<db_name>, ../data/<db_name>/tables
static int make_dirs(void) {
  char dirName[256];

  if (mkdir("../data", 0755) != 0 && errno != EEXIST) {
    perror("mkdir ../data");
    return -1;
  }

  snprintf(dirName, sizeof(dirName), "../data/%s", g_db_name);
  if (mkdir(dirName, 0755) != 0 && errno != EEXIST) {
    perror("mkdir");
    return -1;
  }

  snprintf(dirName, sizeof(dirName), "../data/%s/tables", g_db_name);
  if (mkdir(dirName, 0755) != 0 && errno != EEXIST) {
    perror("mkdir");
    return -1;
  }

  return 0;
}

int create_page_file(void) {
  if (make_dirs() != 0) {
    return -1;
  }

  if (target_exists()) {
    return 0;
  }

  FILE *file = fopen(g_path, "w+b");
  if (file == NULL) {
    perror("fopen");
    return -1;
  }

  fileHeader file_head = {.page_count = 0, .page_size = PAGE_SIZE};
  int ok = fwrite(&file_head, sizeof(fileHeader), 1, file) == 1;
  fclose(file);

  if (!ok) {
    perror("fwrite");
    return -1;
  }
  return 0;
}

PageId allocate_page(void) {
  if (make_dirs() != 0) {
    return (PageId)-1;
  }

  FILE *file = fopen(g_path, "r+b");

  if (file == NULL) {
    file = fopen(g_path, "w+b");
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
  FILE *file = fopen(g_path, "rb");

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
  FILE *file = fopen(g_path, "rb");
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
  // (each insert needs room for the record AND its slot entry)
  if (page_id == (PageId)-1 || page.header.page_id != page_id ||
      (size_t)page.header.free_space_start + length + sizeof(Slot) >
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

  // records grow up from the header, slots grow down from the page tail
  uint16_t rec_offset = page.header.free_space_start;
  memcpy((uint8_t *)&page + rec_offset, data, length);

  Slot *slot = (Slot *)((uint8_t *)&page + PAGE_SIZE -
                        (page.header.num_slots + 1) * sizeof(Slot));
  slot->offset = rec_offset;
  slot->length = length;

  page.header.free_space_start += length;
  page.header.num_slots += 1;
  page.header.free_space_end -= sizeof(Slot);

  FILE *file = fopen(g_path, "r+b");
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

// array access: fetch_record(&page, 0, &r, sizeof(r)) -> record 0
// copies min(record_length, max) bytes into out, returns the record's length
// (0 = bad index or bad slot)
uint16_t fetch_record(const Page *page, uint16_t index, void *out,
                      uint16_t max) {
  if (index >= page->header.num_slots) {
    return 0;
  }

  Slot s = get_slot(page, index);
  if (s.offset < sizeof(PageHeader) ||
      (size_t)s.offset + s.length > page->header.free_space_start) {
    return 0;
  }

  uint16_t len = s.length < max ? s.length : max;
  memcpy(out, (const uint8_t *)page + s.offset, len);
  return s.length;
}
