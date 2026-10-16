CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -Iinclude

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)
LIB = libdatabase.so

# default target builds the shared library every consumer links against
all: $(LIB)

$(LIB): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -shared -o $@ $(SRC)

clean:
	rm -f $(LIB) a.out *.o src/*.o

.PHONY: all clean
