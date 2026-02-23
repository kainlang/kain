// Quick test to verify AnimSequence type mapping
use std::fs;

fn main() {
    // Load engine_knowledge.json
    let data = fs::read_to_string("unreal/metadata/engine_knowledge.json")
        .expect("Failed to read engine_knowledge.json");
    
    // Parse and check if AnimSequence is in type aliases
    let json: serde_json::Value = serde_json::from_str(&data).expect("Failed to parse JSON");
    
    if let Some(aliases) = json.get("type_aliases").and_then(|v| v.as_array()) {
        for alias in aliases {
            if let Some(kain_name) = alias.get("kain_name").and_then(|v| v.as_str()) {
                if kain_name == "AnimSequence" {
                    println!("Found AnimSequence alias:");
                    println!("  kain_name: {}", kain_name);
                    if let Some(ue5_name) = alias.get("ue5_name").and_then(|v| v.as_str()) {
                        println!("  ue5_name: {}", ue5_name);
                    }
                    if let Some(header) = alias.get("header").and_then(|v| v.as_str()) {
                        println!("  header: {}", header);
                    }
                }
            }
        }
    }
    
    // Check if UAnimSequence is in classes
    if let Some(classes) = json.get("classes").and_then(|v| v.as_array()) {
        for class in classes {
            if let Some(name) = class.get("name").and_then(|v| v.as_str()) {
                if name == "UAnimSequence" {
                    println!("\nFound UAnimSequence class:");
                    println!("  name: {}", name);
                    if let Some(parent) = class.get("parent").and_then(|v| v.as_str()) {
                        println!("  parent: {}", parent);
                    }
                    if let Some(prefix) = class.get("prefix").and_then(|v| v.as_str()) {
                        println!("  prefix: {}", prefix);
                    }
                }
            }
        }
    }
}
