#include "catalog.h"

#include "page_manager.h"

#include <stdlib.h>
#include <string.h>

// copies record i of the current target file into out
static int copy_meta(const Page *page, uint16_t i, TableMetadata *out) {
  Slot s = get_slot(page, i);

  if (s.length != sizeof(TableMetadata) ||
      s.offset < sizeof(PageHeader) ||
      (size_t)s.offset + s.length > PAGE_SIZE) {
    return -1;
  }

  memcpy(out, (const uint8_t *)page + s.offset, sizeof(TableMetadata));
  return 0;
}

int catalogue_create(const char *db_name) {
  if (db_name == NULL) {
    return -1;
  }

  set_catalogue_target(db_name);
  return create_page_file();
}

int catalogue_get(const char *db_name, const char *table_name,
                  TableMetadata *out) {
  if (db_name == NULL || table_name == NULL || out == NULL) {
    return -1;
  }

  set_catalogue_target(db_name);

  PageId latest = fetch_latest();
  if (latest == (PageId)-1) {
    return -1;
  }

  for (PageId id = 0; id <= latest; id++) {
    Page page = read_page(id);
    if (page.header.page_id != id) {
      return -1;
    }

    for (uint16_t i = 0; i < page.header.num_slots; i++) {
      TableMetadata meta;
      if (copy_meta(&page, i, &meta) != 0) {
        return -1;
      }
      if (strcmp(meta.name, table_name) == 0) {
        *out = meta;
        return 0;
      }
    }
  }

  return -1;
}

int catalogue_put(const char *db_name, const TableMetadata *meta) {
  if (db_name == NULL || meta == NULL) {
    return -1;
  }

  TableMetadata existing;
  if (catalogue_get(db_name, meta->name, &existing) == 0) {
    return -1; // already catalogued
  }

  set_catalogue_target(db_name);
  insert_record(meta, (uint16_t)sizeof(TableMetadata));
  return 0;
}

Catalogue *catalogue_load(const char *db_name) {
  if (db_name == NULL) {
    return NULL;
  }

  Catalogue *cat = calloc(1, sizeof(Catalogue));
  if (cat == NULL) {
    return NULL;
  }

  set_catalogue_target(db_name);

  PageId latest = fetch_latest();
  if (latest == (PageId)-1) {
    return cat; // no records yet
  }

  for (PageId id = 0; id <= latest && cat->table_count < MAX_TABLES; id++) {
    Page page = read_page(id);
    if (page.header.page_id != id) {
      catalogue_free(cat);
      return NULL;
    }

    for (uint16_t i = 0;
         i < page.header.num_slots && cat->table_count < MAX_TABLES; i++) {
      if (copy_meta(&page, i, &cat->tables[cat->table_count]) != 0) {
        catalogue_free(cat);
        return NULL;
      }
      cat->table_count++;
    }
  }

  return cat;
}

void catalogue_free(Catalogue *catalogue) { free(catalogue); }
