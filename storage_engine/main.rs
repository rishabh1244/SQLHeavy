use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};

const TYPE_UINT16: c_int = 0;
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

// opaque handle, layout lives on the C side
#[repr(C)]
pub struct Database {
    _private: [u8; 0],
}

// typedef struct { uint16_t age; char name[23]; } record;  -> 26 bytes
#[repr(C)]
pub struct Person {
    pub age: u16,
    pub name: [u8; 23],
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

    fn find_table(db: *mut Database, name: *const c_char) -> *mut TableMetadata;

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

fn person(age: u16, name: &str) -> Person {
    let mut p = Person {
        age,
        name: [0; 23],
    };
    fill(&mut p.name, name);
    p
}

fn main() {
    let db_name = CString::new("TEST_DB").unwrap();
    let table = CString::new("PEOPLE").unwrap();

    // 1. create the database (+ catalog.dat)
    let db = unsafe { new_db(db_name.as_ptr()) };
    if db.is_null() {
        eprintln!("new_db failed");
        std::process::exit(1);
    }

    // 2. create the table with its column layout
    let columns = [
        column("age", TYPE_UINT16, 0, 2),
        column("name", TYPE_STRING, 2, 23),
    ];

    let mut meta = unsafe { new_table(db, table.as_ptr(), columns.as_ptr(), columns.len() as u16) };
    if meta.is_null() {
        // already in catalog.dat -> connect to the existing table
        meta = unsafe { find_table(db, table.as_ptr()) };
    }
    if meta.is_null() {
        eprintln!("new_table failed");
        unsafe { close_db(db) };
        std::process::exit(1);
    }
    let meta = unsafe { &*meta };
    println!("table {} ({} columns)", c_field(&meta.name), meta.column_count);

    // 3. insert records
    let people = [person(12, "Sizuka"), person(25, "Nobita"), person(31, "Gian")];
    for p in &people {
        let rc = unsafe {
            db_insert_record(
                db,
                table.as_ptr(),
                p as *const Person as *const c_void,
                std::mem::size_of::<Person>() as u16,
            )
        };
        if rc != 0 {
            eprintln!("insert failed: rc={}", rc);
            unsafe { close_db(db) };
            std::process::exit(1);
        }
    }
    println!("inserted {} rows", people.len());

    // 4. fetch the records back
    let mut batch = ScanBatch {
        records: std::ptr::null_mut(),
        count: 0,
    };

    let rc = unsafe { scan_table(db_name.as_ptr(), table.as_ptr(), &mut batch) };
    if rc != 0 {
        eprintln!("scan_table failed: rc={}", rc);
        unsafe { close_db(db) };
        std::process::exit(1);
    }

    println!("{} rows in {}", batch.count, table.to_str().unwrap());
    if batch.count > 0 {
        let records = unsafe { std::slice::from_raw_parts(batch.records, batch.count as usize) };

        for (i, rec) in records.iter().enumerate() {
            let bytes = unsafe { std::slice::from_raw_parts(rec.data, rec.len as usize) };

            if bytes.len() < 25 {
                println!("[{}] bad record: len={} expected 26", i, rec.len);
                continue;
            }
            let age = u16::from_le_bytes([bytes[0], bytes[1]]);
            println!("[{}] age={} name={}", i, age, c_field(&bytes[2..25]));
        }
    }
    unsafe { scan_batch_free(&mut batch) };

    // 5. fetch the catalogue
    let cat_ptr = unsafe { catalogue_load(db_name.as_ptr()) };
    if cat_ptr.is_null() {
        eprintln!("catalogue_load failed");
        unsafe { close_db(db) };
        std::process::exit(1);
    }

    let cat = unsafe { &*cat_ptr };
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

    unsafe { close_db(db) };
}
