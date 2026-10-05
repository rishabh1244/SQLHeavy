# top level build
#   make        -> lib/libdatabase.so + cargo build of query_processing
#   make run    -> cargo run
#   make clean

STORAGE = storage_engine
QP = query_processing
LIB = lib/libdatabase.so

all: rust

# build the C engine and install the shared library where rust links from
lib:
	$(MAKE) -C $(STORAGE)
	mkdir -p lib
	cp $(STORAGE)/libdatabase.so $(LIB)

rust: lib
	cd $(QP) && cargo build

run: rust
	cd $(QP) && cargo run

clean:
	$(MAKE) -C $(STORAGE) clean
	rm -rf lib main
	cd $(QP) && cargo clean

.PHONY: all lib rust run clean
