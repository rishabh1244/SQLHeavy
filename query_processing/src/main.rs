// query processor front-end, talks to the C engine (storage_engine/libdatabase.so)
//   make run   (from the repo root)
//
// all unsafe FFI lives in ffi.rs; this file only calls its safe wrappers

mod ffi;

use ffi::{ColType, ColumnSpec, Database};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let db = Database::open("TEST_DB")?;

    db.create_table(
        "USERS",
        &[
            ColumnSpec::new("id", ColType::I32, 0, 4),
            ColumnSpec::new("name", ColType::Str, 4, 16),
        ],
    )?;

    db.insert_user("USERS", 1, "Alice")?;
    db.insert_user("USERS", 2, "Bob")?;
    db.insert_user("USERS", 3, "Charlie")?;

    let rows = db.scan_users("USERS")?;
    println!("{} rows in USERS", rows.len());
    for (i, row) in rows.iter().enumerate() {
        println!("[{}] id={} name={}", i, row.id, row.name);
    }

    let tables = db.list_tables()?;
    println!("catalogue: {} tables", tables.len());
    for table in &tables {
        print!("  {} ({} cols)", table.name, table.columns.len());
        for col in &table.columns {
            print!(
                " | {} type={} off={} len={}",
                col.name, col.type_, col.offset, col.length
            );
        }
        println!();
    }

    Ok(())
}
