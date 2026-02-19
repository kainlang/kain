use crate::material_graph::*;
use kain_core::ast::{MaterialGraphDef, MaterialStatement, MaterialInput, MaterialOutput, Expr, BinaryOp, Type, CallArg};
use std::collections::HashMap;

/// Converts KAIN AST material graph definitions to MaterialGraph IR
pub struct MaterialGraphConverter {
    node_counter: usize,
    variable_map: HashMap<String, String>, // var name → node id
}

impl MaterialGraphConverter {
    pub fn new() -> Self {
        Self {
            node_counter: 0,
            variable_map: HashMap::new(),
        }
    }

    /// Convert a MaterialGraphDef AST node to MaterialGraph IR
    pub fn convert(&mut self, def: &MaterialGraphDef) -> Result<MaterialGraph, String> {
        let mut graph = MaterialGraph::new(def.name.clone());
        
        // Extract properties from attributes
        graph.properties = self.extract_properties(&def.attributes)?;
        
        // Convert inputs to parameter nodes
        for input in &def.inputs {
            let node_id = self.create_input_node(&mut graph, input)?;
            self.variable_map.insert(input.name.clone(), node_id);
        }
        
        // Convert body statements (let bindings)
        for stmt in &def.body {
            match stmt {
                MaterialStatement::Let { name, value, .. } => {
                    let node_id = self.convert_expr(&mut graph, value)?;
                    self.variable_map.insert(name.clone(), node_id);
                }
            }
        }
        
        // Convert outputs
        for output in &def.outputs {
            let node_id = self.convert_expr(&mut graph, &output.value)?;
            self.set_output(&mut graph, &output.name, node_id)?;
        }
        
        Ok(graph)
    }

    fn next_node_id(&mut self) -> String {
        let id = format!("node_{}", self.node_counter);
        self.node_counter += 1;
        id
    }

    fn create_input_node(&mut self, graph: &mut MaterialGraph, input: &MaterialInput) -> Result<String, String> {
        let node_id = self.next_node_id();
        let x = -400;
        let y = (graph.nodes.len() as i32) * 100;
        
        let node = match &input.ty {
            Type::Named { name, .. } if name == "Float" => {
                let default = self.extract_float_default(&input.default)?;
                MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::ScalarParameter {
                        name: input.name.clone(),
                        default,
                    },
                    position: (x, y),
                }
            }
            Type::Named { name, .. } if name == "Vec3" => {
                let default = self.extract_vec3_default(&input.default)?;
                MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::VectorParameter {
                        name: input.name.clone(),
                        default,
                    },
                    position: (x, y),
                }
            }
            Type::Named { name, .. } if name == "Vec4" => {
                let default = self.extract_vec4_default(&input.default)?;
                MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::VectorParameter {
                        name: input.name.clone(),
                        default: [default[0], default[1], default[2]], // Convert Vec4 to Vec3
                    },
                    position: (x, y),
                }
            }
            Type::Named { name, .. } if name == "Sampler2D" => {
                MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::TextureParameter {
                        name: input.name.clone(),
                        default_path: None,
                    },
                    position: (x, y),
                }
            }
            _ => return Err(format!("Unsupported input type: {:?}", input.ty)),
        };
        
        graph.nodes.push(node);
        Ok(node_id)
    }

    fn convert_expr(&mut self, graph: &mut MaterialGraph, expr: &Expr) -> Result<String, String> {
        match expr {
            // Variable reference
            Expr::Ident(name, _) => {
                self.variable_map.get(name)
                    .cloned()
                    .ok_or_else(|| format!("Undefined variable: {}", name))
            }
            
            // Binary operations
            Expr::Binary { op, left, right, .. } => {
                let left_id = self.convert_expr(graph, left)?;
                let right_id = self.convert_expr(graph, right)?;
                
                let node_id = self.next_node_id();
                let x = -200;
                let y = (graph.nodes.len() as i32) * 100;
                
                let node_type = match op {
                    BinaryOp::Mul => MaterialNodeType::Multiply {
                        a: left_id,
                        b: right_id,
                    },
                    BinaryOp::Add => MaterialNodeType::Add {
                        a: left_id,
                        b: right_id,
                    },
                    BinaryOp::Sub => MaterialNodeType::Subtract {
                        a: left_id,
                        b: right_id,
                    },
                    BinaryOp::Div => MaterialNodeType::Divide {
                        a: left_id,
                        b: right_id,
                    },
                    _ => return Err(format!("Unsupported binary op: {:?}", op)),
                };
                
                graph.nodes.push(MaterialNode {
                    id: node_id.clone(),
                    node_type,
                    position: (x, y),
                });
                
                Ok(node_id)
            }
            
            // Function calls (sin, cos, sample, vec3, vec4, etc.)
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = &**callee {
                    match name.as_str() {
                        "sin" => {
                            if args.len() != 1 {
                                return Err(format!("sin() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Sine { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "cos" => {
                            if args.len() != 1 {
                                return Err(format!("cos() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Cosine { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "sample" => {
                            if args.len() != 2 {
                                return Err(format!("sample() expects 2 arguments (texture, uv), got {}", args.len()));
                            }
                            let texture_id = self.convert_expr(graph, &args[0].value)?;
                            let uv_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::TextureSample {
                                    texture: texture_id,
                                    uv: uv_id,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "vec3" => {
                            // vec3 constructor - convert to constant or component mask
                            if args.len() == 3 {
                                // vec3(x, y, z) - create constant vector
                                let x = self.extract_float_from_expr(&args[0].value)?;
                                let y = self.extract_float_from_expr(&args[1].value)?;
                                let z = self.extract_float_from_expr(&args[2].value)?;
                                
                                let node_id = self.next_node_id();
                                let pos_x = -400;
                                let pos_y = (graph.nodes.len() as i32) * 100;
                                
                                graph.nodes.push(MaterialNode {
                                    id: node_id.clone(),
                                    node_type: MaterialNodeType::ConstantVector { value: [x, y, z] },
                                    position: (pos_x, pos_y),
                                });
                                
                                Ok(node_id)
                            } else {
                                Err(format!("vec3() expects 3 arguments, got {}", args.len()))
                            }
                        }
                        "vec4" => {
                            // vec4 constructor - convert to constant vector (drop w component)
                            if args.len() == 4 {
                                let x = self.extract_float_from_expr(&args[0].value)?;
                                let y = self.extract_float_from_expr(&args[1].value)?;
                                let z = self.extract_float_from_expr(&args[2].value)?;
                                // Drop w component for now
                                
                                let node_id = self.next_node_id();
                                let pos_x = -400;
                                let pos_y = (graph.nodes.len() as i32) * 100;
                                
                                graph.nodes.push(MaterialNode {
                                    id: node_id.clone(),
                                    node_type: MaterialNodeType::ConstantVector { value: [x, y, z] },
                                    position: (pos_x, pos_y),
                                });
                                
                                Ok(node_id)
                            } else {
                                Err(format!("vec4() expects 4 arguments, got {}", args.len()))
                            }
                        }
                        _ => Err(format!("Unknown function: {}", name)),
                    }
                } else {
                    Err("Complex function calls not supported".to_string())
                }
            }
            
            // Field access (e.g., texture.rgb, color.r)
            Expr::Field { object, field, .. } => {
                let object_id = self.convert_expr(graph, object)?;
                let node_id = self.next_node_id();
                let x = -200;
                let y = (graph.nodes.len() as i32) * 100;
                
                // Convert field name to component mask
                let mask = match field.as_str() {
                    "r" | "x" => "R",
                    "g" | "y" => "G",
                    "b" | "z" => "B",
                    "a" | "w" => "A",
                    "rg" | "xy" => "RG",
                    "rgb" | "xyz" => "RGB",
                    "rgba" | "xyzw" => "RGBA",
                    _ => return Err(format!("Unknown component mask: {}", field)),
                };
                
                graph.nodes.push(MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::ComponentMask {
                        input: object_id,
                        mask: mask.to_string(),
                    },
                    position: (x, y),
                });
                
                Ok(node_id)
            }
            
            // Literals
            Expr::Float(value, _) => {
                let node_id = self.next_node_id();
                let x = -400;
                let y = (graph.nodes.len() as i32) * 100;
                
                graph.nodes.push(MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::ConstantFloat { value: *value as f32 },
                    position: (x, y),
                });
                
                Ok(node_id)
            }
            
            Expr::Int(value, _) => {
                let node_id = self.next_node_id();
                let x = -400;
                let y = (graph.nodes.len() as i32) * 100;
                
                graph.nodes.push(MaterialNode {
                    id: node_id.clone(),
                    node_type: MaterialNodeType::ConstantFloat { value: *value as f32 },
                    position: (x, y),
                });
                
                Ok(node_id)
            }
            
            _ => Err(format!("Unsupported expression: {:?}", expr)),
        }
    }

    fn extract_properties(&self, attributes: &[kain_core::ast::Attribute]) -> Result<MaterialProperties, String> {
        let mut props = MaterialProperties::default();
        
        for attr in attributes {
            if attr.name == "material_graph" {
                // Parse attribute arguments
                for arg in &attr.args {
                    // Expected format: Ident("blend_mode") or Call with named args
                    // For now, we'll use defaults
                    // TODO: Parse actual attribute arguments when format is defined
                }
            }
        }
        
        Ok(props)
    }

    fn extract_float_default(&self, default: &Option<Expr>) -> Result<f32, String> {
        match default {
            Some(Expr::Float(value, _)) => Ok(*value as f32),
            Some(Expr::Int(value, _)) => Ok(*value as f32),
            None => Ok(0.0),
            _ => Err("Invalid float default value".to_string()),
        }
    }

    fn extract_vec3_default(&self, default: &Option<Expr>) -> Result<[f32; 3], String> {
        match default {
            Some(Expr::Call { callee, args, .. }) => {
                if let Expr::Ident(name, _) = &**callee {
                    if name == "vec3" && args.len() == 3 {
                        let x = self.extract_float_from_expr(&args[0].value)?;
                        let y = self.extract_float_from_expr(&args[1].value)?;
                        let z = self.extract_float_from_expr(&args[2].value)?;
                        return Ok([x, y, z]);
                    }
                }
                Err("Invalid vec3 constructor".to_string())
            }
            None => Ok([0.0, 0.0, 0.0]),
            _ => Err("Invalid vec3 default value".to_string()),
        }
    }

    fn extract_vec4_default(&self, default: &Option<Expr>) -> Result<[f32; 4], String> {
        match default {
            Some(Expr::Call { callee, args, .. }) => {
                if let Expr::Ident(name, _) = &**callee {
                    if name == "vec4" && args.len() == 4 {
                        let x = self.extract_float_from_expr(&args[0].value)?;
                        let y = self.extract_float_from_expr(&args[1].value)?;
                        let z = self.extract_float_from_expr(&args[2].value)?;
                        let w = self.extract_float_from_expr(&args[3].value)?;
                        return Ok([x, y, z, w]);
                    }
                }
                Err("Invalid vec4 constructor".to_string())
            }
            None => Ok([0.0, 0.0, 0.0, 1.0]),
            _ => Err("Invalid vec4 default value".to_string()),
        }
    }

    fn extract_float_from_expr(&self, expr: &Expr) -> Result<f32, String> {
        match expr {
            Expr::Float(value, _) => Ok(*value as f32),
            Expr::Int(value, _) => Ok(*value as f32),
            _ => Err("Expected numeric value".to_string()),
        }
    }

    fn set_output(&self, graph: &mut MaterialGraph, name: &str, node_id: String) -> Result<(), String> {
        match name {
            "base_color" => graph.outputs.base_color = Some(node_id),
            "emissive" => graph.outputs.emissive = Some(node_id),
            "roughness" => graph.outputs.roughness = Some(node_id),
            "metallic" => graph.outputs.metallic = Some(node_id),
            "normal" => graph.outputs.normal = Some(node_id),
            "opacity" => graph.outputs.opacity = Some(node_id),
            "specular" => graph.outputs.specular = Some(node_id),
            "ambient_occlusion" => graph.outputs.ambient_occlusion = Some(node_id),
            _ => return Err(format!("Unknown output: {}", name)),
        }
        Ok(())
    }
}

impl Default for MaterialGraphConverter {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Span, Type};

    #[test]
    fn test_simple_material_conversion() {
        let mut converter = MaterialGraphConverter::new();
        
        // Create a simple material graph AST
        let def = MaterialGraphDef {
            name: "TestMaterial".to_string(),
            attributes: vec![],
            inputs: vec![
                MaterialInput {
                    name: "base_color".to_string(),
                    ty: Type::Named {
                        name: "Vec3".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    default: Some(Expr::Call {
                        callee: Box::new(Expr::Ident("vec3".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Float(1.0, Span::default()) },
                            CallArg { name: None, value: Expr::Float(0.0, Span::default()) },
                            CallArg { name: None, value: Expr::Float(0.0, Span::default()) },
                        ],
                        span: Span::default(),
                    }),
                    span: Span::default(),
                },
            ],
            body: vec![],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("base_color".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok());
        
        let graph = result.unwrap();
        assert_eq!(graph.name, "TestMaterial");
        assert_eq!(graph.nodes.len(), 1); // One parameter node
        assert!(graph.outputs.base_color.is_some());
    }

    #[test]
    fn test_binary_operations() {
        let mut converter = MaterialGraphConverter::new();
        
        let def = MaterialGraphDef {
            name: "MathMaterial".to_string(),
            attributes: vec![],
            inputs: vec![
                MaterialInput {
                    name: "a".to_string(),
                    ty: Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    default: Some(Expr::Float(1.0, Span::default())),
                    span: Span::default(),
                },
                MaterialInput {
                    name: "b".to_string(),
                    ty: Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    default: Some(Expr::Float(2.0, Span::default())),
                    span: Span::default(),
                },
            ],
            body: vec![],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Binary {
                        left: Box::new(Expr::Ident("a".to_string(), Span::default())),
                        op: BinaryOp::Mul,
                        right: Box::new(Expr::Ident("b".to_string(), Span::default())),
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok());
        
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 3); // 2 parameters + 1 multiply
    }
}
