CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -Iinclude

LIB_SRC = $(filter-out src/main.c, $(wildcard src/*.c))
HDR = $(wildcard include/*.h)
LIB = libdatabase.so
BIN = db

# default target builds the shared library, no .o files are left behind
all: $(LIB)

$(LIB): $(LIB_SRC) $(HDR)
	$(CC) $(CFLAGS) -shared -o $@ $(LIB_SRC)

# demo program, same sources plus main.c  (make db / make run)
$(BIN): src/main.c $(LIB_SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LIB_SRC)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(LIB) $(BIN) a.out *.o src/*.o

.PHONY: all run clean
