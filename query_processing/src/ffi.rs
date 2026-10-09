// ffi.rs — the only module that talks to storage_engine/libdatabase.so.
// Raw C types, extern declarations and every unsafe block live here;
// main.rs uses only the safe wrappers below.

use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};

const TYPE_UINT16: c_int = 0;
const TYPE_INT32: c_int = 1;
const TYPE_INT64: c_int = 2;
const TYPE_FLOAT64: c_int = 3;
const TYPE_STRING: c_int = 4;

#[repr(C)]
struct RawRecord {
    data: *const u8,
    len: u32,
}

#[repr(C)]
struct ScanBatch {
    records: *mut RawRecord,
    count: u32,
}

#[repr(C)]
struct ColumnMetadata {
    name: [u8; 32],
    type_: c_int,
    offset: u16,
    length: u16,
}

#[repr(C)]
struct TableMetadata {
    name: [u8; 64],
    column_count: u16,
    columns: [ColumnMetadata; 32],
}

#[repr(C)]
struct Catalogue {
    table_count: u16,
    tables: [TableMetadata; 128],
}

#[repr(C)]
struct RawDatabase {
    _private: [u8; 0],
}

// typedef struct { int32_t id; char name[16]; } user;  -> 20 bytes
#[repr(C)]
struct User {
    id: i32,
    name: [u8; 16],
}

unsafe extern "C" {
    fn new_db(name: *const c_char) -> *mut RawDatabase;
    fn close_db(db: *mut RawDatabase);

    fn new_table(
        db: *mut RawDatabase,
        name: *const c_char,
        columns: *const ColumnMetadata,
        column_count: u16,
    ) -> *mut TableMetadata;

    fn find_table(db: *mut RawDatabase, name: *const c_char) -> *mut TableMetadata;

    fn db_insert_record(
        db: *mut RawDatabase,
        table_name: *const c_char,
        data: *const c_void,
        length: u16,
    ) -> c_int;

    fn scan_table(db_name: *const c_char, table_name: *const c_char, out: *mut ScanBatch) -> c_int;
    fn scan_batch_free(batch: *mut ScanBatch);

    fn catalogue_load(db_name: *const c_char) -> *mut Catalogue;
    fn catalogue_free(catalogue: *mut Catalogue);
}

// ---------- safe API ----------

/// Column type codes accepted by the C engine.
#[derive(Clone, Copy)]
#[allow(dead_code)] // not every variant is used by main yet
pub enum ColType {
    U16,
    I32,
    I64,
    F64,
    Str,
}

impl ColType {
    fn code(self) -> c_int {
        match self {
            ColType::U16 => TYPE_UINT16,
            ColType::I32 => TYPE_INT32,
            ColType::I64 => TYPE_INT64,
            ColType::F64 => TYPE_FLOAT64,
            ColType::Str => TYPE_STRING,
        }
    }
}

pub struct ColumnSpec {
    pub name: String,
    pub type_: ColType,
    pub offset: u16,
    pub length: u16,
}

impl ColumnSpec {
    pub fn new(name: &str, type_: ColType, offset: u16, length: u16) -> Self {
        Self {
            name: name.to_string(),
            type_,
            offset,
            length,
        }
    }
}

pub struct UserRow {
    pub id: i32,
    pub name: String,
}

pub struct ColumnInfo {
    pub name: String,
    pub type_: i32,
    pub offset: u16,
    pub length: u16,
}

pub struct TableInfo {
    pub name: String,
    pub columns: Vec<ColumnInfo>,
}

/// Open handle; closes the C database when dropped.
pub struct Database {
    ptr: *mut RawDatabase,
    name: String,
}

impl Database {
    pub fn open(name: &str) -> Result<Self, String> {
        let c_name = CString::new(name).map_err(|_| "database name contains NUL".to_string())?;
        let ptr = unsafe { new_db(c_name.as_ptr()) };
        if ptr.is_null() {
            return Err(format!("new_db({name}) failed"));
        }
        Ok(Self {
            ptr,
            name: name.to_string(),
        })
    }

    /// Creates the table unless it is already catalogued.
    pub fn create_table(&self, name: &str, columns: &[ColumnSpec]) -> Result<(), String> {
        let c_name = CString::new(name).map_err(|_| "table name contains NUL".to_string())?;
        if !unsafe { find_table(self.ptr, c_name.as_ptr()) }.is_null() {
            return Ok(());
        }
        let raw: Vec<ColumnMetadata> = columns.iter().map(to_metadata).collect();
        let ptr = unsafe { new_table(self.ptr, c_name.as_ptr(), raw.as_ptr(), raw.len() as u16) };
        if ptr.is_null() {
            return Err(format!("new_table({name}) failed"));
        }
        Ok(())
    }

    pub fn insert_user(&self, table: &str, id: i32, name: &str) -> Result<(), String> {
        let c_table = CString::new(table).map_err(|_| "table name contains NUL".to_string())?;
        let user = pack_user(id, name);
        let rc = unsafe {
            db_insert_record(
                self.ptr,
                c_table.as_ptr(),
                &user as *const User as *const c_void,
                std::mem::size_of::<User>() as u16,
            )
        };
        if rc != 0 {
            return Err(format!("db_insert_record into {table} failed: rc={rc}"));
        }
        Ok(())
    }

    pub fn scan_users(&self, table: &str) -> Result<Vec<UserRow>, String> {
        let c_table = CString::new(table).map_err(|_| "table name contains NUL".to_string())?;
        let c_db = CString::new(self.name.as_str())
            .map_err(|_| "database name contains NUL".to_string())?;

        let cat_guard = load_catalogue(&c_db)?;
        let cat = unsafe { &*cat_guard.ptr };
        let meta = lookup_table(cat, table)
            .ok_or_else(|| format!("no catalogue entry for {table}"))?;
        let id_col =
            column_by_name(meta, "id").ok_or_else(|| format!("{table} has no id column"))?;
        let name_col =
            column_by_name(meta, "name").ok_or_else(|| format!("{table} has no name column"))?;

        let mut batch = ScanGuard::new();
        let rc = unsafe { scan_table(c_db.as_ptr(), c_table.as_ptr(), batch.as_mut_ptr()) };
        if rc != 0 {
            return Err(format!("scan_table({}.{}) failed: rc={rc}", self.name, table));
        }

        let inner = &batch.0;
        if inner.records.is_null() || inner.count == 0 {
            return Ok(Vec::new());
        }

        let records = unsafe { std::slice::from_raw_parts(inner.records, inner.count as usize) };
        let mut rows = Vec::with_capacity(records.len());
        for rec in records {
            let bytes = unsafe { std::slice::from_raw_parts(rec.data, rec.len as usize) };
            rows.push(UserRow {
                id: read_i32(bytes, id_col)?,
                name: read_str(bytes, name_col)?,
            });
        }
        Ok(rows)
    }

    pub fn list_tables(&self) -> Result<Vec<TableInfo>, String> {
        let c_db = CString::new(self.name.as_str())
            .map_err(|_| "database name contains NUL".to_string())?;
        let cat_guard = load_catalogue(&c_db)?;
        let cat = unsafe { &*cat_guard.ptr };

        Ok((0..cat.table_count as usize)
            .map(|i| {
                let t = &cat.tables[i];
                TableInfo {
                    name: c_field(&t.name),
                    columns: (0..t.column_count as usize)
                        .map(|j| {
                            let c = &t.columns[j];
                            ColumnInfo {
                                name: c_field(&c.name),
                                type_: c.type_,
                                offset: c.offset,
                                length: c.length,
                            }
                        })
                        .collect(),
                }
            })
            .collect())
    }
}

impl Drop for Database {
    fn drop(&mut self) {
        unsafe { close_db(self.ptr) };
    }
}

// ---------- RAII guards for C allocations ----------

struct ScanGuard(ScanBatch);

impl ScanGuard {
    fn new() -> Self {
        Self(ScanBatch {
            records: std::ptr::null_mut(),
            count: 0,
        })
    }

    fn as_mut_ptr(&mut self) -> *mut ScanBatch {
        &mut self.0
    }
}

impl Drop for ScanGuard {
    fn drop(&mut self) {
        unsafe { scan_batch_free(&mut self.0) };
    }
}

struct CatalogueGuard {
    ptr: *mut Catalogue,
}

impl Drop for CatalogueGuard {
    fn drop(&mut self) {
        unsafe { catalogue_free(self.ptr) };
    }
}

fn load_catalogue(db_name: &CString) -> Result<CatalogueGuard, String> {
    let ptr = unsafe { catalogue_load(db_name.as_ptr()) };
    if ptr.is_null() {
        return Err("catalogue_load failed".to_string());
    }
    Ok(CatalogueGuard { ptr })
}

// ---------- byte/layout helpers ----------

fn c_field(bytes: &[u8]) -> String {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end]).into_owned()
}

fn fill<const N: usize>(dst: &mut [u8; N], src: &str) {
    let bytes = src.as_bytes();
    let n = bytes.len().min(N - 1);
    dst[..n].copy_from_slice(&bytes[..n]);
}

fn to_metadata(spec: &ColumnSpec) -> ColumnMetadata {
    let mut col = ColumnMetadata {
        name: [0; 32],
        type_: spec.type_.code(),
        offset: spec.offset,
        length: spec.length,
    };
    fill(&mut col.name, &spec.name);
    col
}

fn pack_user(id: i32, name: &str) -> User {
    let mut u = User { id, name: [0; 16] };
    fill(&mut u.name, name);
    u
}

fn lookup_table<'a>(cat: &'a Catalogue, name: &str) -> Option<&'a TableMetadata> {
    cat.tables
        .iter()
        .take(cat.table_count as usize)
        .find(|t| c_field(&t.name) == name)
}

fn column_by_name<'a>(meta: &'a TableMetadata, name: &str) -> Option<&'a ColumnMetadata> {
    meta.columns
        .iter()
        .take(meta.column_count as usize)
        .find(|c| c_field(&c.name) == name)
}

fn read_i32(record: &[u8], col: &ColumnMetadata) -> Result<i32, String> {
    let start = col.offset as usize;
    let raw = record
        .get(start..start + 4)
        .ok_or_else(|| format!("record too short for column at offset {start}"))?;
    Ok(i32::from_le_bytes(raw.try_into().unwrap()))
}

fn read_str(record: &[u8], col: &ColumnMetadata) -> Result<String, String> {
    let start = col.offset as usize;
    let raw = record
        .get(start..start + col.length as usize)
        .ok_or_else(|| format!("record too short for column at offset {start}"))?;
    Ok(c_field(raw))
}
