use crate::error::{KainError, KainResult};
use std::path::Path;
use std::fs;

/// Import a C file into KAIN AST and optionally write/compile
pub fn import_c(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    include_paths: &[String],
    defines: &[String],
) -> KainResult<()> {
    let options = kain_import::c::CImportOptions {
        include_paths: include_paths.to_vec(),
        defines: defines.to_vec(),
        cpp_options: Vec::new(),
        cpp_command: None,
    };

    // Import C file to KAIN AST
    let program = kain_import::c::import_c_file_with_options(input, &options)
        .map_err(|e| KainError::runtime(format!("C import failed: {}", e)))?;
    
    // Generate KAIN source code from AST
    let kain_source = generate_kain_source(&program)?;
    
    // If output path specified, write KAIN source
    if let Some(out_path) = output {
        fs::write(out_path, &kain_source)
            .map_err(|e| KainError::runtime(format!("Failed to write output: {}", e)))?;
        
        println!(" Generated KAIN source: {} ({} bytes)", out_path.display(), kain_source.len());
    }
    
    // If target specified, compile directly
    if let Some(target_str) = target {
        let compile_target = crate::parse_compile_target(target_str)
            .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;
        
        println!(" Compiling to target: {}", target_str);
        
        let compiled = crate::compile(&kain_source, compile_target)
            .map_err(|e| KainError::runtime(format!("Compilation failed: {}", e)))?;
        
        // Determine output path for compiled result
        let compiled_output = if let Some(out) = output {
            out.with_extension(crate::target_extension(compile_target))
        } else {
            input.with_extension(crate::target_extension(compile_target))
        };
        
        fs::write(&compiled_output, &compiled)
            .map_err(|e| KainError::runtime(format!("Failed to write compiled output: {}", e)))?;
        
        println!(" Compiled output: {} ({} bytes)", compiled_output.display(), compiled.len());
    }
    
    // Print summary
    println!(" Import complete");
    println!(" Functions: {}", count_functions(&program));
    println!(" Structs: {}", count_structs(&program));
    
    Ok(())
}

/// Generate KAIN source code from AST
fn generate_kain_source(program: &kain_core::ast::Program) -> KainResult<String> {
    use std::fmt::Write;
    
    let mut output = String::new();
    
    // Header comment
    writeln!(output, "# Generated from C source by kain import-c")
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    
    // Generate code for each item
    for item in &program.items {
        match item {
            kain_core::ast::Item::Function(func) => {
                write_function(&mut output, func)?;
            }
            kain_core::ast::Item::Struct(s) => {
                write_struct(&mut output, s)?;
            }
            kain_core::ast::Item::Enum(e) => {
                write_enum(&mut output, e)?;
            }
            _ => {
                // Skip other items for now
            }
        }
    }
    
    Ok(output)
}

fn write_function(output: &mut String, func: &kain_core::ast::Function) -> KainResult<()> {
    use std::fmt::Write;
    
    // Function signature
    write!(output, "fn {}(", func.name)
        .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    
    // Parameters
    for (i, param) in func.params.iter().enumerate() {
        if i > 0 {
            write!(output, ", ")
                .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
        }
        write!(output, "{}: {}", param.name, type_to_string(&param.ty))
            .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    }
    
    write!(output, ")")
        .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    
    // Return type
    if let Some(ret_ty) = &func.return_type {
        write!(output, " -> {}", type_to_string(ret_ty))
            .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    }
    
    writeln!(output, ":")
        .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    
    // Body (simplified - just a placeholder)
    writeln!(output, "    # Function body")
        .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    
    Ok(())
}

fn write_struct(output: &mut String, s: &kain_core::ast::Struct) -> KainResult<()> {
    use std::fmt::Write;
    
    writeln!(output, "struct {}:", s.name)
        .map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;
    
    for field in &s.fields {
        writeln!(output, "    {}: {}", field.name, type_to_string(&field.ty))
            .map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;
    }
    
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;
    
    Ok(())
}

fn write_enum(output: &mut String, e: &kain_core::ast::Enum) -> KainResult<()> {
    use std::fmt::Write;
    
    writeln!(output, "enum {}:", e.name)
        .map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;
    
    for variant in &e.variants {
        writeln!(output, "    {}", variant.name)
            .map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;
    }
    
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;
    
    Ok(())
}

fn type_to_string(ty: &kain_core::ast::Type) -> String {
    match ty {
        kain_core::ast::Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                let args = generics
                    .iter()
                    .map(type_to_string)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}<{}>", name, args)
            }
        }
        kain_core::ast::Type::Tuple(types, _) => {
            let members = types.iter().map(type_to_string).collect::<Vec<_>>().join(", ");
            format!("({})", members)
        }
        kain_core::ast::Type::Array(inner, size, _) => {
            format!("[{}; {}]", type_to_string(inner), size)
        }
        kain_core::ast::Type::Slice(inner, _) => format!("[{}]", type_to_string(inner)),
        kain_core::ast::Type::Ref {
            mutable, inner, ..
        } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        kain_core::ast::Type::Function {
            params,
            return_type,
            ..
        } => {
            let args = params.iter().map(type_to_string).collect::<Vec<_>>().join(", ");
            format!("fn({}) -> {}", args, type_to_string(return_type))
        }
        kain_core::ast::Type::Option(inner, _) => format!("{}?", type_to_string(inner)),
        kain_core::ast::Type::Result(ok, err, _) => {
            format!("{}!{}", type_to_string(ok), type_to_string(err))
        }
        kain_core::ast::Type::Infer(_) => "_".to_string(),
        kain_core::ast::Type::Never(_) => "!".to_string(),
        kain_core::ast::Type::Unit(_) => "()".to_string(),
        kain_core::ast::Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                let args = generics
                    .iter()
                    .map(type_to_string)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("impl {}<{}>", trait_name, args)
            }
        }
    }
}

fn count_functions(program: &kain_core::ast::Program) -> usize {
    program.items.iter().filter(|item| matches!(item, kain_core::ast::Item::Function(_))).count()
}

fn count_structs(program: &kain_core::ast::Program) -> usize {
    program.items.iter().filter(|item| matches!(item, kain_core::ast::Item::Struct(_))).count()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    use tempfile::NamedTempFile;
    
    #[test]
    fn test_import_simple_c_file() {
        // Create a temporary C file
        let mut temp_file = NamedTempFile::new().unwrap();
        writeln!(temp_file, "int add(int a, int b) {{").unwrap();
        writeln!(temp_file, "    return a + b;").unwrap();
        writeln!(temp_file, "}}").unwrap();
        temp_file.flush().unwrap();
        
        // Import it
        let result = import_c(temp_file.path(), None, None, &[], &[]);
        
        // Should succeed
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_import_with_output() {
        // Create a temporary C file
        let mut temp_c = NamedTempFile::new().unwrap();
        writeln!(temp_c, "int multiply(int x, int y) {{").unwrap();
        writeln!(temp_c, "    return x * y;").unwrap();
        writeln!(temp_c, "}}").unwrap();
        temp_c.flush().unwrap();
        
        // Create output path
        let temp_out = NamedTempFile::new().unwrap();
        let out_path = temp_out.path();
        
        // Import with output
        let result = import_c(temp_c.path(), Some(out_path), None, &[], &[]);
        
        // Should succeed and create output file
        assert!(result.is_ok());
        assert!(out_path.exists());
        
        // Output should contain KAIN code
        let content = fs::read_to_string(out_path).unwrap();
        assert!(content.contains("fn multiply"));
    }
    
    #[test]
    fn test_import_with_target() {
        // Create a temporary C file
        let mut temp_c = NamedTempFile::new().unwrap();
        writeln!(temp_c, "int square(int n) {{").unwrap();
        writeln!(temp_c, "    return n * n;").unwrap();
        writeln!(temp_c, "}}").unwrap();
        temp_c.flush().unwrap();
        
        // Import with wasm target
        let result = import_c(temp_c.path(), None, Some("wasm"), &[], &[]);
        
        // Should succeed
        assert!(result.is_ok());
    }
}
