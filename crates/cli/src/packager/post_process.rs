use crate::error::{KainError, KainResult};
use std::path::PathBuf;

/// Run Python post-processor to auto-fix edge cases
pub fn run_python_post_processor(plugin_path: &PathBuf, plugin_name: &str) -> KainResult<()> {
    use std::process::Command;

    // Find Python script - try multiple locations
    let mut script_path = None;

    // 1. Try relative to cwd
    let cwd_script = std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join("kain")
        .join("python")
        .join("post_process.py");

    if cwd_script.exists() {
        script_path = Some(cwd_script);
    } else {
        // 2. Try walking up directories to find kain/python/post_process.py
        let mut current = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        for _ in 0..5 {
            // Try up to 5 levels up
            let candidate = current.join("kain").join("python").join("post_process.py");
            if candidate.exists() {
                script_path = Some(candidate);
                break;
            }
            if let Some(parent) = current.parent() {
                current = parent.to_path_buf();
            } else {
                break;
            }
        }

        // 3. Try relative to executable
        if script_path.is_none() {
            if let Ok(exe_path) = std::env::current_exe() {
                if let Some(exe_dir) = exe_path.parent() {
                    let exe_script = exe_dir.join("python").join("post_process.py");
                    if exe_script.exists() {
                        script_path = Some(exe_script);
                    }
                }
            }
        }
    }

    let script_path = match script_path {
        Some(p) => p,
        None => {
            println!("   ⚠️  Python post-processor not found (skipping)");
            return Ok(());
        }
    };

    // Run Python script
    let output = Command::new("python")
        .arg(&script_path)
        .arg(plugin_path)
        .arg(plugin_name)
        .arg("--verbose")
        .output();

    match output {
        Ok(output) => {
            if output.status.success() {
                // Parse JSON output
                let stdout = String::from_utf8_lossy(&output.stdout);

                // Find JSON in output (last line)
                if let Some(json_line) = stdout.lines().last() {
                    if let Ok(result) = serde_json::from_str::<serde_json::Value>(json_line) {
                        if let Some(fixes) = result.get("fixes_applied").and_then(|v| v.as_u64()) {
                            println!("   ✅ Applied {} auto-fixes", fixes);

                            // Show fixes if verbose
                            if let Some(fixes_list) = result.get("fixes").and_then(|v| v.as_array())
                            {
                                for fix in fixes_list {
                                    if let Some(fix_str) = fix.as_str() {
                                        println!("      - {}", fix_str);
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                let stderr = String::from_utf8_lossy(&output.stderr);
                println!("   ⚠️  Python post-processor failed:");
                println!("{}", stderr);
            }
        }
        Err(e) => {
            println!("   ⚠️  Could not run Python post-processor: {}", e);
            println!("      (Make sure Python is installed and in PATH)");
        }
    }

    Ok(())
}

/// Convert span position to line:column for better error messages
pub fn format_error_with_location(source: &str, error_msg: &str, file_name: String) -> String {
    // Extract span from error message if present
    if let Some(start_idx) = error_msg.find("Span { start: ") {
        if let Some(end_idx) = error_msg[start_idx..].find(" }") {
            let span_str = &error_msg[start_idx..start_idx + end_idx + 2];

            // Parse span
            if let Some(start_pos) = span_str.split("start: ").nth(1) {
                if let Some(start_num_str) = start_pos.split(',').next() {
                    if let Ok(start_pos) = start_num_str.parse::<usize>() {
                        // Convert position to line:column
                        let (line, col) = position_to_line_col(source, start_pos);

                        // Extract the line content
                        let line_content = get_line_content(source, line);

                        // Format nice error message
                        return format!(
                            "\n   {}:{}:{}\n   |\n{} | {}\n   | {}^\n   |\n   {}",
                            file_name,
                            line,
                            col,
                            line,
                            line_content,
                            " ".repeat(col.saturating_sub(1)),
                            error_msg.split(": ").last().unwrap_or(error_msg)
                        );
                    }
                }
            }
        }
    }

    // Fallback to original error message
    error_msg.to_string()
}

/// Convert byte position to line:column (1-indexed)
fn position_to_line_col(source: &str, pos: usize) -> (usize, usize) {
    let mut line = 1;
    let mut col = 1;

    for (i, ch) in source.chars().enumerate() {
        if i >= pos {
            break;
        }
        if ch == '\n' {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
    }

    (line, col)
}

/// Get the content of a specific line (1-indexed)
fn get_line_content(source: &str, line_num: usize) -> String {
    source
        .lines()
        .nth(line_num.saturating_sub(1))
        .unwrap_or("")
        .to_string()
}

/// Extract shader names from KAIN source code
pub fn extract_shader_names(source: &str) -> KainResult<Vec<String>> {
    match kain_core::Lexer::new(source).tokenize() {
        Ok(tokens) => {
            let span_mapper = kain_core::diagnostics::SpanMapper::new(source);
            match kain_core::Parser::new(&tokens, &span_mapper, "<shader_extract>").parse() {
                Ok(ast) => {
                    let names: Vec<String> = ast
                        .items
                        .iter()
                        .filter_map(|item| {
                            if let kain_core::ast::Item::Shader(shader) = item {
                                Some(shader.name.clone())
                            } else {
                                None
                            }
                        })
                        .collect();
                    Ok(names)
                }
                Err(e) => Err(KainError::runtime(format!("Failed to parse source: {}", e))),
            }
        }
        Err(e) => Err(KainError::runtime(format!(
            "Failed to tokenize source: {}",
            e
        ))),
    }
}
