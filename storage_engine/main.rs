use std::ffi::CString;
use std::os::raw::{c_char, c_int};

#[repr(C)]
pub struct RawRecord {
    pub data: *mut u8,
    pub len: u32,
}

#[repr(C)]
pub struct ScanBatch {
    pub records: *mut RawRecord,
    pub count: u32,
}

unsafe extern "C" {
    fn db_scan(db_name: *const c_char, table_name: *const c_char, out: *mut ScanBatch) -> c_int;

    fn free_scan_batch(batch: *mut ScanBatch);
}

let db = CString::new("TEST_DB").unwrap();
let table = CString::new("users").unwrap();

let mut batch = ScanBatch {
    records: std::ptr::null_mut(),
    count: 0,
};

let result = unsafe {
    db_scan(
        db.as_ptr(),
        table.as_ptr(),
        &mut batch,
    )
};

if result != 0 {
    panic!("db_scan failed");
}

println!("records: {}", batch.count);
