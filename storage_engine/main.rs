use std::ffi::CString;
use std::os::raw::{c_char, c_int};

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

unsafe extern "C" {
    fn scan_table(db_name: *const c_char, table_name: *const c_char, out: *mut ScanBatch) -> c_int;

    fn scan_batch_free(batch: *mut ScanBatch);
}

// record layout from the C engine: typedef struct { uint16_t age; char name[23]; }
// age at byte 0..2, name at byte 2..25, sizeof == 26 (tail byte is padding)
const AGE: usize = 2;
const NAME: usize = 23;
const RECORD_SIZE: usize = AGE + NAME;

fn decode_record(bytes: &[u8]) -> Option<(u16, String)> {
    if bytes.len() < AGE + NAME {
        return None;
    }

    let age = u16::from_le_bytes([bytes[0], bytes[1]]);
    let raw = &bytes[AGE..AGE + NAME];
    let end = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());

    Some((age, String::from_utf8_lossy(&raw[..end]).into_owned()))
}

fn main() {
    let db: CString = CString::new("TEST_DB").unwrap();
    let table: CString = CString::new("TEST_TABLE").unwrap();

    let mut batch = ScanBatch {
        records: std::ptr::null_mut(),
        count: 0,
    };

    let result = unsafe { scan_table(db.as_ptr(), table.as_ptr(), &mut batch) };

    if result != 0 {
        panic!("scan_table failed: rc={}", result);
    }

    println!("records: {}", batch.count);

    if batch.count > 0 {
        let records = unsafe { std::slice::from_raw_parts(batch.records, batch.count as usize) };

        for (i, rec) in records.iter().enumerate() {
            let bytes = unsafe { std::slice::from_raw_parts(rec.data, rec.len as usize) };

            match decode_record(bytes) {
                Some((age, name)) => println!("[{}] age={} name={}", i, age, name),
                None => println!("[{}] bad record: len={} expected {}", i, rec.len, RECORD_SIZE),
            }
        }
    }

    unsafe { scan_batch_free(&mut batch) };
    println!("freed: count={} records={:?}", batch.count, batch.records);
}
