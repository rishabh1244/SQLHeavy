// engine/query.rs — the DataFusion boundary. It turns scanned records into an
// in-memory Arrow table, runs the SQL `query`, and decodes the result back into
// engine `Value`s so callers never see Arrow types.

use std::collections::HashSet;
use std::sync::Arc;

use datafusion::arrow::array::{
    ArrayRef, Float64Array, Int32Array, Int64Array, StringArray, UInt16Array,
};
use datafusion::arrow::datatypes::{DataType, Field, Schema};
use datafusion::arrow::record_batch::RecordBatch;
use datafusion::common::ScalarValue;
use datafusion::datasource::MemTable;
use datafusion::prelude::*;

use super::db::Database;
use super::types::{ColType, QueryResult, TableInfo, Value};

/// Runs `query` via DataFusion over every catalogued table and returns its data.
pub(crate) async fn fetch_data(db: &Database, query: &str) -> Result<QueryResult, String> {
    let ctx = SessionContext::new();

    // SQL identifiers are case-insensitive, so two catalogue tables whose names
    // differ only by case would collide. Keep the first and warn on the rest.
    let mut registered = HashSet::new();
    for table in db.list_tables()? {
        if !registered.insert(table.name.to_ascii_lowercase()) {
            eprintln!(
                "warning: table {} skipped — its name collides (case-insensitively) with another table",
                table.name
            );
            continue;
        }
        let scan = db.scan(&table.name)?;
        let schema = arrow_schema(&scan.table);
        let batch = arrow_batch(schema.clone(), &scan.table, &scan.rows)?;
        let mem = MemTable::try_new(schema.into(), vec![vec![batch]]).map_err(|e| e.to_string())?;
        ctx.register_table(table.name.as_str(), Arc::new(mem))
            .map_err(|e| e.to_string())?;
    }

    let df = ctx.sql(query).await.map_err(|e| e.to_string())?;
    let batches = df.collect().await.map_err(|e| e.to_string())?;
    decode_batches(&batches)
}

/// Prints a result as an ordered `col | col` table.
pub(crate) fn print_records(result: &QueryResult) {
    if !result.columns.is_empty() {
        println!("{}", result.columns.join(" | "));
    }
    for row in &result.rows {
        let cells: Vec<String> = row.iter().map(Value::to_string).collect();
        println!("{}", cells.join(" | "));
    }
}

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

fn arrow_schema(info: &TableInfo) -> Schema {
    let fields = info
        .columns
        .iter()
        .map(|c| Field::new(&c.name, arrow_type(c.kind()), true))
        .collect::<Vec<_>>();
    Schema::new(fields)
}

fn arrow_batch(schema: Schema, info: &TableInfo, rows: &[Vec<Value>]) -> Result<RecordBatch, String> {
    let mut arrays: Vec<ArrayRef> = Vec::with_capacity(info.columns.len());
    for (ci, col) in info.columns.iter().enumerate() {
        let array: ArrayRef = match col.kind() {
            Some(ColType::U16) => Arc::new(
                rows.iter()
                    .map(|r| match &r[ci] {
                        Value::U16(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<UInt16Array>(),
            ),
            Some(ColType::I32) => Arc::new(
                rows.iter()
                    .map(|r| match &r[ci] {
                        Value::I32(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Int32Array>(),
            ),
            Some(ColType::I64) => Arc::new(
                rows.iter()
                    .map(|r| match &r[ci] {
                        Value::I64(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Int64Array>(),
            ),
            Some(ColType::F64) => Arc::new(
                rows.iter()
                    .map(|r| match &r[ci] {
                        Value::F64(v) => Some(*v),
                        _ => None,
                    })
                    .collect::<Float64Array>(),
            ),
            Some(ColType::Str) => Arc::new(
                rows.iter()
                    .map(|r| match &r[ci] {
                        Value::Str(v) => Some(v.as_str()),
                        _ => None,
                    })
                    .collect::<StringArray>(),
            ),
            None => return Err(format!("column {} has an unknown type", col.name)),
        };
        arrays.push(array);
    }
    RecordBatch::try_new(Arc::new(schema), arrays).map_err(|e| e.to_string())
}

fn decode_batches(batches: &[RecordBatch]) -> Result<QueryResult, String> {
    let mut columns: Vec<String> = Vec::new();
    let mut rows: Vec<Vec<Value>> = Vec::new();

    for (b, batch) in batches.iter().enumerate() {
        if b == 0 {
            columns = batch
                .schema()
                .fields()
                .iter()
                .map(|f| f.name().clone())
                .collect();
        }
        for r in 0..batch.num_rows() {
            let mut row = Vec::with_capacity(batch.num_columns());
            for c in 0..batch.num_columns() {
                let scalar = ScalarValue::try_from_array(batch.column(c).as_ref(), r)
                    .map_err(|e| e.to_string())?;
                row.push(scalar_value(scalar));
            }
            rows.push(row);
        }
    }

    Ok(QueryResult { columns, rows })
}

fn scalar_value(s: ScalarValue) -> Value {
    if s.is_null() {
        return Value::Null;
    }
    match s {
        ScalarValue::UInt16(Some(v)) => Value::U16(v),
        ScalarValue::Int32(Some(v)) => Value::I32(v),
        ScalarValue::Int64(Some(v)) => Value::I64(v),
        ScalarValue::Float64(Some(v)) => Value::F64(v),
        ScalarValue::Utf8(Some(v))
        | ScalarValue::Utf8View(Some(v))
        | ScalarValue::LargeUtf8(Some(v)) => Value::Str(v),
        other => Value::Str(other.to_string()),
    }
}
