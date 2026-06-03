use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, Default)]
pub struct LlvmIrReachability {
    pub defined_functions: BTreeSet<String>,
    pub reachable_functions: BTreeSet<String>,
    pub reachable_declared_targets: BTreeSet<String>,
    pub reachable_external_targets: BTreeSet<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LlvmIrSliceStats {
    pub original_bytes: usize,
    pub sliced_bytes: usize,
    pub original_functions: usize,
    pub kept_functions: usize,
    pub removed_functions: usize,
    pub removed_declarations: usize,
}

pub fn slice_llvm_native_executable_ir(ir: &str) -> Option<(String, LlvmIrSliceStats)> {
    let reachability = analyze_llvm_ir_reachability(ir, &["main"]);
    if !reachability.defined_functions.contains("main") {
        return None;
    }

    let original_functions = reachability.defined_functions.len();
    let kept_functions = reachability.reachable_functions.len();
    if kept_functions >= original_functions {
        return None;
    }

    let mut output = String::with_capacity(ir.len());
    let mut skipping_function = false;
    let mut removed_functions = 0usize;
    let mut removed_declarations = 0usize;

    for line in ir.lines() {
        if let Some(function) = extract_defined_llvm_function(line) {
            if reachability.reachable_functions.contains(&function) {
                output.push_str(line);
                output.push('\n');
                skipping_function = false;
            } else {
                removed_functions += 1;
                skipping_function = true;
            }
            continue;
        }

        if skipping_function {
            if line.trim() == "}" {
                skipping_function = false;
            }
            continue;
        }

        if let Some(declared) = extract_declared_llvm_function(line) {
            if !reachability.reachable_declared_targets.contains(&declared) {
                removed_declarations += 1;
                continue;
            }
        }

        output.push_str(line);
        output.push('\n');
    }

    let stats = LlvmIrSliceStats {
        original_bytes: ir.len(),
        sliced_bytes: output.len(),
        original_functions,
        kept_functions,
        removed_functions,
        removed_declarations,
    };
    Some((output, stats))
}

pub fn analyze_llvm_ir_reachability(ir: &str, roots: &[&str]) -> LlvmIrReachability {
    let mut defined_functions = BTreeSet::new();
    let mut declared_functions = BTreeSet::new();
    let mut defined_globals = BTreeSet::new();

    for line in ir.lines() {
        if let Some(function) = extract_defined_llvm_function(line) {
            defined_functions.insert(function);
        } else if let Some(function) = extract_declared_llvm_function(line) {
            declared_functions.insert(function);
        } else if let Some(global) = extract_defined_llvm_global(line) {
            defined_globals.insert(global);
        }
    }

    let mut refs_by_function: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    let mut top_level_declared_targets = BTreeSet::new();
    let mut top_level_external_targets = BTreeSet::new();
    let mut root_functions = roots
        .iter()
        .filter(|root| defined_functions.contains(**root))
        .map(|root| (*root).to_string())
        .collect::<BTreeSet<_>>();
    let mut current_function: Option<String> = None;

    for line in ir.lines() {
        if let Some(function) = extract_defined_llvm_function(line) {
            refs_by_function.entry(function.clone()).or_default();
            current_function = Some(function);
            continue;
        }

        if line.trim() == "}" {
            current_function = None;
            continue;
        }

        if let Some(function) = current_function.as_ref() {
            for target in extract_llvm_symbol_references(line) {
                refs_by_function
                    .entry(function.clone())
                    .or_default()
                    .insert(target);
            }
            continue;
        }

        if extract_declared_llvm_function(line).is_some() {
            continue;
        }

        for target in extract_llvm_symbol_references(line) {
            if defined_functions.contains(&target) {
                root_functions.insert(target);
            } else if declared_functions.contains(&target) {
                if !is_allowed_llvm_external_target(&target) {
                    top_level_external_targets.insert(target.clone());
                }
                top_level_declared_targets.insert(target);
            } else if !defined_globals.contains(&target)
                && !is_allowed_llvm_external_target(&target)
            {
                top_level_external_targets.insert(target);
            }
        }
    }

    let mut reachable_functions = BTreeSet::new();
    let mut reachable_declared_targets = top_level_declared_targets;
    let mut reachable_external_targets = top_level_external_targets;
    let mut work = root_functions.into_iter().collect::<Vec<_>>();

    while let Some(function) = work.pop() {
        if !reachable_functions.insert(function.clone()) {
            continue;
        }
        for target in refs_by_function.get(&function).into_iter().flatten() {
            if defined_functions.contains(target) {
                work.push(target.clone());
            } else if declared_functions.contains(target) {
                reachable_declared_targets.insert(target.clone());
                if !is_allowed_llvm_external_target(target) {
                    reachable_external_targets.insert(target.clone());
                }
            } else if !defined_globals.contains(target) && !is_allowed_llvm_external_target(target)
            {
                reachable_external_targets.insert(target.clone());
            }
        }
    }

    LlvmIrReachability {
        defined_functions,
        reachable_functions,
        reachable_declared_targets,
        reachable_external_targets,
    }
}

fn extract_defined_llvm_function(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with("define ") {
        return None;
    }
    let at_index = trimmed.find('@')?;
    let (symbol, consumed) = extract_llvm_symbol_after_at(trimmed, at_index)?;
    let suffix = &trimmed[(at_index + 1 + consumed)..];
    if suffix.trim_start().starts_with('(') {
        Some(symbol)
    } else {
        None
    }
}

fn extract_declared_llvm_function(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with("declare ") {
        return None;
    }
    let at_index = trimmed.find('@')?;
    let (symbol, consumed) = extract_llvm_symbol_after_at(trimmed, at_index)?;
    let suffix = &trimmed[(at_index + 1 + consumed)..];
    if suffix.trim_start().starts_with('(') {
        Some(symbol)
    } else {
        None
    }
}

fn extract_defined_llvm_global(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with('@') {
        return None;
    }
    let (symbol, consumed) = extract_llvm_symbol_after_at(trimmed, 0)?;
    let suffix = &trimmed[(1 + consumed)..];
    if suffix.trim_start().starts_with('=') {
        Some(symbol)
    } else {
        None
    }
}

fn extract_llvm_symbol_references(line: &str) -> Vec<String> {
    if line.trim_start().starts_with(';') || !line.contains('@') {
        return Vec::new();
    }
    let mut targets = Vec::new();
    for (at_index, _) in line.match_indices('@') {
        let Some((symbol, _)) = extract_llvm_symbol_after_at(line, at_index) else {
            continue;
        };
        targets.push(symbol);
    }
    targets
}

fn extract_llvm_symbol_after_at(line: &str, at_index: usize) -> Option<(String, usize)> {
    let after_at = line.get((at_index + 1)..)?;
    if let Some(quoted) = after_at.strip_prefix('"') {
        let mut escaped = false;
        for (idx, ch) in quoted.char_indices() {
            if escaped {
                escaped = false;
                continue;
            }
            if ch == '\\' {
                escaped = true;
                continue;
            }
            if ch == '"' {
                let symbol = quoted[..idx].to_string();
                return (!symbol.is_empty()).then_some((symbol, idx + 2));
            }
        }
        return None;
    }

    let len = after_at
        .chars()
        .take_while(|ch| is_unquoted_llvm_symbol_char(*ch))
        .map(char::len_utf8)
        .sum::<usize>();
    if len == 0 {
        return None;
    }
    Some((after_at[..len].to_string(), len))
}

fn is_unquoted_llvm_symbol_char(ch: char) -> bool {
    ch.is_ascii_alphanumeric() || matches!(ch, '_' | '$' | '.' | '-')
}

fn is_allowed_llvm_external_target(symbol: &str) -> bool {
    symbol.starts_with("llvm.")
}

#[cfg(test)]
mod tests {
    use super::{analyze_llvm_ir_reachability, slice_llvm_native_executable_ir};

    #[test]
    fn reachability_handles_quoted_symbols() {
        let ir = r#"
define i32 @"main"() {
entry:
  call void @"kain.helper"()
  ret i32 0
}

define void @"kain.helper"() {
entry:
  ret void
}
"#;

        let reachability = analyze_llvm_ir_reachability(ir, &["main"]);

        assert!(reachability.reachable_external_targets.is_empty());
        assert_eq!(reachability.reachable_functions.len(), 2);
    }

    #[test]
    fn slicing_removes_unreachable_runtime_bodies() {
        let ir = r#"
declare void @llvm.lifetime.start.p0(i64, ptr)
declare void @unused_runtime()
@message = internal constant [1 x i8] c"\00"

define i64 @helper(i64 %value) {
entry:
  call void @llvm.lifetime.start.p0(i64 8, ptr null)
  ret i64 %value
}

define void @unreachable_runtime_wrapper() {
entry:
  call void @unused_runtime()
  ret void
}

define i32 @main() {
entry:
  %value = call i64 @helper(i64 7)
  ret i32 0
}
"#;

        let (sliced, stats) = slice_llvm_native_executable_ir(ir).expect("expected slice");

        assert!(sliced.contains("define i64 @helper"));
        assert!(sliced.contains("define i32 @main"));
        assert!(sliced.contains("@message = internal constant"));
        assert!(sliced.contains("declare void @llvm.lifetime.start.p0"));
        assert!(!sliced.contains("define void @unreachable_runtime_wrapper"));
        assert!(!sliced.contains("declare void @unused_runtime"));
        assert_eq!(stats.removed_functions, 1);
    }

    #[test]
    fn slicing_preserves_address_taken_callbacks() {
        let ir = r#"
declare void @register_callback(i8*)

define void @handler() {
entry:
  ret void
}

define void @dead_runtime_wrapper() {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @register_callback(i8* bitcast (void ()* @handler to i8*))
  ret i32 0
}
"#;

        let (sliced, _stats) = slice_llvm_native_executable_ir(ir).expect("expected slice");
        let reachability = analyze_llvm_ir_reachability(&sliced, &["main"]);

        assert!(sliced.contains("define void @handler"));
        assert!(sliced.contains("declare void @register_callback"));
        assert_eq!(
            reachability
                .reachable_external_targets
                .into_iter()
                .collect::<Vec<_>>(),
            vec!["register_callback".to_string()]
        );
    }

    #[test]
    fn slicing_preserves_top_level_external_declarations() {
        let ir = r#"
declare void @registered_from_global()
@callback_table = internal global [1 x ptr] [ptr @registered_from_global]

define void @dead_runtime_wrapper() {
entry:
  ret void
}

define i32 @main() {
entry:
  ret i32 0
}
"#;

        let (sliced, _stats) = slice_llvm_native_executable_ir(ir).expect("expected slice");
        let reachability = analyze_llvm_ir_reachability(&sliced, &["main"]);

        assert!(sliced.contains("declare void @registered_from_global"));
        assert!(sliced.contains("@callback_table = internal global"));
        assert!(!sliced.contains("define void @dead_runtime_wrapper"));
        assert_eq!(
            reachability
                .reachable_external_targets
                .into_iter()
                .collect::<Vec<_>>(),
            vec!["registered_from_global".to_string()]
        );
    }
}
