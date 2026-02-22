// Build script for KAIN-PRO
// Note: Cargo's build.rs runs BEFORE the binary is created,
// so we can't auto-install from here. Use build.bat or build.ps1 instead.

fn main() {
    // Rerun if build.rs changes
    println!("cargo:rerun-if-changed=build.rs");
    
    // Detect release builds
    let profile = std::env::var("PROFILE").unwrap_or_default();
    if profile == "release" {
        println!("cargo:warning=📦 Building release - run build.ps1 for auto-install");
    }
}

