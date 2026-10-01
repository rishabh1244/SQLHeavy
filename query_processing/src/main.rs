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
}
