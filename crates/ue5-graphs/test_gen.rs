use ue5_graphs::{GraphEditor, runtime_codegen::InstanceGenerator};

fn main() {
    let graph = GraphEditor::new("TestGraph");
    let gen = InstanceGenerator::new(&graph, "TestPlugin");
    let header = gen.generate_instance_header().unwrap();
    
    // Print first 500 chars
    println!("{}", &header[..500.min(header.len())]);
    
    // Check for class name
    if header.contains("class UTestGraphInstance") {
        println!("\n✓ Contains 'class UTestGraphInstance'");
    } else {
        println!("\n✗ Does NOT contain 'class UTestGraphInstance'");
        // Search for what it does contain
        if let Some(pos) = header.find("class U") {
            let snippet = &header[pos..pos+50.min(header.len()-pos)];
            println!("Found instead: {}", snippet);
        }
    }
}
