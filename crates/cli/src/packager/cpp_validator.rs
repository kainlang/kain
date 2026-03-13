// Copyright 2026 Zentako. All Rights Reserved.
// C++ Validation Module - ReSharper CLI Integration
//
// Validates generated C++ headers using JetBrains ReSharper C++ analyzer
// before UE5 compilation. Catches errors in ~2 seconds instead of 2 minutes.

use crate::error::{KainError, KainResult};
use std::path::{Path, PathBuf};
use std::process::Command;

/// Find ReSharper inspectcode.exe on the system
pub fn find_inspectcode() -> Option<PathBuf> {
    let locations = vec![
        // User's C:\Tools location
        PathBuf::from("C:\\Tools\\ReSharper.CLI\\inspectcode.exe"),
        PathBuf::from("C:\\Tools\\JetBrains.ReSharper.CommandLineTools.2025.3.3\\inspectcode.exe"),
        // Common installation paths
        PathBuf::from("C:\\Program Files\\JetBrains\\ReSharper.CLI\\inspectcode.exe"),
        PathBuf::from("C:\\Program Files (x86)\\JetBrains\\ReSharper.CLI\\inspectcode.exe"),
        // Check if it's in PATH
        which::which("inspectcode").ok()?,
    ];

    locations.into_iter().find(|p| p.exists())
}

/// Validate generated C++ code with ReSharper C++ analyzer
pub fn validate_cpp_with_resharper(plugin_dir: &Path) -> KainResult<()> {
    let inspectcode = match find_inspectcode() {
        Some(path) => {
            println!("   🔍 Found ReSharper CLI: {}", path.display());
            path
        }
        None => {
            println!("   ⚠️  ReSharper CLI not found (skipping advanced C++ validation)");
            println!("      Install to C:\\Tools\\ReSharper.CLI\\ for better error detection");
            return Ok(()); // Don't fail, just skip
        }
    };

    let source_dir = plugin_dir.join("Source");
    if !source_dir.exists() {
        return Err(KainError::runtime("Source directory not found"));
    }

    println!("   🔬 Running ReSharper C++ analysis...");

    // Create temp output file
    let output_file = plugin_dir.join("resharper_report.xml");

    // Run inspectcode
    let output = Command::new(&inspectcode)
        .arg(source_dir.to_str().unwrap())
        .arg(format!("--output={}", output_file.display()))
        .arg("--severity=WARNING")
        .arg("--format=Xml")
        .arg("--no-build")
        .output()
        .map_err(|e| KainError::runtime(format!("Failed to run inspectcode: {}", e)))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);

        // Check if it's just warnings (exit code 1 with warnings is OK)
        if output.status.code() == Some(1) && output_file.exists() {
            // Parse and display warnings
            parse_resharper_report(&output_file)?;
        } else {
            return Err(KainError::runtime(format!(
                "ReSharper analysis failed:\n{}",
                stderr
            )));
        }
    } else {
        println!("      ✅ No C++ issues found");
    }

    // Clean up report file
    let _ = std::fs::remove_file(&output_file);

    Ok(())
}

/// Parse ReSharper XML report and display errors/warnings
fn parse_resharper_report(report_path: &Path) -> KainResult<()> {
    use quick_xml::events::Event;
    use quick_xml::Reader;

    let xml_content = std::fs::read_to_string(report_path)
        .map_err(|e| KainError::runtime(format!("Failed to read report: {}", e)))?;

    let mut reader = Reader::from_str(&xml_content);

    let mut buf = Vec::new();
    let mut issues = Vec::new();
    let mut current_issue: Option<CppIssue> = None;

    loop {
        match reader.read_event_into(&mut buf) {
            Ok(Event::Start(e)) | Ok(Event::Empty(e)) => match e.name().as_ref() {
                b"Issue" => {
                    let mut issue = CppIssue::default();

                    for attr in e.attributes() {
                        if let Ok(attr) = attr {
                            match attr.key.as_ref() {
                                b"TypeId" => {
                                    issue.type_id = String::from_utf8_lossy(&attr.value).to_string()
                                }
                                b"File" => {
                                    issue.file = String::from_utf8_lossy(&attr.value).to_string()
                                }
                                b"Line" => {
                                    issue.line =
                                        String::from_utf8_lossy(&attr.value).parse().unwrap_or(0)
                                }
                                b"Message" => {
                                    issue.message = String::from_utf8_lossy(&attr.value).to_string()
                                }
                                b"Severity" => {
                                    issue.severity =
                                        String::from_utf8_lossy(&attr.value).to_string()
                                }
                                _ => {}
                            }
                        }
                    }

                    current_issue = Some(issue);
                }
                _ => {}
            },
            Ok(Event::End(e)) => {
                if e.name().as_ref() == b"Issue" {
                    if let Some(issue) = current_issue.take() {
                        issues.push(issue);
                    }
                }
            }
            Ok(Event::Eof) => break,
            Err(e) => return Err(KainError::runtime(format!("XML parse error: {}", e))),
            _ => {}
        }
        buf.clear();
    }

    if !issues.is_empty() {
        println!("      ⚠️  Found {} C++ issues:", issues.len());
        for issue in issues.iter().take(10) {
            println!(
                "         {}:{} - {} ({})",
                issue.file, issue.line, issue.message, issue.type_id
            );
        }
        if issues.len() > 10 {
            println!("         ... and {} more", issues.len() - 10);
        }
    }

    Ok(())
}

#[derive(Default)]
struct CppIssue {
    type_id: String,
    file: String,
    line: usize,
    message: String,
    severity: String,
}

/// Fallback: Basic C++ validation without ReSharper
pub fn validate_cpp_basic(plugin_dir: &Path) -> KainResult<()> {
    println!("   🔍 Running basic C++ validation...");

    let source_dir = plugin_dir.join("Source");
    if !source_dir.exists() {
        return Ok(());
    }

    // Find all .h files
    let headers = find_headers(&source_dir)?;

    let mut errors = Vec::new();

    for header in headers {
        let content = std::fs::read_to_string(&header).map_err(|e| {
            KainError::runtime(format!("Failed to read {}: {}", header.display(), e))
        })?;

        // Check for common issues
        check_struct_class_consistency(&content, &header, &mut errors);
        check_missing_generated_body(&content, &header, &mut errors);
    }

    if !errors.is_empty() {
        println!("      ⚠️  Found {} potential issues:", errors.len());
        for error in errors.iter().take(5) {
            println!("         {}", error);
        }
        if errors.len() > 5 {
            println!("         ... and {} more", errors.len() - 5);
        }
    } else {
        println!("      ✅ No obvious issues found");
    }

    Ok(())
}

fn find_headers(dir: &Path) -> KainResult<Vec<PathBuf>> {
    let mut headers = Vec::new();

    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                headers.extend(find_headers(&path)?);
            } else if path.extension().and_then(|s| s.to_str()) == Some("h") {
                headers.push(path);
            }
        }
    }

    Ok(headers)
}

fn check_struct_class_consistency(content: &str, path: &Path, errors: &mut Vec<String>) {
    use std::collections::HashMap;

    let mut forward_decls: HashMap<String, &str> = HashMap::new();
    let mut definitions: HashMap<String, &str> = HashMap::new();

    // Simple regex-like parsing for forward declarations
    for line in content.lines() {
        let trimmed = line.trim();

        // Forward declaration: "class Foo;" or "struct Foo;"
        if let Some(rest) = trimmed.strip_prefix("class ") {
            if let Some(name) = rest.split(';').next() {
                forward_decls.insert(name.trim().to_string(), "class");
            }
        } else if let Some(rest) = trimmed.strip_prefix("struct ") {
            if let Some(name) = rest.split(';').next() {
                forward_decls.insert(name.trim().to_string(), "struct");
            }
        }

        // Definition: "class PLUGIN_API Foo" or "struct PLUGIN_API Foo"
        if trimmed.starts_with("class ") && trimmed.contains("_API ") {
            if let Some(name) = trimmed.split("_API ").nth(1) {
                if let Some(name) = name.split_whitespace().next() {
                    definitions.insert(name.to_string(), "class");
                }
            }
        } else if trimmed.starts_with("struct ") && trimmed.contains("_API ") {
            if let Some(name) = trimmed.split("_API ").nth(1) {
                if let Some(name) = name.split_whitespace().next() {
                    definitions.insert(name.to_string(), "struct");
                }
            }
        }
    }

    // Check for mismatches
    for (name, fwd_type) in forward_decls {
        if let Some(&def_type) = definitions.get(&name) {
            if fwd_type != def_type {
                errors.push(format!(
                    "{}:\n   Type '{}' forward declared as '{}' but defined as '{}'",
                    path.display(),
                    name,
                    fwd_type,
                    def_type
                ));
            }
        }
    }
}

fn check_missing_generated_body(content: &str, path: &Path, errors: &mut Vec<String>) {
    // Check if UCLASS/USTRUCT exists but GENERATED_BODY() is missing
    let has_uclass = content.contains("UCLASS(") || content.contains("USTRUCT(");
    let has_generated_body =
        content.contains("GENERATED_BODY()") || content.contains("GENERATED_USTRUCT_BODY()");

    if has_uclass && !has_generated_body {
        errors.push(format!(
            "{}:\n   UCLASS/USTRUCT found but GENERATED_BODY() is missing",
            path.display()
        ));
    }
}
