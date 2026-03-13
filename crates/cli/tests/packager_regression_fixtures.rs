use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

use cli::packager::build_cs_gen::generate_build_cs_module;
use cli::packager::config::{Ue5ModuleConfig, Ue5ModuleOutputConfig, Ue5ModuleType};
use cli::packager::uplugin_gen::{generate_uplugin_file, generate_uplugin_file_from_modules};

fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join("packager")
        .join(name);
    fs::read_to_string(path).expect("fixture should exist")
}

fn normalize(s: &str) -> String {
    s.replace("\r\n", "\n")
}

#[test]
fn legacy_single_uplugin_fixture_matches() {
    let actual = generate_uplugin_file("DemoPlugin", &None, false, false, false, &[]);
    let expected = fixture("legacy_single.uplugin.json");
    assert_eq!(normalize(&actual), normalize(&expected));
}

#[test]
fn legacy_split_uplugin_fixture_matches() {
    let actual = generate_uplugin_file("DemoPlugin", &None, false, true, false, &[]);
    let expected = fixture("legacy_split.uplugin.json");
    assert_eq!(normalize(&actual), normalize(&expected));
}

#[test]
fn multimodule_uplugin_fixture_matches() {
    let modules = vec![
        Ue5ModuleConfig {
            name: "DemoPlugin".to_string(),
            module_type: Ue5ModuleType::Runtime,
            loading_phase: "PostConfigInit".to_string(),
            source_globs: vec![],
            public_deps: vec![],
            private_deps: vec![],
            depends_on: vec![],
            output: Ue5ModuleOutputConfig::default(),
            folders: HashMap::new(),
        },
        Ue5ModuleConfig {
            name: "DemoPluginEditor".to_string(),
            module_type: Ue5ModuleType::Editor,
            loading_phase: "PostEngineInit".to_string(),
            source_globs: vec![],
            public_deps: vec![],
            private_deps: vec![],
            depends_on: vec!["DemoPlugin".to_string()],
            output: Ue5ModuleOutputConfig::default(),
            folders: HashMap::new(),
        },
        Ue5ModuleConfig {
            name: "DemoPluginUncooked".to_string(),
            module_type: Ue5ModuleType::UncookedOnly,
            loading_phase: "PostDefault".to_string(),
            source_globs: vec![],
            public_deps: vec![],
            private_deps: vec![],
            depends_on: vec!["DemoPlugin".to_string()],
            output: Ue5ModuleOutputConfig::default(),
            folders: HashMap::new(),
        },
    ];

    let actual = generate_uplugin_file_from_modules("DemoPlugin", &None, true, &modules, &[]);
    let expected = fixture("multimodule.uplugin.json");
    assert_eq!(normalize(&actual), normalize(&expected));
}

#[test]
fn multimodule_build_cs_fixture_matches() {
    let actual = generate_build_cs_module(
        "DemoPlugin",
        &[
            "Core".to_string(),
            "CoreUObject".to_string(),
            "Engine".to_string(),
        ],
        &["Projects".to_string()],
    );
    let expected = fixture("multimodule_runtime.Build.cs");
    assert_eq!(normalize(&actual), normalize(&expected));
}
