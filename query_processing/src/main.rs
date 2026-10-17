// query processor front-end, talks to the C engine (storage_engine/libdatabase.so)
//   make run   (from the repo root)
//
// all unsafe FFI lives in ffi.rs; this file only calls its safe wrappers

mod ffi;

use std::sync::Arc;

use datafusion::arrow::array::{
    ArrayRef, Float64Array, Int32Array, Int64Array, StringArray, UInt16Array,
};
use datafusion::arrow::datatypes::{DataType, Field, Schema};
use datafusion::arrow::record_batch::RecordBatch;
use datafusion::datasource::MemTable;
use datafusion::prelude::*;
use ffi::{ColType, ColumnSpec, Database, Value};

fn arrow_type(kind: Option<ColType>) -> DataType {
    match kind {
        Some(ColType::U16) => DataType::UInt16,
        Some(ColType::I32) => DataType::Int32,
        Some(ColType::I64) => DataType::Int64,
        Some(ColType::F64) => DataType::Float64,
        Some(ColType::Str) => DataType::Utf8,
        None => DataType::Null,
    }
}

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

    db.insert("USERS", &[Value::I32(1), Value::Str("Alice".to_string())])?;
    db.insert("USERS", &[Value::I32(2), Value::Str("Bob".to_string())])?;
    db.insert("USERS", &[Value::I32(3), Value::Str("Charlie".to_string())])?;

    // scan drives everything off the table metadata carried by the batch
    let scan = db.scan("USERS")?;

    let header: Vec<&str> = scan.table.columns.iter().map(|c| c.name.as_str()).collect();
    println!("{} rows in USERS", scan.rows.len());
    println!("{}", header.join(" | "));
    for row in &scan.rows {
        let cells: Vec<String> = row.iter().map(Value::to_string).collect();
        println!("{}", cells.join(" | "));
    }

    // build the arrow batch column by column from the scanned metadata + values
    let fields: Vec<Field> = scan
        .table
        .columns
        .iter()
        .map(|c| Field::new(&c.name, arrow_type(c.kind()), true))
        .collect();
    let schema = Arc::new(Schema::new(fields));

    let mut arrays: Vec<ArrayRef> = Vec::with_capacity(scan.table.columns.len());
    for (ci, col) in scan.table.columns.iter().enumerate() {
        let array: ArrayRef = match col.kind() {
            Some(ColType::U16) => Arc::new(
                scan.rows
                    .iter()
                    .map(|r| match &r[ci] {
                        Value::U16(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<UInt16Array>(),
            ),
            Some(ColType::I32) => Arc::new(
                scan.rows
                    .iter()
                    .map(|r| match &r[ci] {
                        Value::I32(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Int32Array>(),
            ),
            Some(ColType::I64) => Arc::new(
                scan.rows
                    .iter()
                    .map(|r| match &r[ci] {
                        Value::I64(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Int64Array>(),
            ),
            Some(ColType::F64) => Arc::new(
                scan.rows
                    .iter()
                    .map(|r| match &r[ci] {
                        Value::F64(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Float64Array>(),
            ),
            Some(ColType::Str) => Arc::new(
                scan.rows
                    .iter()
                    .map(|r| match &r[ci] {
                        Value::Str(v) => Some(v.as_str()),
                        _ => None,
                    })
                    .collect::<StringArray>(),
            ),
            None => return Err(format!("column {} has an unknown type", col.name).into()),
        };
        arrays.push(array);
    }

    let batch = RecordBatch::try_new(schema.clone(), arrays)?;

    let ctx = SessionContext::new();
    ctx.register_table(
        "USERS",
        Arc::new(MemTable::try_new(schema, vec![vec![batch]])?),
    )?;

    let df = ctx.sql("SELECT * FROM USERS ORDER BY 1").await?;
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
