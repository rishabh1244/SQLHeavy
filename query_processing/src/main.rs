// query processor front-end, talks to the C engine (storage_engine/libdatabase.so)
//   make run   (from the repo root)
//
// all unsafe FFI lives in ffi.rs; this file only calls its safe wrappers

mod ffi;

use std::sync::Arc;

use datafusion::arrow::array::{Int32Array, StringArray};
use datafusion::arrow::datatypes::{DataType, Field, Schema};
use datafusion::arrow::record_batch::RecordBatch;
use datafusion::datasource::MemTable;
use datafusion::prelude::*;
use ffi::{ColType, ColumnSpec, Database};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
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

    // hand the scanned rows to datafusion as an in-memory arrow table
    let schema = Arc::new(Schema::new(vec![
        Field::new("id", DataType::Int32, false),
        Field::new("name", DataType::Utf8, false),
    ]));
    let ids: Int32Array = rows.iter().map(|r| r.id).collect();
    let names: StringArray = rows.iter().map(|r| Some(r.name.as_str())).collect();
    let batch = RecordBatch::try_new(schema.clone(), vec![Arc::new(ids), Arc::new(names)])?;

    let ctx = SessionContext::new();
    ctx.register_table("USERS", Arc::new(MemTable::try_new(schema, vec![vec![batch]])?))?;

    let df = ctx
        .sql("SELECT id, name FROM USERS WHERE id > 1 ORDER BY id")
        .await?;
    df.show().await?;

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
