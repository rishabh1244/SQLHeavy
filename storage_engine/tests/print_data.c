#include "catalog.h"
#include "scan.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// printing data
//
static void print_record(const RawRecord *record, const TableMetadata *meta) {
  for (uint16_t c = 0; c < meta->column_count; c++) {
    const ColumnMetadata *col = &meta->columns[c];

    if (c > 0) {
      printf(" | ");
    }

    printf("%s: ", col->name);

    // Ensure the column fits within this record.
    if ((size_t)col->offset + col->length > record->len) {
      printf("<invalid field>");
      continue;
    }

    const uint8_t *field = (const uint8_t *)record->data + col->offset;

    switch (col->type) {
    case TYPE_UINT16: {
      if (col->length != sizeof(uint16_t)) {
        printf("<invalid uint16>");
        break;
      }

      uint16_t value;
      memcpy(&value, field, sizeof(value));
      printf("%" PRIu16, value);
      break;
    }

    case TYPE_INT32: {
      if (col->length != sizeof(int32_t)) {
        printf("<invalid int32>");
        break;
      }

      int32_t value;
      memcpy(&value, field, sizeof(value));
      printf("%" PRId32, value);
      break;
    }

    case TYPE_INT64: {
      if (col->length != sizeof(int64_t)) {
        printf("<invalid int64>");
        break;
      }

      int64_t value;
      memcpy(&value, field, sizeof(value));
      printf("%" PRId64, value);
      break;
    }

    case TYPE_FLOAT64: {
      if (col->length != sizeof(double)) {
        printf("<invalid float64>");
        break;
      }

      double value;
      memcpy(&value, field, sizeof(value));
      printf("%g", value);
      break;
    }

    case TYPE_STRING: {
      // A fixed-width string might not contain a terminating '\0'.
      size_t len = 0;
      while (len < col->length && field[len] != '\0') {
        len++;
      }

      printf("%.*s", (int)len, (const char *)field);
      break;
    }

    default:
      printf("<unknown type>");
      break;
    }
  }

  putchar('\n');
}

static void print_batch(const ScanBatch *batch) {
  const TableMetadata *meta = &batch->meta;

  // Print column headers.
  for (uint16_t c = 0; c < meta->column_count; c++) {
    if (c > 0) {
      printf(" | ");
    }
    printf("%s", meta->columns[c].name);
  }
  putchar('\n');

  // Print every record.
  for (uint32_t i = 0; i < batch->count; i++) {
    print_record(&batch->records[i], meta);
  }
}
