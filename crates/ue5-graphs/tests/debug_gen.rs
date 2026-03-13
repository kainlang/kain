use ue5_graphs::{generate_graph_instance, GraphEditor};

fn main() {
    let mut graph = GraphEditor::new("CombatGraph");
    graph.properties.allow_cycles = false;

    let result = generate_graph_instance(&graph, "CombatPlugin");

    if let Ok(output) = result {
        println!("=== INSTANCE HEADER ===");
        println!("{}", output.instance_header.1);
        println!("\n=== NODE DATA HEADER ===");
        println!("{}", output.node_data_header.1);
    } else {
        println!("Error: {:?}", result.err());
    }
}
