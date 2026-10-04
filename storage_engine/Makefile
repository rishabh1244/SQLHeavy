CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)
BIN = db

# compile + link in one step, so no .o files are left behind
all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) a.out *.o src/*.o

.PHONY: all run clean
