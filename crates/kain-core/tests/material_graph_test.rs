use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::ast::Item;

#[test]
fn test_material_graph_parsing() {
    let source = r#"
@material_graph
material HologramMaterial:
    input glow_intensity: Float = 1.0
    input glow_color: Vec3 = vec3(0, 1, 1)
    
    let scan = sin(uv.y * 10.0)
    let glow = glow_color * scan * glow_intensity
    
    output base_color = glow
    output emissive = glow * 2.0
"#;

    let tokens = Lexer::new(source).tokenize().expect("Lexer should succeed");
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().expect("Parser should succeed");
    
    assert_eq!(program.items.len(), 1, "Should have 1 item");
    
    match &program.items[0] {
        Item::MaterialGraph(mat) => {
            assert_eq!(mat.name, "HologramMaterial");
            assert_eq!(mat.attributes.len(), 1);
            assert_eq!(mat.attributes[0].name, "material_graph");
            
            assert_eq!(mat.inputs.len(), 2);
            assert_eq!(mat.inputs[0].name, "glow_intensity");
            assert_eq!(mat.inputs[1].name, "glow_color");
            
            assert_eq!(mat.body.len(), 2);
            
            assert_eq!(mat.outputs.len(), 2);
            assert_eq!(mat.outputs[0].name, "base_color");
            assert_eq!(mat.outputs[1].name, "emissive");
        }
        _ => panic!("Expected MaterialGraph item"),
    }
}

#[test]
fn test_material_graph_minimal() {
    let source = r#"
@material_graph
material SimpleMaterial:
    input color: Vec3
    output base_color = color
"#;

    let tokens = Lexer::new(source).tokenize().expect("Lexer should succeed");
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().expect("Parser should succeed");
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::MaterialGraph(mat) => {
            assert_eq!(mat.name, "SimpleMaterial");
            assert_eq!(mat.inputs.len(), 1);
            assert_eq!(mat.body.len(), 0);
            assert_eq!(mat.outputs.len(), 1);
        }
        _ => panic!("Expected MaterialGraph item"),
    }
}
