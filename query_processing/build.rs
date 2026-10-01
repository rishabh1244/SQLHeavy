use std::ffi::{CString, c_char};

#[repr(C)]
pub struct Database {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ScanRequest {
    pub table_name: *const c_char,
}

#[repr(C)]
pub struct Row {
    pub data: *const c_char,
    pub len: usize,
}

unsafe extern "C" {
    fn db_open(path: *const c_char) -> *mut Database;

    fn db_scan(
        db: *mut Database,
        request: *mut ScanRequest,
        rows: *mut Row,
        capacity: usize,
        count: *mut usize,
    ) -> i32;

    fn db_close(db: *mut Database);
}
