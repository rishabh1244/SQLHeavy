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

/*

fn main() {
    let path = CString::new("mydb").unwrap();

    let db = unsafe { db_open(path.as_ptr()) };

    let table = CString::new("users").unwrap();

    let mut request = ScanRequest {
        table_name: table.as_ptr(),
    };

    let mut rows = vec![
        Row {
            data: std::ptr::null(),
            len: 0,
        };
        100
    ];

    let mut count = 0;

    let result = unsafe { db_scan(db, &mut request, rows.as_mut_ptr(), rows.len(), &mut count) };

    assert_eq!(result, 0);

    for row in &rows[..count] {
        let bytes = unsafe { std::slice::from_raw_parts(row.data as *const u8, row.len) };

        println!("{}", String::from_utf8_lossy(bytes));
    }

    unsafe {
        db_close(db);
    }
}*/
