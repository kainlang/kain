use std::{env, path::PathBuf};

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=vendor");
    println!("cargo:rerun-if-env-changed=CARGO_FEATURE_OODLE");

    let source_path =
        PathBuf::from(&env::var_os("CARGO_MANIFEST_DIR").expect("Failed to read source path"));

    if env::var_os("CARGO_FEATURE_OODLE").is_some() {
        let vendor_path = source_path.join("vendor");
        println!("cargo:rustc-link-search={}", vendor_path.to_str().unwrap());
    }
}
