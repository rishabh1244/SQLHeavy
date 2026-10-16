
CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -Iinclude

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)
LIB = libdatabase.so

TEST_SRC = tests/test_storage.c
TEST_BIN = test_storage

.PHONY: all lib test clean

all: lib

lib: $(LIB)

$(LIB): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -shared -o $@ $(SRC)

$(TEST_BIN): $(TEST_SRC) $(HDR) $(LIB)
	$(CC) -Wall -Wextra -g -Iinclude \
		-o $@ $(TEST_SRC) -L. -ldatabase \
		-Wl,-rpath,'$$ORIGIN'

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(LIB) $(TEST_BIN) a.out *.o src/*.o

