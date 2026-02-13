//! UE5 Standard Library Resolver for Godmode v3
//! 
//! This module handles the mapping of KAIN StdLib functions to actual UE5 C++ calls.
//! It allows high-level KAIN code to translate into efficient, native Unreal C++.

use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct StdLibResolver {
    /// Map of KAIN function name to C++ template (using $0, $1 for args)
    /// Example: "GetActorLocation" -> "$0->GetActorLocation()"
    pub function_map: HashMap<String, String>,
}

impl StdLibResolver {
    pub fn new() -> Self {
        let mut resolver = Self {
            function_map: HashMap::new(),
        };

        // --- CORE KAIN -> UE5 HANDLERS (Manual overrides) ---
        resolver.add("println", "UE_LOG(LogTemp, Warning, TEXT(\"%s\"), *$0)");
        resolver.add("print", "UE_LOG(LogTemp, Warning, TEXT(\"%s\"), *$0)");

        resolver
    }

    /// Load thousands of mappings from the Oracle JSON
    pub fn load_from_metadata(&mut self, json_data: &str) -> Result<(), String> {
        let v: serde_json::Value = serde_json::from_str(json_data).map_err(|e| e.to_string())?;
        
        if let Some(files) = v.as_array() {
            for file in files {
                if let Some(content) = file.get("content") {
                    // 1. Functions
                    if let Some(funcs) = content.get("functions").and_then(|f| f.as_array()) {
                        for func in funcs {
                            let name = func.get("name").and_then(|n| n.as_str()).unwrap_or("");
                            if name.is_empty() { continue; }
                            
                            let mut template = format!("$0->{}()", name);
                            if let Some(params) = func.get("params").and_then(|p| p.as_array()) {
                                if !params.is_empty() {
                                    let mut p_placeholders = Vec::new();
                                    for i in 1..=params.len() {
                                        p_placeholders.push(format!("${}", i));
                                    }
                                    template = format!("$0->{}({})", name, p_placeholders.join(", "));
                                }
                            }
                            self.add(name, &template);
                        }
                    }

                    // 2. Properties (Getters/Setters)
                    if let Some(props) = content.get("properties").and_then(|p| p.as_array()) {
                        for prop in props {
                            let name = prop.get("name").and_then(|n| n.as_str()).unwrap_or("");
                            if name.is_empty() { continue; }

                            // Simple getter: $0->Name
                            self.add(&format!("get_{}", name), &format!("$0->{}", name));
                            // Simple setter: $0->Name = $1
                            self.add(&format!("set_{}", name), &format!("$0->{} = $1", name));
                        }
                    }
                }
            }
        }
        Ok(())
    }

    fn add(&mut self, name: &str, template: &str) {
        self.function_map.insert(name.to_string(), template.to_string());
    }

    /// Resolve a function call to C++ code
    pub fn resolve(&self, name: &str, args: &[String]) -> Option<String> {
        let template = self.function_map.get(name)?;
        let mut result = template.clone();
        
        for (i, arg) in args.iter().enumerate() {
            let placeholder = format!("${}", i);
            result = result.replace(&placeholder, arg);
        }
        
        Some(result)
    }
}

impl Default for StdLibResolver {
    fn default() -> Self {
        Self::new()
    }
}
