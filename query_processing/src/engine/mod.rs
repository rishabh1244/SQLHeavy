// engine — the query-processing boundary. It wraps the C storage engine over
// FFI and DataFusion behind a handful of high-level calls, so `main.rs` never
// touches C directly.
//
//     mod.rs   this file: the public API + re-exports below
//     db.rs    safe wrapper over the C database handle
//     ffi.rs   raw #[repr(C)] types and extern "C" declarations
//     types.rs SQL-free domain types (Value, ColumnSpec, ...) + byte helpers
//     query.rs DataFusion: fetch_data(query) and print_records

mod db;
mod ffi;
mod query;
mod types;

pub use db::Database;
pub use types::{ColType, ColumnSpec, QueryResult, Value};

/// Opens (creating if needed) the database `name`.
pub fn open_db(name: &str) -> Result<Database, String> {
    Database::open(name)
}

/// Declares `table` with the given `columns` (no-op if it already exists).
pub fn create_table(db: &Database, table: &str, columns: &[ColumnSpec]) -> Result<(), String> {
    db.create_table(table, columns)
}

/// Appends one record to `table`; `values` must match the table's columns.
pub fn insert_data(db: &Database, table: &str, values: &[Value]) -> Result<(), String> {
    db.insert(table, values)
}

/// Runs the SQL `query` over the catalogued tables and returns its contents.
pub async fn fetch_data(db: &Database, query: &str) -> Result<QueryResult, String> {
    query::fetch_data(db, query).await
}

/// Prints a `QueryResult` as an ordered `col | col` table.
pub fn print_records(result: &QueryResult) {
    query::print_records(result)
}

/// Scans `table` and prints every raw record with its unique physical id.
pub fn print_table(db: &Database, table: &str) -> Result<(), String> {
    let scan = db.scan(table)?;

    print!("{:<8}", "rid");
    for col in &scan.table.columns {
        print!(" | {}", col.name);
    }
    println!();

    for record in &scan.records {
        print!("{:<8}", record.id.as_u64());
        for value in &record.values {
            print!(" | {value}");
        }
        println!();
    }
    Ok(())
}
