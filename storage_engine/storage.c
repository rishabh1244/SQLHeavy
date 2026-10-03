#include "page_manager.h"
#include <string.h>

typedef struct {
  int age;
  char name[];
} record;

int main() {
  record *n1;
  n1->age = 12;
  strcpy(n1->name, "NAME");
  allocate_page(0);
  insert_record(n1, sizeof(record));
  read_page(0);
}
