// query processor front-end. All C/DataFusion details live behind the `engine`
// module, so this file only orchestrates high-level calls.

mod engine;

use engine::{
    ColType, ColumnSpec, Value, create_table, fetch_data, insert_data, open_db, print_records,
};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let db = open_db("TEST_DB")?;

    create_table(
        &db,
        "USERS",
        &[
            ColumnSpec::new("id", ColType::I32, 0, 4),
            ColumnSpec::new("name", ColType::Str, 4, 16),
        ],
    )?;

    insert_data(&db, "USERS", &[Value::I32(1), Value::Str("Alice".into())])?;
    insert_data(&db, "USERS", &[Value::I32(2), Value::Str("Bob".into())])?;
    insert_data(&db, "USERS", &[Value::I32(3), Value::Str("Charlie".into())])?;

    let result = fetch_data(&db, "SELECT id, name FROM USERS ORDER BY id").await?;
    print_records(&result);

    Ok(())
}
