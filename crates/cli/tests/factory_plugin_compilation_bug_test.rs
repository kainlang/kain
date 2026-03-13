use std::path::PathBuf;
/// Bug Condition Exploration Test for Factory Plugin Compilation Failures
///
/// This test MUST FAIL on unfixed code - failure confirms the bug exists.
///
/// Bug Condition: FULLBUILD.bat fails for plugins where the plugin-compilation-pipeline
/// spec shows all tasks complete (88% failure rate - 22 out of 25 plugins).
///
/// Expected Outcome: This test FAILS on unfixed code, documenting counterexamples across:
/// - 13 plugins with parse errors (KAIN compilation stage)
/// - 9 plugins with UE5 build errors (C++ compilation stage)
///
/// DO NOT attempt to fix the test or code when it fails - this test encodes the
/// expected behavior and will validate the fix when it passes after implementation.
use std::process::Command;

/// Represents a plugin compilation failure category
#[derive(Debug, Clone, PartialEq)]
enum FailureCategory {
    ParseError,
    NameCollision { types: Vec<String> },
    MissingGeneratedHeader { header: String },
    FunctionConflict { function: String },
    CppSyntaxError { error_count: usize },
    Unknown,
}

/// Represents a plugin and its expected failure state
#[derive(Debug, Clone)]
struct PluginTestCase {
    name: &'static str,
    path: PathBuf,
    expected_failure: FailureCategory,
}

impl PluginTestCase {
    fn new(name: &'static str, expected_failure: FailureCategory) -> Self {
        let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        path.pop(); // crates
        path.pop(); // Kain
        path.push("Factory");
        path.push(name);

        Self {
            name,
            path,
            expected_failure,
        }
    }
}

/// Get all 22 failing plugins with their expected failure categories
fn get_failing_plugins() -> Vec<PluginTestCase> {
    vec![
        // Parse Errors (13 plugins)
        PluginTestCase::new("VoxelForgePro", FailureCategory::ParseError),
        PluginTestCase::new("AeroTunnel", FailureCategory::ParseError),
        PluginTestCase::new("AlphagenKain", FailureCategory::ParseError),
        PluginTestCase::new("AutoInstancer", FailureCategory::ParseError),
        PluginTestCase::new("CineMasterPro", FailureCategory::ParseError),
        PluginTestCase::new("FluidFlow", FailureCategory::ParseError),
        PluginTestCase::new("Materialize", FailureCategory::ParseError),
        PluginTestCase::new("OmniCam", FailureCategory::ParseError),
        PluginTestCase::new("PSOEliminator", FailureCategory::ParseError),
        PluginTestCase::new("TitanGraph", FailureCategory::ParseError),
        PluginTestCase::new("ToonShaderz", FailureCategory::ParseError),
        PluginTestCase::new("UESculpt", FailureCategory::ParseError),
        PluginTestCase::new("VRAMSniper", FailureCategory::ParseError),
        // Name Collisions (5 plugins)
        PluginTestCase::new(
            "BulkMatte",
            FailureCategory::NameCollision {
                types: vec![
                    "EParameterType".to_string(),
                    "FMaterialInstanceInfo".to_string(),
                ],
            },
        ),
        PluginTestCase::new(
            "Example",
            FailureCategory::NameCollision {
                types: vec!["EQuestStatus".to_string(), "UPhysicsComponent".to_string()],
            },
        ),
        PluginTestCase::new(
            "NarrativeGraph",
            FailureCategory::NameCollision {
                types: vec!["EDialogueNodeType".to_string()],
            },
        ),
        // Missing .generated.h (3 plugins)
        PluginTestCase::new(
            "Cosmos",
            FailureCategory::MissingGeneratedHeader {
                header: "FVec2.generated.h".to_string(),
            },
        ),
        PluginTestCase::new(
            "MetaHumanVAT",
            FailureCategory::MissingGeneratedHeader {
                header: "FVec2.generated.h".to_string(),
            },
        ),
        PluginTestCase::new(
            "TickOptimizer",
            FailureCategory::MissingGeneratedHeader {
                header: "FVec2.generated.h".to_string(),
            },
        ),
        // Function Conflicts (2 plugins)
        PluginTestCase::new(
            "Cinema4DMograph",
            FailureCategory::FunctionConflict {
                function: "Remap".to_string(),
            },
        ),
        PluginTestCase::new(
            "TemporalBlueprint",
            FailureCategory::FunctionConflict {
                function: "ease_in_out".to_string(),
            },
        ),
        // C++ Syntax Errors (1 plugin)
        PluginTestCase::new(
            "UltimateVFX",
            FailureCategory::CppSyntaxError { error_count: 259 },
        ),
        // Unknown Errors (1 plugin)
        PluginTestCase::new("MetaFitter", FailureCategory::Unknown),
    ]
}

/// Run FULLBUILD.bat for a plugin and capture the result
fn run_fullbuild(plugin: &PluginTestCase) -> Result<(), String> {
    let fullbuild_path = plugin.path.join("FULLBUILD.bat");

    if !fullbuild_path.exists() {
        return Err(format!("FULLBUILD.bat not found at {:?}", fullbuild_path));
    }

    let output = Command::new("cmd")
        .args(&["/C", fullbuild_path.to_str().unwrap()])
        .current_dir(&plugin.path)
        .output()
        .map_err(|e| format!("Failed to execute FULLBUILD.bat: {}", e))?;

    if output.status.success() {
        Ok(())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        Err(format!(
            "FULLBUILD.bat failed:\nSTDOUT:\n{}\nSTDERR:\n{}",
            stdout, stderr
        ))
    }
}

/// Categorize the failure based on error output
fn categorize_failure(error_output: &str) -> FailureCategory {
    if error_output.contains("Parse error") {
        FailureCategory::ParseError
    } else if error_output.contains("shares engine name") {
        // Extract type names from error message
        let types = vec![]; // Simplified for now
        FailureCategory::NameCollision { types }
    } else if error_output.contains("Cannot open include file")
        && error_output.contains(".generated.h")
    {
        // Extract header name
        FailureCategory::MissingGeneratedHeader {
            header: "FVec2.generated.h".to_string(), // Simplified
        }
    } else if error_output.contains("conflicts with") && error_output.contains("Function") {
        FailureCategory::FunctionConflict {
            function: "unknown".to_string(), // Simplified
        }
    } else if error_output.contains("missing type specifier")
        || error_output.contains("syntax error")
    {
        FailureCategory::CppSyntaxError { error_count: 0 }
    } else {
        FailureCategory::Unknown
    }
}

#[test]
#[ignore] // Run with: cargo test --test factory_plugin_compilation_bug_test -- --ignored
fn test_bug_condition_all_failing_plugins() {
    println!("\n=== Bug Condition Exploration Test ===");
    println!("Testing that FULLBUILD.bat fails for 22 plugins where spec shows tasks complete");
    println!("EXPECTED OUTCOME: This test FAILS (proves bug exists)\n");

    let failing_plugins = get_failing_plugins();
    let mut counterexamples = Vec::new();
    let mut unexpected_successes = Vec::new();

    for plugin in &failing_plugins {
        println!(
            "Testing plugin: {} (expected: {:?})",
            plugin.name, plugin.expected_failure
        );

        match run_fullbuild(plugin) {
            Ok(()) => {
                // Plugin compiled successfully - this is UNEXPECTED on unfixed code
                println!("  ❌ UNEXPECTED SUCCESS - plugin compiled when it should fail");
                unexpected_successes.push(plugin.name);
            }
            Err(error) => {
                // Plugin failed - this is EXPECTED on unfixed code
                let actual_category = categorize_failure(&error);
                println!("  ✓ EXPECTED FAILURE - {:?}", actual_category);

                counterexamples.push((plugin.name, plugin.expected_failure.clone(), error));
            }
        }
    }

    // Document counterexamples
    println!("\n=== Counterexamples Found ===");
    println!("Total failing plugins: {}", counterexamples.len());

    // Group by category
    let parse_errors: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::ParseError))
        .collect();
    let name_collisions: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::NameCollision { .. }))
        .collect();
    let missing_headers: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::MissingGeneratedHeader { .. }))
        .collect();
    let function_conflicts: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::FunctionConflict { .. }))
        .collect();
    let cpp_errors: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::CppSyntaxError { .. }))
        .collect();
    let unknown_errors: Vec<_> = counterexamples
        .iter()
        .filter(|(_, cat, _)| matches!(cat, FailureCategory::Unknown))
        .collect();

    println!("\nParse Errors ({} plugins):", parse_errors.len());
    for (name, _, _) in &parse_errors {
        println!("  - {}", name);
    }

    println!("\nName Collisions ({} plugins):", name_collisions.len());
    for (name, cat, _) in &name_collisions {
        if let FailureCategory::NameCollision { types } = cat {
            println!("  - {} (types: {:?})", name, types);
        }
    }

    println!(
        "\nMissing .generated.h ({} plugins):",
        missing_headers.len()
    );
    for (name, cat, _) in &missing_headers {
        if let FailureCategory::MissingGeneratedHeader { header } = cat {
            println!("  - {} ({})", name, header);
        }
    }

    println!(
        "\nFunction Conflicts ({} plugins):",
        function_conflicts.len()
    );
    for (name, cat, _) in &function_conflicts {
        if let FailureCategory::FunctionConflict { function } = cat {
            println!("  - {} (function: {})", name, function);
        }
    }

    println!("\nC++ Syntax Errors ({} plugins):", cpp_errors.len());
    for (name, cat, _) in &cpp_errors {
        if let FailureCategory::CppSyntaxError { error_count } = cat {
            println!("  - {} ({} errors)", name, error_count);
        }
    }

    println!("\nUnknown Errors ({} plugins):", unknown_errors.len());
    for (name, _, _) in &unknown_errors {
        println!("  - {}", name);
    }

    if !unexpected_successes.is_empty() {
        println!(
            "\n⚠️  WARNING: {} plugins compiled successfully when they should fail:",
            unexpected_successes.len()
        );
        for name in &unexpected_successes {
            println!("  - {}", name);
        }
    }

    println!("\n=== Test Result ===");
    println!(
        "Failure rate: {}/{} plugins ({}%)",
        counterexamples.len(),
        failing_plugins.len(),
        (counterexamples.len() * 100) / failing_plugins.len()
    );

    // This test MUST FAIL on unfixed code
    // When the bug is fixed, this assertion will pass
    assert_eq!(
        counterexamples.len(),
        0,
        "Bug condition confirmed: {} plugins fail to compile despite spec showing tasks complete. \
        This is EXPECTED on unfixed code. The test will pass after the fix is implemented.",
        counterexamples.len()
    );
}

/// Property-based test: For any plugin in the failing set, FULLBUILD.bat should fail
#[test]
#[ignore]
fn property_all_failing_plugins_should_fail() {
    let failing_plugins = get_failing_plugins();

    for plugin in failing_plugins {
        let result = run_fullbuild(&plugin);

        // On unfixed code, we expect failure
        // On fixed code, we expect success
        match result {
            Ok(()) => {
                // Success means the bug is fixed for this plugin
                println!("✓ Plugin {} compiles successfully (bug fixed)", plugin.name);
            }
            Err(error) => {
                // Failure means the bug still exists for this plugin
                println!("✗ Plugin {} fails: {}", plugin.name, error);
                panic!(
                    "Bug condition exists: Plugin {} fails to compile",
                    plugin.name
                );
            }
        }
    }
}

/// Scoped property test: Parse errors should be detected at KAIN compilation stage
#[test]
#[ignore]
fn property_parse_errors_fail_at_kain_stage() {
    let parse_error_plugins = vec![
        "VoxelForgePro",
        "AeroTunnel",
        "AlphagenKain",
        "AutoInstancer",
        "CineMasterPro",
        "FluidFlow",
        "Materialize",
        "OmniCam",
        "PSOEliminator",
        "TitanGraph",
        "ToonShaderz",
        "UESculpt",
        "VRAMSniper",
    ];

    for plugin_name in parse_error_plugins {
        let plugin = PluginTestCase::new(plugin_name, FailureCategory::ParseError);

        match run_fullbuild(&plugin) {
            Ok(()) => {
                println!("✓ Plugin {} compiles successfully", plugin_name);
            }
            Err(error) => {
                // Verify it's a parse error
                assert!(
                    error.contains("Parse error") || error.contains("kain build"),
                    "Expected parse error for {}, got: {}",
                    plugin_name,
                    error
                );
                panic!("Parse error exists for {}", plugin_name);
            }
        }
    }
}

/// Scoped property test: Name collisions should be caught by Oracle validation
#[test]
#[ignore]
fn property_name_collisions_caught_by_oracle() {
    let collision_plugins = vec![
        ("BulkMatte", vec!["EParameterType", "FMaterialInstanceInfo"]),
        ("Example", vec!["EQuestStatus", "UPhysicsComponent"]),
        ("NarrativeGraph", vec!["EDialogueNodeType"]),
    ];

    for (plugin_name, expected_types) in collision_plugins {
        let plugin = PluginTestCase::new(
            plugin_name,
            FailureCategory::NameCollision {
                types: expected_types.iter().map(|s| s.to_string()).collect(),
            },
        );

        match run_fullbuild(&plugin) {
            Ok(()) => {
                println!(
                    "✓ Plugin {} compiles successfully (Oracle catches collisions)",
                    plugin_name
                );
            }
            Err(error) => {
                // Verify it's a name collision error
                let has_collision = expected_types.iter().any(|t| error.contains(t));
                assert!(
                    has_collision || error.contains("shares engine name"),
                    "Expected name collision for {}, got: {}",
                    plugin_name,
                    error
                );
                panic!("Name collision exists for {}", plugin_name);
            }
        }
    }
}

/// Scoped property test: Missing .generated.h should be fixed by USTRUCT macros
#[test]
#[ignore]
fn property_missing_generated_headers_fixed() {
    let missing_header_plugins = vec!["Cosmos", "MetaHumanVAT", "TickOptimizer"];

    for plugin_name in missing_header_plugins {
        let plugin = PluginTestCase::new(
            plugin_name,
            FailureCategory::MissingGeneratedHeader {
                header: "FVec2.generated.h".to_string(),
            },
        );

        match run_fullbuild(&plugin) {
            Ok(()) => {
                println!(
                    "✓ Plugin {} compiles successfully (USTRUCT macros present)",
                    plugin_name
                );
            }
            Err(error) => {
                // Verify it's a missing header error
                assert!(
                    error.contains(".generated.h") || error.contains("Cannot open include file"),
                    "Expected missing .generated.h for {}, got: {}",
                    plugin_name,
                    error
                );
                panic!("Missing .generated.h for {}", plugin_name);
            }
        }
    }
}
