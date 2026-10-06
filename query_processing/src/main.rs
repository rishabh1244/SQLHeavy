// query processor front-end, talks to the C engine (storage_engine/libdatabase.so)
//   rustc src/main.rs -L ../storage_engine -l database
//   LD_LIBRARY_PATH=../storage_engine ./main

use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};

const TYPE_UINT16: c_int = 0;
const TYPE_INT32: c_int = 1;
const TYPE_INT64: c_int = 2;
const TYPE_FLOAT64: c_int = 3;
const TYPE_STRING: c_int = 4;

#[repr(C)]
pub struct RawRecord {
    pub data: *const u8,
    pub len: u32,
}

#[repr(C)]
pub struct ScanBatch {
    pub records: *mut RawRecord,
    pub count: u32,
}

#[repr(C)]
pub struct ColumnMetadata {
    pub name: [u8; 32],
    pub type_: c_int,
    pub offset: u16,
    pub length: u16,
}

#[repr(C)]
pub struct TableMetadata {
    pub name: [u8; 64],
    pub column_count: u16,
    pub columns: [ColumnMetadata; 32],
}

#[repr(C)]
pub struct Catalogue {
    pub table_count: u16,
    pub tables: [TableMetadata; 128],
}

#[repr(C)]
pub struct Database {
    _private: [u8; 0],
}

// typedef struct { int32_t id; char name[16]; } user;  -> 20 bytes
#[repr(C)]
pub struct User {
    pub id: i32,
    pub name: [u8; 16],
}

unsafe extern "C" {
    fn new_db(name: *const c_char) -> *mut Database;
    fn close_db(db: *mut Database);

    fn new_table(
        db: *mut Database,
        name: *const c_char,
        columns: *const ColumnMetadata,
        column_count: u16,
    ) -> *mut TableMetadata;

    fn db_insert_record(
        db: *mut Database,
        table_name: *const c_char,
        data: *const c_void,
        length: u16,
    ) -> c_int;

    fn scan_table(db_name: *const c_char, table_name: *const c_char, out: *mut ScanBatch) -> c_int;
    fn scan_batch_free(batch: *mut ScanBatch);

    fn catalogue_load(db_name: *const c_char) -> *mut Catalogue;
    fn catalogue_free(catalogue: *mut Catalogue);
}

fn c_field(bytes: &[u8]) -> String {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end]).into_owned()
}

fn type_width(type_: c_int) -> usize {
    match type_ {
        TYPE_UINT16 => 2,
        TYPE_INT32 => 4,
        TYPE_INT64 | TYPE_FLOAT64 => 8,
        _ => 0,
    }
}

fn decode_value(type_: c_int, raw: &[u8]) -> String {
    match type_ {
        TYPE_STRING => c_field(raw),
        t if raw.len() < type_width(t) => {
            format!("<short: {} < {} bytes>", raw.len(), type_width(t))
        }
        TYPE_UINT16 => u16::from_le_bytes(word(raw)).to_string(),
        TYPE_INT32 => i32::from_le_bytes(word(raw)).to_string(),
        TYPE_INT64 => i64::from_le_bytes(word(raw)).to_string(),
        TYPE_FLOAT64 => f64::from_le_bytes(word(raw)).to_string(),
        t => format!("<type {}>", t),
    }
}

// zero pads when raw is short (decode_value already rejected those cases)
fn word<const N: usize>(raw: &[u8]) -> [u8; N] {
    let mut out = [0u8; N];
    let n = raw.len().min(N);
    out[..n].copy_from_slice(&raw[..n]);
    out
}

// every field name/offset/type comes from the catalogue, never from rust code
fn decode_record(meta: &TableMetadata, bytes: &[u8]) -> String {
    let mut parts = Vec::with_capacity(meta.column_count as usize);

    for c in meta.columns.iter().take(meta.column_count as usize) {
        let start = c.offset as usize;
        let raw = bytes.get(start..start + c.length as usize).unwrap_or(&[]);
        parts.push(format!(
            "{}={}",
            c_field(&c.name),
            decode_value(c.type_, raw)
        ));
    }

    parts.join(" ")
}

fn lookup_table<'a>(cat: &'a Catalogue, name: &str) -> Option<&'a TableMetadata> {
    (0..cat.table_count as usize)
        .map(|i| &cat.tables[i])
        .find(|t| c_field(&t.name) == name)
}

fn fill<const N: usize>(dst: &mut [u8; N], src: &str) {
    let bytes = src.as_bytes();
    let n = bytes.len().min(N - 1);
    dst[..n].copy_from_slice(&bytes[..n]);
}

fn column(name: &str, type_: c_int, offset: u16, length: u16) -> ColumnMetadata {
    let mut col = ColumnMetadata {
        name: [0; 32],
        type_,
        offset,
        length,
    };
    fill(&mut col.name, name);
    col
}

fn user(id: i32, name: &str) -> User {
    let mut u = User { id, name: [0; 16] };
    fill(&mut u.name, name);
    u
}

fn insert(db: *mut Database, table: &CString) {
    let users = [user(1, "Alice"), user(2, "Bob"), user(3, "Charlie")];

    for u in &users {
        unsafe {
            db_insert_record(
                db,
                table.as_ptr(),
                u as *const User as *const c_void,
                std::mem::size_of::<User>() as u16,
            );
        }
    }
}

fn fetch(db_name: &CString, table: &CString) {
    let cat_ptr = unsafe { catalogue_load(db_name.as_ptr()) };
    if cat_ptr.is_null() {
        eprintln!("catalogue_load failed");
        return;
    }
    let cat = unsafe { &*cat_ptr };
    let meta = lookup_table(cat, table.to_str().unwrap());

    let mut batch = ScanBatch {
        records: std::ptr::null_mut(),
        count: 0,
    };
    unsafe { scan_table(db_name.as_ptr(), table.as_ptr(), &mut batch) };

    println!("{} rows in {}", batch.count, table.to_str().unwrap());
    match meta {
        None => println!("no catalogue entry for {}", table.to_str().unwrap()),
        Some(meta) if !batch.records.is_null() => {
            let records =
                unsafe { std::slice::from_raw_parts(batch.records, batch.count as usize) };
            for (i, rec) in records.iter().enumerate() {
                let bytes = unsafe { std::slice::from_raw_parts(rec.data, rec.len as usize) };
                println!("[{}] {}", i, decode_record(meta, bytes));
            }
        }
        Some(_) => {}
    }
    unsafe { scan_batch_free(&mut batch) };

    println!("catalogue: {} tables", cat.table_count);
    for i in 0..cat.table_count as usize {
        let t = &cat.tables[i];
        print!("  {} ({} cols)", c_field(&t.name), t.column_count);

        for j in 0..t.column_count as usize {
            let c = &t.columns[j];
            print!(
                " | {} type={} off={} len={}",
                c_field(&c.name),
                c.type_,
                c.offset,
                c.length
            );
        }
        println!();
    }
    unsafe { catalogue_free(cat_ptr) };
}

fn main() {
    let db_name = CString::new("TEST_DB").unwrap();
    let table = CString::new("USERS").unwrap();

    let db = unsafe { new_db(db_name.as_ptr()) };

    let columns = [
        column("id", TYPE_INT32, 0, 4),
        column("name", TYPE_STRING, 4, 16),
    ];
    unsafe { new_table(db, table.as_ptr(), columns.as_ptr(), columns.len() as u16) };

    insert(db, &table);
    fetch(&db_name, &table);

    unsafe { close_db(db) };
}
