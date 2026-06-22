//! Integration tests for the vcpkg source-owned manifest pipeline.
//!
//! Tests that require actual vcpkg installation are gated behind
//! `#[ignore]` or the `KAIN_VCPKG_INTEGRATION_TESTS` env var.

use std::path::PathBuf;

// ── Test 1: version_gt comparison ──────────────────────────────────

#[test]
fn test_version_gt_handles_multi_digit_segments() {
    // Reuse the version_gt from vcpkg_plan (already tested in unit tests)
    // This integration test verifies the function is publicly accessible.
    assert!(kain_c_ffi::vcpkg_plan::version_gt("3.10.0", "3.9.0"));
    assert!(!kain_c_ffi::vcpkg_plan::version_gt("3.9.0", "3.10.0"));
    assert!(kain_c_ffi::vcpkg_plan::version_gt("10.0.0", "9.99.99"));
}

// ── Test 2: vcpkg plan deduplication with version conflicts ──────

#[test]
fn test_vcpkg_plan_deduplicates_by_highest_version() {
    let entries = vec![
        (
            "openssl/ssl.h".to_string(),
            "3.0.8".to_string(),
            PathBuf::from("a.kn"),
        ),
        (
            "openssl/err.h".to_string(),
            "3.10.1".to_string(),
            PathBuf::from("b.kn"),
        ),
        (
            "openssl/crypto.h".to_string(),
            "3.9.0".to_string(),
            PathBuf::from("c.kn"),
        ),
    ];
    let plan = kain_c_ffi::vcpkg_plan::build_plan(&entries).unwrap();
    assert_eq!(plan.dependencies.len(), 1);
    // 3.10.1 should win over 3.0.8 and 3.9.0
    assert_eq!(plan.dependencies[0].version, "3.10.1");
}

// ── Test 3: vcpkg plan handles multiple ports ──────────────────────

#[test]
fn test_vcpkg_plan_multiple_ports() {
    let entries = vec![
        (
            "sqlite3.h".to_string(),
            "3.45.0".to_string(),
            PathBuf::from("a.kn"),
        ),
        (
            "openssl/ssl.h".to_string(),
            "3.0.8".to_string(),
            PathBuf::from("b.kn"),
        ),
        (
            "zlib.h".to_string(),
            "1.3.1".to_string(),
            PathBuf::from("c.kn"),
        ),
    ];
    let plan = kain_c_ffi::vcpkg_plan::build_plan(&entries).unwrap();
    assert_eq!(plan.dependencies.len(), 3);
}

// ── Test 4: vcpkg JSON generation ──────────────────────────────────

#[test]
fn test_vcpkg_json_emits_version_constraint() {
    let plan = kain_c_ffi::vcpkg_plan::build_plan(&[(
        "sqlite3.h".to_string(),
        "3.45.0".to_string(),
        PathBuf::from("main.kn"),
    )])
    .unwrap();
    let json = plan.to_vcpkg_json(None);
    assert!(json.contains("\"name\": \"sqlite3\""));
    assert!(json.contains("\"version>=\": \"3.45.0\""));
    assert!(json.contains("\"name\": \"kain-project\""));
}

// ── Test 5: vcpkg root resolution ──────────────────────────────────

#[test]
fn test_vcpkg_root_default() {
    let root = kain_c_ffi::vcpkg::resolve_vcpkg_root();
    // Default goes to ~/.kain/vcpkg/
    let root_str = root.to_string_lossy().to_string();
    assert!(root_str.contains(".kain") || root_str.contains("kain"));
    assert!(root_str.contains("vcpkg"));
}

// ── Test 6: port override heuristic ────────────────────────────────

#[test]
fn test_port_override_known_exceptions() {
    // nlohmann/json.hpp is a well-known exception
    assert_eq!(
        kain_c_ffi::port_overrides::header_to_port("nlohmann/json.hpp"),
        "nlohmann-json"
    );
    // openssl is the standard case
    assert_eq!(
        kain_c_ffi::port_overrides::header_to_port("openssl/ssl.h"),
        "openssl"
    );
    // bare header gets first segment
    assert_eq!(
        kain_c_ffi::port_overrides::header_to_port("sqlite3.h"),
        "sqlite3"
    );
}
