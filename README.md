# database

A learning database, built bottom-up:

```
query_processing/   Rust front-end — SQL via DataFusion, talks to C over FFI
storage_engine/     C engine — pages, records, catalog, scans (knows no SQL)
plan.txt            Design notes and phased roadmap
```

Data is not in the DB layer; it lives in `data/` (a page-based binary format),
overridable with `MYDB_DATA_DIR`. See `storage_engine/README.md`.

## Build & run

```sh
make        # build the C shared library + cargo build
make run    # build, then run the Rust demo
make clean
```

`cargo build` alone needs `lib/libdatabase.so`, so run `make lib` first if you
build Rust directly.

## Where to start reading

- `storage_engine/README.md` — on-disk layout, page structure, code flow.
- `query_processing/README.md` — the FFI boundary and the DataFusion flow.
- `plan.txt` — full architecture and what is left to build.
