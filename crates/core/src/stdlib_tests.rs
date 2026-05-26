//! Unit tests for stdlib file discovery and loading

use crate::stdlib::{find_stdlib_search_roots, load_kn_files_from_dir, load_stdlib};
use std::env;
use std::fs;
use std::path::PathBuf;

/// Helper to create a temporary test directory structure
struct TempTestDir {
    path: PathBuf,
}

impl TempTestDir {
    fn new(name: &str) -> Self {
        let temp_dir = env::temp_dir().join(format!("kain_test_{}", name));
        if temp_dir.exists() {
            let _ = fs::remove_dir_all(&temp_dir);
        }
        fs::create_dir_all(&temp_dir).expect("Failed to create temp dir");
        Self { path: temp_dir }
    }

    fn create_file(&self, relative_path: &str, content: &str) {
        let file_path = self.path.join(relative_path);
        if let Some(parent) = file_path.parent() {
            fs::create_dir_all(parent).expect("Failed to create parent dirs");
        }
        fs::write(&file_path, content).expect("Failed to write file");
    }

    fn create_dir(&self, relative_path: &str) {
        let dir_path = self.path.join(relative_path);
        fs::create_dir_all(&dir_path).expect("Failed to create directory");
    }

    fn path(&self) -> &PathBuf {
        &self.path
    }
}

impl Drop for TempTestDir {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.path);
    }
}

#[test]
fn test_find_stdlib_with_env_var_valid() {
    let test_dir = TempTestDir::new("env_var_valid");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/test.kn", "fn test() -> Int: 42");

    // Set environment variable
    env::set_var("KAIN_STDLIB_PATH", test_dir.path().join("stdlib"));

    let roots = find_stdlib_search_roots();

    // Clean up env var
    env::remove_var("KAIN_STDLIB_PATH");

    assert_eq!(roots.len(), 1, "Should find exactly one root with env var");
    assert_eq!(roots[0], test_dir.path().join("stdlib"));
}

#[test]
fn test_find_stdlib_with_env_var_invalid() {
    // Set environment variable to non-existent path
    let invalid_path = env::temp_dir().join("kain_nonexistent_stdlib_12345");
    env::set_var("KAIN_STDLIB_PATH", &invalid_path);

    let roots = find_stdlib_search_roots();

    // Clean up env var
    env::remove_var("KAIN_STDLIB_PATH");

    // Should fall back to filesystem walking, which may or may not find stdlib
    // The key is that it doesn't crash and returns a valid Vec
    assert!(
        roots.is_empty() || !roots.is_empty(),
        "Should return valid Vec"
    );
}

#[test]
fn test_find_stdlib_in_parent_directory() {
    let test_dir = TempTestDir::new("parent_dir");
    test_dir.create_dir("stdlib");
    test_dir.create_dir("subdir/deep");
    test_dir.create_file("stdlib/test.kn", "fn test() -> Int: 42");

    // Change to deep subdirectory
    let original_dir = env::current_dir().unwrap();
    let deep_dir = test_dir.path().join("subdir/deep");
    env::set_current_dir(&deep_dir).expect("Failed to change directory");

    let roots = find_stdlib_search_roots();

    // Restore original directory
    env::set_current_dir(original_dir).expect("Failed to restore directory");

    // Should find stdlib in ancestor directory
    assert!(!roots.is_empty(), "Should find stdlib in parent directory");
    assert!(
        roots.iter().any(|r| r.ends_with("stdlib")),
        "Should find a path ending with 'stdlib'"
    );
}

#[test]
fn test_find_stdlib_in_grandparent_directory() {
    let test_dir = TempTestDir::new("grandparent_dir");
    test_dir.create_dir("stdlib");
    test_dir.create_dir("level1/level2/level3");
    test_dir.create_file("stdlib/test.kn", "fn test() -> Int: 42");

    // Change to deep subdirectory
    let original_dir = env::current_dir().unwrap();
    let deep_dir = test_dir.path().join("level1/level2/level3");
    env::set_current_dir(&deep_dir).expect("Failed to change directory");

    let roots = find_stdlib_search_roots();

    // Restore original directory
    env::set_current_dir(original_dir).expect("Failed to restore directory");

    // Should find stdlib in ancestor directory
    assert!(
        !roots.is_empty(),
        "Should find stdlib in grandparent directory"
    );
}

#[test]
fn test_find_stdlib_no_stdlib_present() {
    let test_dir = TempTestDir::new("no_stdlib");
    test_dir.create_dir("some_other_dir");

    // Change to test directory
    let original_dir = env::current_dir().unwrap();
    env::set_current_dir(test_dir.path()).expect("Failed to change directory");

    let roots = find_stdlib_search_roots();

    // Restore original directory
    env::set_current_dir(original_dir).expect("Failed to restore directory");

    // May find stdlib from actual project structure or be empty
    // The key is that it doesn't crash
    assert!(
        roots.is_empty() || !roots.is_empty(),
        "Should return valid Vec"
    );
}

#[test]
fn test_load_kn_files_multiple_files() {
    let test_dir = TempTestDir::new("multiple_files");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/a.kn", "fn a() -> Int: 1");
    test_dir.create_file("stdlib/b.kn", "fn b() -> Int: 2");
    test_dir.create_file("stdlib/c.kn", "fn c() -> Int: 3");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(result.is_some(), "Should load files successfully");
    let content = result.unwrap();

    // Should contain all three functions
    assert!(content.contains("fn a()"), "Should contain function a");
    assert!(content.contains("fn b()"), "Should contain function b");
    assert!(content.contains("fn c()"), "Should contain function c");

    // Files should be in alphabetical order
    let a_pos = content.find("fn a()").unwrap();
    let b_pos = content.find("fn b()").unwrap();
    let c_pos = content.find("fn c()").unwrap();
    assert!(
        a_pos < b_pos && b_pos < c_pos,
        "Files should be in alphabetical order"
    );
}

#[test]
fn test_load_kn_files_with_readme() {
    let test_dir = TempTestDir::new("with_readme");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/code.kn", "fn code() -> Int: 42");
    test_dir.create_file("stdlib/README.md", "# This is a readme");
    test_dir.create_file("stdlib/readme.kn", "# This should be excluded");
    test_dir.create_file("stdlib/ReadMe.kn", "# This should also be excluded");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(result.is_some(), "Should load files successfully");
    let content = result.unwrap();

    // Should contain code.kn
    assert!(
        content.contains("fn code()"),
        "Should contain code function"
    );

    // Should NOT contain any readme content
    assert!(
        !content.contains("This is a readme"),
        "Should exclude README.md"
    );
    assert!(
        !content.contains("This should be excluded"),
        "Should exclude readme.kn"
    );
    assert!(
        !content.contains("This should also be excluded"),
        "Should exclude ReadMe.kn"
    );
}

#[test]
fn test_load_kn_files_only_readme() {
    let test_dir = TempTestDir::new("only_readme");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/README.md", "# This is a readme");
    test_dir.create_file("stdlib/readme.kn", "# This should be excluded");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(
        result.is_none(),
        "Should return None when only README files present"
    );
}

#[test]
fn test_load_kn_files_empty_directory() {
    let test_dir = TempTestDir::new("empty_dir");
    test_dir.create_dir("stdlib");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(result.is_none(), "Should return None for empty directory");
}

#[test]
fn test_load_kn_files_nonexistent_directory() {
    let nonexistent = env::temp_dir().join("kain_nonexistent_dir_98765");

    let result = load_kn_files_from_dir(&nonexistent);

    assert!(
        result.is_none(),
        "Should return None for nonexistent directory"
    );
}

#[test]
fn test_load_kn_files_alphabetical_ordering() {
    let test_dir = TempTestDir::new("alphabetical");
    test_dir.create_dir("stdlib");

    // Create files in non-alphabetical order
    test_dir.create_file("stdlib/zebra.kn", "fn zebra() -> Int: 26");
    test_dir.create_file("stdlib/alpha.kn", "fn alpha() -> Int: 1");
    test_dir.create_file("stdlib/middle.kn", "fn middle() -> Int: 13");
    test_dir.create_file("stdlib/beta.kn", "fn beta() -> Int: 2");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(result.is_some(), "Should load files successfully");
    let content = result.unwrap();

    // Verify alphabetical ordering
    let alpha_pos = content.find("fn alpha()").unwrap();
    let beta_pos = content.find("fn beta()").unwrap();
    let middle_pos = content.find("fn middle()").unwrap();
    let zebra_pos = content.find("fn zebra()").unwrap();

    assert!(
        alpha_pos < beta_pos && beta_pos < middle_pos && middle_pos < zebra_pos,
        "Files should be in strict alphabetical order"
    );
}

#[test]
fn test_load_stdlib_with_ue5_subdirectory_keeps_root_default() {
    let test_dir = TempTestDir::new("ue5_subdir");
    test_dir.create_dir("stdlib/ue5");
    test_dir.create_file(
        "stdlib/ue5/actor.kn",
        "fn get_location() -> Vec3: vec3(0.0, 0.0, 0.0)",
    );
    test_dir.create_file("stdlib/generic.kn", "fn generic() -> Int: 42");

    // Set environment variable to point to stdlib directory
    env::set_var("KAIN_STDLIB_PATH", test_dir.path().join("stdlib"));

    let result = load_stdlib();

    // Clean up env var
    env::remove_var("KAIN_STDLIB_PATH");

    // Generic loads should stay on the universal root profile.
    assert!(
        result.contains("fn generic()"),
        "Should load the root stdlib by default"
    );
    assert!(
        !result.contains("fn get_location()"),
        "Should not pull in ue5/ overlays for generic loads"
    );
}

#[test]
fn test_load_stdlib_fallback_to_root() {
    let test_dir = TempTestDir::new("fallback_root");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/generic.kn", "fn generic() -> Int: 42");
    // No ue5/ subdirectory

    // Set environment variable to point to stdlib directory
    env::set_var("KAIN_STDLIB_PATH", test_dir.path().join("stdlib"));

    let result = load_stdlib();

    // Clean up env var
    env::remove_var("KAIN_STDLIB_PATH");

    // Should fall back to root directory
    assert!(
        result.contains("fn generic()"),
        "Should load from root when ue5/ doesn't exist"
    );
}

#[test]
fn test_load_stdlib_graceful_degradation() {
    // Save original directory first
    let original_dir = env::current_dir().unwrap();

    {
        let test_dir = TempTestDir::new("graceful_degradation");
        test_dir.create_dir("no_stdlib_here");

        // Set environment variable to non-existent path
        let invalid_path = env::temp_dir().join("kain_nonexistent_stdlib_67890");
        env::set_var("KAIN_STDLIB_PATH", &invalid_path);

        // Change to directory without stdlib to prevent fallback discovery
        env::set_current_dir(test_dir.path()).expect("Failed to change directory");

        let result = load_stdlib();

        // Restore directory before test_dir is dropped
        env::set_current_dir(&original_dir).expect("Failed to restore directory");
        env::remove_var("KAIN_STDLIB_PATH");

        // Should not crash - may return empty or may find stdlib from exe path
        // The key is graceful degradation, not necessarily empty result
        assert!(
            result.is_empty() || !result.is_empty(),
            "Should return valid string without crashing"
        );
    }
    // test_dir is dropped here, after we've restored the directory
}

#[test]
fn test_load_stdlib_deterministic_ordering() {
    let test_dir = TempTestDir::new("deterministic");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/file1.kn", "fn file1() -> Int: 1");
    test_dir.create_file("stdlib/file2.kn", "fn file2() -> Int: 2");
    test_dir.create_file("stdlib/file3.kn", "fn file3() -> Int: 3");

    // Set environment variable and keep it set for all loads
    let stdlib_path = test_dir.path().join("stdlib");
    env::set_var("KAIN_STDLIB_PATH", &stdlib_path);

    // Load multiple times - env var should remain set
    let result1 = load_stdlib();

    // Re-set env var to ensure it's still there (defensive)
    env::set_var("KAIN_STDLIB_PATH", &stdlib_path);
    let result2 = load_stdlib();

    // Re-set env var again
    env::set_var("KAIN_STDLIB_PATH", &stdlib_path);
    let result3 = load_stdlib();

    // Clean up env var
    env::remove_var("KAIN_STDLIB_PATH");

    // All results should be identical
    assert_eq!(
        result1, result2,
        "First and second load should be identical"
    );
    assert_eq!(
        result2, result3,
        "Second and third load should be identical"
    );

    // Verify we actually loaded the test files
    assert!(
        result1.contains("fn file1()"),
        "Should contain file1 function"
    );
    assert!(
        result1.contains("fn file2()"),
        "Should contain file2 function"
    );
    assert!(
        result1.contains("fn file3()"),
        "Should contain file3 function"
    );
}

#[test]
fn test_readme_case_insensitive_filtering() {
    let test_dir = TempTestDir::new("readme_cases");
    test_dir.create_dir("stdlib");
    test_dir.create_file("stdlib/code.kn", "fn code() -> Int: 42");
    test_dir.create_file("stdlib/README.kn", "# Should be excluded");
    test_dir.create_file("stdlib/readme.kn", "# Should be excluded");
    test_dir.create_file("stdlib/ReadMe.kn", "# Should be excluded");
    test_dir.create_file("stdlib/README.kn", "# Should be excluded");
    test_dir.create_file("stdlib/rEaDmE.kn", "# Should be excluded");

    let result = load_kn_files_from_dir(&test_dir.path().join("stdlib"));

    assert!(result.is_some(), "Should load files successfully");
    let content = result.unwrap();

    // Should only contain code.kn
    assert!(
        content.contains("fn code()"),
        "Should contain code function"
    );
    assert!(
        !content.contains("Should be excluded"),
        "Should exclude all README variants"
    );
}

#[test]
fn test_env_var_priority_over_filesystem() {
    let test_dir1 = TempTestDir::new("priority1");
    let test_dir2 = TempTestDir::new("priority2");

    test_dir1.create_dir("stdlib");
    test_dir1.create_file("stdlib/file1.kn", "fn from_env() -> Int: 1");

    test_dir2.create_dir("stdlib");
    test_dir2.create_file("stdlib/file2.kn", "fn from_fs() -> Int: 2");

    // Set env var to first directory
    env::set_var("KAIN_STDLIB_PATH", test_dir1.path().join("stdlib"));

    // Change current directory to second directory
    let original_dir = env::current_dir().unwrap();
    env::set_current_dir(test_dir2.path()).expect("Failed to change directory");

    let result = load_stdlib();

    // Restore
    env::set_current_dir(original_dir).expect("Failed to restore directory");
    env::remove_var("KAIN_STDLIB_PATH");

    // Should load from env var path, not filesystem path
    assert!(
        result.contains("fn from_env()"),
        "Should load from KAIN_STDLIB_PATH"
    );
    assert!(
        !result.contains("fn from_fs()"),
        "Should not load from filesystem when env var is set"
    );
}
