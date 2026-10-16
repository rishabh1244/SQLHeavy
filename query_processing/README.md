# query_processing

The Rust front-end. It adds SQL on top of the C engine: it reads/writes rows
through FFI, hands them to **Apache DataFusion** as an in-memory Arrow table, and
runs SQL. The C engine stays SQL-free — DataFusion only ever sees a copy of the
rows in RAM.

## File map

| File | Responsibility |
|------|----------------|
| `src/ffi.rs` | The **only** module that touches `libdatabase.so`: raw `#[repr(C)]` types, `extern "C"` declarations, and all `unsafe`. Exposes safe wrappers: `Database`, `create_table`, `insert_user`, `scan_users`, `list_tables`. |
| `src/main.rs` | Demo driver: open DB → create/insert `USERS` → scan → run SQL → print catalogue. |
| `build.rs` | Tells Cargo where to find and link the C `.so`. It does **not** build it. |

## Linking (build.rs)

`build.rs` emits:

```
cargo:rustc-link-search=native=../lib
cargo:rustc-link-lib=dylib=database
cargo:rustc-link-arg=-Wl,-rpath,../lib
```

So `lib/libdatabase.so` must exist first. It is built by the top-level Makefile
(`make lib`), which runs `make -C storage_engine` and copies the result.

## Flow

```
Database::open("TEST_DB")          -> new_db            (C)
create_table("USERS", cols)        -> new_table         (C)
insert_user(...)                   -> db_insert_record  (C)
scan_users("USERS")                -> scan_table        (C)
    -> decode each raw record using column offsets from the catalogue
    -> build an Arrow RecordBatch
    -> register it as a DataFusion MemTable
    -> ctx.sql("SELECT ...") -> show()
list_tables()                      -> catalogue_load    (C)
```

The raw structs in `ffi.rs` (`ColumnMetadata`, `TableMetadata`, `Catalogue`,
`ScanBatch`) must stay byte-for-byte identical to the C structs in
`../storage_engine/include/`. The `User` layout (`i32 id` @0, `[u8;16] name` @4)
must match the column offsets/lengths passed to `create_table`.

## Build & run

From the repo root:

```sh
make            # builds the C .so (lib) then cargo build
make run        # builds, then cargo run
make clean
```

Or here:

```sh
cargo build
cargo run
```

> Note: `cargo build` alone fails if `../lib/libdatabase.so` is missing — run
> `make lib` first.

See `../storage_engine/README.md` for the storage engine internals.
