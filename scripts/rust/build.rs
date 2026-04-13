// Build script note for the Windows wrappers.
// Cargo build scripts run before the binary exists, so installation has to
// happen in the wrapper layer (`scripts/windows/build.bat` or `scripts/windows/cb.ps1`).

fn main() {
    // Rerun if this helper changes
    println!("cargo:rerun-if-changed=build.rs");

    // Detect release builds
    let profile = std::env::var("PROFILE").unwrap_or_default();
    if profile == "release" {
        println!("cargo:warning=📦 Building release - use scripts/windows/build.bat for auto-install");
    }
}
