fn main() {
    // libdatabase.so lives in <repo>/lib (copied there by the top level Makefile)
    let dir = std::fs::canonicalize("../lib").unwrap_or_else(|_| "../lib".into());

    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=dylib=database");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", dir.display());

    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=../lib/libdatabase.so");
}
