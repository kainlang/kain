use serde::Deserialize;
use serde_json::Value;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, Deserialize)]
struct ValidationManifest {
    name: String,
    #[serde(default)]
    case_root: Option<String>,
    #[serde(default)]
    command: Option<CommandSpec>,
    assertions: Vec<AssertionSpec>,
}

#[derive(Debug, Deserialize)]
struct CommandSpec {
    program: String,
    #[serde(default)]
    args: Vec<String>,
    #[serde(default)]
    cwd: Option<String>,
}

#[derive(Debug, Deserialize)]
struct AssertionSpec {
    kind: String,
    #[serde(default)]
    file: Option<String>,
    #[serde(default)]
    pattern: Option<String>,
    #[serde(default)]
    include: Option<String>,
    #[serde(default)]
    module_name: Option<String>,
    #[serde(default)]
    uplugin_file: Option<String>,
    #[serde(default)]
    build_cs_file: Option<String>,
}

fn fixtures_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join("codegen_validation")
}

fn read_manifest(path: &Path) -> ValidationManifest {
    let raw = fs::read_to_string(path)
        .unwrap_or_else(|e| panic!("failed to read manifest {}: {}", path.display(), e));
    toml::from_str(&raw)
        .unwrap_or_else(|e| panic!("failed to parse manifest {}: {}", path.display(), e))
}

fn resolve_command_program(program: &str) -> String {
    if program.eq_ignore_ascii_case("kain") {
        if let Ok(bin) = std::env::var("CARGO_BIN_EXE_kain") {
            return bin;
        }
    }
    program.to_string()
}

fn run_optional_command(manifest: &ValidationManifest, case_root: &Path) {
    let Some(spec) = &manifest.command else {
        return;
    };

    let program = resolve_command_program(&spec.program);
    let mut cmd = Command::new(&program);
    cmd.args(&spec.args);

    let cwd = spec
        .cwd
        .as_ref()
        .map(|p| case_root.join(p))
        .unwrap_or_else(|| case_root.to_path_buf());
    cmd.current_dir(&cwd);

    let output = cmd
        .output()
        .unwrap_or_else(|e| panic!("failed to run command {} in {}: {}", program, cwd.display(), e));

    if !output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        panic!(
            "command failed for case {}\nprogram: {}\ncwd: {}\nstdout:\n{}\nstderr:\n{}",
            manifest.name,
            program,
            cwd.display(),
            stdout,
            stderr
        );
    }
}

fn require_field(spec: &AssertionSpec, field: &str) -> String {
    match field {
        "file" => spec.file.clone(),
        "pattern" => spec.pattern.clone(),
        "include" => spec.include.clone(),
        "module_name" => spec.module_name.clone(),
        "uplugin_file" => spec.uplugin_file.clone(),
        "build_cs_file" => spec.build_cs_file.clone(),
        _ => None,
    }
    .unwrap_or_else(|| panic!("assertion kind '{}' is missing required field '{}': {:?}", spec.kind, field, spec))
}

fn read_case_file(case_root: &Path, relative: &str) -> String {
    let path = case_root.join(relative);
    fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("failed to read file {}: {}", path.display(), e))
}

fn resolve_case_root(manifest: &ValidationManifest, fallback_root: &Path) -> PathBuf {
    if let Some(ref configured) = manifest.case_root {
        return PathBuf::from(configured);
    }
    fallback_root.to_path_buf()
}

fn run_assertion(case_root: &Path, spec: &AssertionSpec) {
    match spec.kind.as_str() {
        "contains" => {
            let file = require_field(spec, "file");
            let pattern = require_field(spec, "pattern");
            let content = read_case_file(case_root, &file);
            assert!(
                content.contains(&pattern),
                "contains assertion failed\nfile: {}\npattern: {}",
                file,
                pattern
            );
        }
        "not_contains" => {
            let file = require_field(spec, "file");
            let pattern = require_field(spec, "pattern");
            let content = read_case_file(case_root, &file);
            assert!(
                !content.contains(&pattern),
                "not_contains assertion failed\nfile: {}\npattern: {}",
                file,
                pattern
            );
        }
        "header_include_exists" => {
            let file = require_field(spec, "file");
            let include = require_field(spec, "include");
            let content = read_case_file(case_root, &file);
            let needle = format!("#include \"{}\"", include);
            assert!(
                content.contains(&needle),
                "header_include_exists assertion failed\nfile: {}\nexpected include: {}",
                file,
                needle
            );
        }
        "api_macro_matches_module" => {
            let file = require_field(spec, "file");
            let module_name = require_field(spec, "module_name");
            let content = read_case_file(case_root, &file);
            let macro_token = format!("{}_API", module_name.to_uppercase());
            assert!(
                content.contains(&macro_token),
                "api_macro_matches_module assertion failed\nfile: {}\nexpected macro token: {}",
                file,
                macro_token
            );
        }
        "module_name_matches_uplugin" => {
            let uplugin_file = require_field(spec, "uplugin_file");
            let build_cs_file = require_field(spec, "build_cs_file");
            let module_name = require_field(spec, "module_name");

            let uplugin_raw = read_case_file(case_root, &uplugin_file);
            let parsed: Value = serde_json::from_str(&uplugin_raw)
                .unwrap_or_else(|e| panic!("failed to parse uplugin {}: {}", uplugin_file, e));

            let module_found = parsed
                .get("Modules")
                .and_then(|mods| mods.as_array())
                .map(|mods| {
                    mods.iter().any(|m| {
                        m.get("Name")
                            .and_then(|n| n.as_str())
                            .map(|n| n == module_name)
                            .unwrap_or(false)
                    })
                })
                .unwrap_or(false);

            assert!(
                module_found,
                "module_name_matches_uplugin assertion failed\nuplugin: {}\nexpected module: {}",
                uplugin_file,
                module_name
            );

            let build_cs = read_case_file(case_root, &build_cs_file);
            let class_decl = format!("public class {} : ModuleRules", module_name);
            assert!(
                build_cs.contains(&class_decl),
                "module_name_matches_uplugin assertion failed\nbuild.cs: {}\nexpected declaration: {}",
                build_cs_file,
                class_decl
            );
        }
        "implement_module_matches" => {
            let file = require_field(spec, "file");
            let module_name = require_field(spec, "module_name");
            let content = read_case_file(case_root, &file);
            let compact: String = content.chars().filter(|c| !c.is_whitespace()).collect();
            let marker = format!(",{})", module_name);
            assert!(
                compact.contains("IMPLEMENT_MODULE(") && compact.contains(&marker),
                "implement_module_matches assertion failed\nfile: {}\nexpected module in IMPLEMENT_MODULE: {}",
                file,
                module_name
            );
        }
        other => panic!("unknown assertion kind: {}", other),
    }
}

fn run_case(case_root: &Path, manifest_path: &Path) {
    let manifest = read_manifest(manifest_path);
    let effective_case_root = resolve_case_root(&manifest, case_root);
    run_optional_command(&manifest, &effective_case_root);
    for assertion in &manifest.assertions {
        run_assertion(&effective_case_root, assertion);
    }
}

#[test]
fn codegen_verify_sample_fixture_case() {
    let case_root = fixtures_root().join("sample_case");
    run_case(&case_root, &case_root.join("manifest.toml"));
}

#[test]
#[ignore]
fn codegen_verify_factory_profiles() {
    let factory_root = PathBuf::from("M:/Code/Factory");
    if !factory_root.exists() {
        return;
    }

    let profile_dir = fixtures_root().join("factory_profiles");
    if !profile_dir.exists() {
        return;
    }

    for entry in fs::read_dir(&profile_dir).expect("failed to list factory_profiles") {
        let entry = entry.expect("failed to read directory entry");
        let path = entry.path();
        if path.extension().and_then(|e| e.to_str()) != Some("toml") {
            continue;
        }

        let profile = read_manifest(&path);
        let default_case_root = profile_dir.join(
            path.file_stem()
                .and_then(|s| s.to_str())
                .expect("profile file stem")
        );
        let case_root = resolve_case_root(&profile, &default_case_root);

        if !case_root.exists() {
            continue;
        }

        run_optional_command(&profile, &case_root);
        for assertion in &profile.assertions {
            run_assertion(&case_root, assertion);
        }
    }
}
