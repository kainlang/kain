use crate::material_graph::*;
use kain_core::ast::{MaterialGraphDef, MaterialStatement, MaterialInput, MaterialOutput, Expr, BinaryOp, Type, CallArg};
use std::collections::HashMap;

/// Converts KAIN AST material graph definitions to MaterialGraph IR
pub struct MaterialGraphConverter {
    node_counter: usize,
    variable_map: HashMap<String, String>, // var name → node id
    default_uv_node: Option<String>, // Cached default UV node ID for deduplication
    time_node_id: Option<String>, // Cached Time node ID for deduplication (Feature 6)
    texture_param_nodes: HashMap<String, String>, // texture param name → node id (for deduplication - Feature 4)
}

impl MaterialGraphConverter {
    pub fn new() -> Self {
        Self {
            node_counter: 0,
            variable_map: HashMap::new(),
            default_uv_node: None,
            time_node_id: None,
            texture_param_nodes: HashMap::new(),
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
        
        // Feature 7.1: Mark all parameters as dynamic if expose_parameters is enabled
        if graph.properties.expose_parameters {
            for input in &def.inputs {
                // Only mark scalar/vector/color parameters as dynamic (not textures)
                if matches!(input.ty, Type::Named { ref name, .. } if name == "Float" || name == "Vec3" || name == "Vec4") {
                    if let Err(e) = graph.mark_parameter_dynamic(&input.name) {
                        eprintln!("Warning: Failed to mark parameter '{}' as dynamic: {}", input.name, e);
                    }
                }
            }
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
                    node_type: MaterialNodeType::TextureSampleParameter2D {
                        param_name: input.name.clone(),
                        default_texture: None,
                        uv_input: None,
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
                        "time" => {
                            // time() - returns engine time for animations
                            // Feature 6: Time-Based Effects - Validates Requirements 6.1
                            if !args.is_empty() {
                                return Err(format!("time() expects 0 arguments, got {}", args.len()));
                            }
                            
                            // Use create_time_node() for deduplication
                            let node_id = self.create_time_node(graph);
                            Ok(node_id)
                        }
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
                        "lerp" => {
                            if args.len() != 3 {
                                return Err(format!("lerp() expects 3 arguments (a, b, alpha), got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let alpha_id = self.convert_expr(graph, &args[2].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Lerp { a: a_id, b: b_id, alpha: alpha_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "clamp" => {
                            if args.len() != 3 {
                                return Err(format!("clamp() expects 3 arguments (input, min, max), got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let min_id = self.convert_expr(graph, &args[1].value)?;
                            let max_id = self.convert_expr(graph, &args[2].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Clamp { input: input_id, min: min_id, max: max_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "pow" => {
                            if args.len() != 2 {
                                return Err(format!("pow() expects 2 arguments (base, exponent), got {}", args.len()));
                            }
                            let base_id = self.convert_expr(graph, &args[0].value)?;
                            let exp_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Power { base: base_id, exponent: exp_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "dot" => {
                            if args.len() != 2 {
                                return Err(format!("dot() expects 2 arguments, got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Dot { a: a_id, b: b_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "cross" => {
                            if args.len() != 2 {
                                return Err(format!("cross() expects 2 arguments, got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Cross { a: a_id, b: b_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "normalize" => {
                            if args.len() != 1 {
                                return Err(format!("normalize() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Normalize { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "length" => {
                            if args.len() != 1 {
                                return Err(format!("length() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Length { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "distance" => {
                            if args.len() != 2 {
                                return Err(format!("distance() expects 2 arguments, got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Distance { a: a_id, b: b_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "abs" => {
                            if args.len() != 1 {
                                return Err(format!("abs() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Abs { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "min" => {
                            if args.len() != 2 {
                                return Err(format!("min() expects 2 arguments, got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Min { a: a_id, b: b_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "max" => {
                            if args.len() != 2 {
                                return Err(format!("max() expects 2 arguments, got {}", args.len()));
                            }
                            let a_id = self.convert_expr(graph, &args[0].value)?;
                            let b_id = self.convert_expr(graph, &args[1].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Max { a: a_id, b: b_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "saturate" => {
                            if args.len() != 1 {
                                return Err(format!("saturate() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Saturate { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "frac" => {
                            if args.len() != 1 {
                                return Err(format!("frac() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Frac { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "floor" => {
                            if args.len() != 1 {
                                return Err(format!("floor() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Floor { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "ceil" => {
                            if args.len() != 1 {
                                return Err(format!("ceil() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Ceil { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "round" => {
                            if args.len() != 1 {
                                return Err(format!("round() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Round { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "sqrt" => {
                            if args.len() != 1 {
                                return Err(format!("sqrt() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Sqrt { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "exp" => {
                            if args.len() != 1 {
                                return Err(format!("exp() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Exp { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "log" => {
                            if args.len() != 1 {
                                return Err(format!("log() expects 1 argument, got {}", args.len()));
                            }
                            let input_id = self.convert_expr(graph, &args[0].value)?;
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::Log { input: input_id },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "sample" | "texture_sample" => {
                            // sample(texture, uv) or sample(texture) - UV is optional
                            // Validates Requirements: 4.1, 4.2, 4.3, 4.5
                            if args.is_empty() || args.len() > 2 {
                                return Err(format!("sample() expects 1 or 2 arguments (texture, [uv]), got {}", args.len()));
                            }
                            
                            // Get texture argument - should be a variable reference to a texture input
                            let texture_arg = &args[0].value;
                            let texture_name = match texture_arg {
                                Expr::Ident(name, _) => name.clone(),
                                _ => return Err("sample() first argument must be a texture input variable".to_string()),
                            };
                            
                            // Get or create UV coordinates
                            let uv_id = if args.len() == 2 {
                                // Explicit UV provided
                                self.convert_expr(graph, &args[1].value)?
                            } else {
                                // No UV provided - use default texture coordinates
                                self.create_default_uv_node(graph)
                            };
                            
                            // Create a unique key for this texture+UV combination
                            // This allows deduplication when the same texture is sampled with the same UVs
                            let cache_key = format!("{}_{}", texture_name, uv_id);
                            
                            // Check if we already have a sample node for this texture+UV combination
                            if let Some(existing_node_id) = self.texture_param_nodes.get(&cache_key) {
                                // Reuse existing node (deduplication)
                                return Ok(existing_node_id.clone());
                            }
                            
                            // Create new TextureSampleParameter2D node
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::TextureSampleParameter2D {
                                    param_name: texture_name,
                                    default_texture: None,
                                    uv_input: Some(uv_id),
                                },
                                position: (x, y),
                            });
                            
                            // Cache the node for future deduplication
                            self.texture_param_nodes.insert(cache_key, node_id.clone());
                            
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
                                    node_type: MaterialNodeType::ConstantVec3 { value: [x, y, z] },
                                    position: (pos_x, pos_y),
                                });
                                
                                Ok(node_id)
                            } else {
                                Err(format!("vec3() expects 3 arguments, got {}", args.len()))
                            }
                        }
                        "vec4" => {
                            // vec4 constructor - convert to constant 4-vector
                            if args.len() == 4 {
                                let x = self.extract_float_from_expr(&args[0].value)?;
                                let y = self.extract_float_from_expr(&args[1].value)?;
                                let z = self.extract_float_from_expr(&args[2].value)?;
                                let w = self.extract_float_from_expr(&args[3].value)?;
                                
                                let node_id = self.next_node_id();
                                let pos_x = -400;
                                let pos_y = (graph.nodes.len() as i32) * 100;
                                
                                graph.nodes.push(MaterialNode {
                                    id: node_id.clone(),
                                    node_type: MaterialNodeType::ConstantVec4 { value: [x, y, z, w] },
                                    position: (pos_x, pos_y),
                                });
                                
                                Ok(node_id)
                            } else {
                                Err(format!("vec4() expects 4 arguments, got {}", args.len()))
                            }
                        }
                        "call_shader" => {
                            // call_shader("ShaderName", param1, param2, ...)
                            // First argument must be a string literal (shader name)
                            // Remaining arguments are shader parameters
                            
                            if args.is_empty() {
                                return Err("call_shader() requires at least 1 argument (shader name)".to_string());
                            }
                            
                            // Extract shader name from first argument
                            let shader_name = match &args[0].value {
                                Expr::String(s, _) => s.clone(),
                                Expr::Ident(s, _) => s.clone(), // Allow identifiers too
                                _ => return Err("call_shader() first argument must be a string literal or identifier".to_string()),
                            };
                            
                            // Convert remaining arguments to node IDs
                            let mut input_ids = Vec::new();
                            for arg in &args[1..] {
                                let input_id = self.convert_expr(graph, &arg.value)?;
                                input_ids.push(input_id);
                            }
                            
                            // Resolve shader path
                            let function_path = self.resolve_shader_path(&shader_name)?;
                            
                            // Create the MaterialFunctionCall node
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::MaterialFunctionCall {
                                    function_path,
                                    inputs: input_ids,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "custom_hlsl" => {
                            // custom_hlsl("""code""", output_type: "float3", inputs: [(Input0, "float3"), (Input1, "float3")])
                            // First argument must be a string literal (HLSL code)
                            // Named arguments: output_type (string), inputs (array of tuples)
                            
                            if args.is_empty() {
                                return Err("custom_hlsl() requires at least 1 argument (HLSL code)".to_string());
                            }
                            
                            // Extract HLSL code from first argument
                            let code = match &args[0].value {
                                Expr::String(s, _) => s.clone(),
                                _ => return Err("custom_hlsl() first argument must be a string literal".to_string()),
                            };
                            
                            // Extract named arguments
                            let mut output_type = CustomOutputType::Float3; // default
                            let mut inputs = Vec::new();
                            
                            for arg in &args[1..] {
                                match arg.name.as_deref() {
                                    Some("output_type") => {
                                        // Parse output_type string
                                        let type_str = match &arg.value {
                                            Expr::String(s, _) => s.as_str(),
                                            _ => return Err("output_type must be a string literal".to_string()),
                                        };
                                        
                                        output_type = match type_str {
                                            "float1" | "float" => CustomOutputType::Float1,
                                            "float2" => CustomOutputType::Float2,
                                            "float3" => CustomOutputType::Float3,
                                            "float4" => CustomOutputType::Float4,
                                            _ => return Err(format!("Invalid output_type: '{}'. Must be 'float1', 'float2', 'float3', or 'float4'", type_str)),
                                        };
                                    }
                                    Some("inputs") => {
                                        // Parse inputs array: [(Input0, "float3"), (Input1, "float3")]
                                        let input_array = match &arg.value {
                                            Expr::Array(items, _) => items,
                                            _ => return Err("inputs must be an array literal".to_string()),
                                        };
                                        
                                        for item in input_array {
                                            // Each item should be a tuple: (InputName, "type")
                                            let (input_name, input_type_str) = match item {
                                                Expr::Tuple(elements, _) if elements.len() == 2 => {
                                                    let name = match &elements[0] {
                                                        Expr::Ident(n, _) => n.clone(),
                                                        _ => return Err("Input name must be an identifier".to_string()),
                                                    };
                                                    let type_str = match &elements[1] {
                                                        Expr::String(s, _) => s.as_str(),
                                                        _ => return Err("Input type must be a string literal".to_string()),
                                                    };
                                                    (name, type_str)
                                                }
                                                _ => return Err("Each input must be a tuple (name, type)".to_string()),
                                            };
                                            
                                            let input_type = match input_type_str {
                                                "float1" | "float" => CustomOutputType::Float1,
                                                "float2" => CustomOutputType::Float2,
                                                "float3" => CustomOutputType::Float3,
                                                "float4" => CustomOutputType::Float4,
                                                _ => return Err(format!("Invalid input type: '{}'. Must be 'float1', 'float2', 'float3', or 'float4'", input_type_str)),
                                            };
                                            
                                            inputs.push(CustomInput {
                                                name: input_name,
                                                input_type,
                                            });
                                        }
                                    }
                                    Some(other) => {
                                        return Err(format!("Unknown named argument for custom_hlsl(): '{}'", other));
                                    }
                                    None => {
                                        return Err("custom_hlsl() arguments after the first must be named (output_type: ..., inputs: ...)".to_string());
                                    }
                                }
                            }
                            
                            // Create the CustomHLSL node
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::CustomHLSL {
                                    code,
                                    output_type,
                                    inputs,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "uv_scroll" => {
                            // uv_scroll(uv, offset_x, offset_y)
                            if args.len() != 3 {
                                return Err(format!("uv_scroll() expects 3 arguments (uv, offset_x, offset_y), got {}", args.len()));
                            }
                            
                            let uv_id = self.convert_expr(graph, &args[0].value)?;
                            let offset_x_id = self.convert_expr(graph, &args[1].value)?;
                            let offset_y_id = self.convert_expr(graph, &args[2].value)?;
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::UVScroll {
                                    uv_input: uv_id,
                                    offset_x: offset_x_id,
                                    offset_y: offset_y_id,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "uv_scale" => {
                            // uv_scale(uv, scale_x, scale_y)
                            if args.len() != 3 {
                                return Err(format!("uv_scale() expects 3 arguments (uv, scale_x, scale_y), got {}", args.len()));
                            }
                            
                            let uv_id = self.convert_expr(graph, &args[0].value)?;
                            let scale_x_id = self.convert_expr(graph, &args[1].value)?;
                            let scale_y_id = self.convert_expr(graph, &args[2].value)?;
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::UVScale {
                                    uv_input: uv_id,
                                    scale_x: scale_x_id,
                                    scale_y: scale_y_id,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "uv_rotate" => {
                            // uv_rotate(uv, angle) or uv_rotate(uv, angle, center_x, center_y)
                            if args.len() != 2 && args.len() != 4 {
                                return Err(format!("uv_rotate() expects 2 or 4 arguments (uv, angle) or (uv, angle, center_x, center_y), got {}", args.len()));
                            }
                            
                            let uv_id = self.convert_expr(graph, &args[0].value)?;
                            let angle_id = self.convert_expr(graph, &args[1].value)?;
                            
                            let center = if args.len() == 4 {
                                let center_x_id = self.convert_expr(graph, &args[2].value)?;
                                let center_y_id = self.convert_expr(graph, &args[3].value)?;
                                Some((center_x_id, center_y_id))
                            } else {
                                None
                            };
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::UVRotate {
                                    uv_input: uv_id,
                                    angle: angle_id,
                                    center,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "world_position" => {
                            // world_position() - returns absolute world position
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.1
                            if !args.is_empty() {
                                return Err(format!("world_position() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::WorldPosition,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "world_normal" => {
                            // world_normal() - returns world-space vertex normal
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.2
                            if !args.is_empty() {
                                return Err(format!("world_normal() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::WorldNormal,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "absolute_world_position" => {
                            // absolute_world_position() - returns absolute world position (no camera offset)
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.1
                            if !args.is_empty() {
                                return Err(format!("absolute_world_position() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::AbsoluteWorldPosition,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "camera_position" => {
                            // camera_position() - returns world-space camera position
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.3
                            if !args.is_empty() {
                                return Err(format!("camera_position() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::CameraPosition,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "object_position" => {
                            // object_position() - returns object pivot world position
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.1
                            if !args.is_empty() {
                                return Err(format!("object_position() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::ObjectPosition,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "object_orientation" => {
                            // object_orientation() - returns object rotation as vector
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.1
                            if !args.is_empty() {
                                return Err(format!("object_orientation() expects 0 arguments, got {}", args.len()));
                            }
                            
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::ObjectOrientation,
                                position: (x, y),
                            });
                            
                            Ok(node_id)
                        }
                        "triplanar_sample" => {
                            // triplanar_sample(texture, [world_position], [blend_sharpness])
                            // Feature 7.4: World-Space Operations - Validates Requirements 7.4.4
                            if args.is_empty() || args.len() > 3 {
                                return Err(format!("triplanar_sample() expects 1-3 arguments (texture, [world_position], [blend_sharpness]), got {}", args.len()));
                            }
                            
                            // Get texture argument - should be a variable reference to a texture input
                            let texture_arg = &args[0].value;
                            let texture_name = match texture_arg {
                                Expr::Ident(name, _) => name.clone(),
                                _ => return Err("triplanar_sample() first argument must be a texture input variable".to_string()),
                            };
                            
                            // Get texture node ID from variable map
                            let texture_id = self.variable_map.get(&texture_name)
                                .cloned()
                                .ok_or_else(|| format!("Undefined texture variable: {}", texture_name))?;
                            
                            // Get optional world position
                            let world_position = if args.len() >= 2 {
                                Some(self.convert_expr(graph, &args[1].value)?)
                            } else {
                                None
                            };
                            
                            // Get optional blend sharpness (default: 4.0)
                            let blend_sharpness = if args.len() >= 3 {
                                self.extract_float_from_expr(&args[2].value)?
                            } else {
                                4.0
                            };
                            
                            // Create triplanar sample node
                            let node_id = self.next_node_id();
                            let x = -200;
                            let y = (graph.nodes.len() as i32) * 100;
                            
                            graph.nodes.push(MaterialNode {
                                id: node_id.clone(),
                                node_type: MaterialNodeType::TriplanarSample {
                                    texture: texture_id,
                                    world_position,
                                    blend_sharpness,
                                },
                                position: (x, y),
                            });
                            
                            Ok(node_id)
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
            // Feature 7.1: Detect @dynamic attribute for runtime parameter modification
            if attr.name == "dynamic" {
                props.expose_parameters = true;
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

    /// Create or reuse a Time node for time-based effects
    /// Implements deduplication - only one Time node per material
    /// Feature 6: Time-Based Effects - Validates Requirements 6.1, 6.5
    fn create_time_node(&mut self, graph: &mut MaterialGraph) -> String {
        // Return cached node if it exists
        if let Some(node_id) = &self.time_node_id {
            return node_id.clone();
        }
        
        // Create new Time node
        let node_id = self.next_node_id();
        let x = -400;
        let y = (graph.nodes.len() as i32) * 100;
        
        graph.nodes.push(MaterialNode {
            id: node_id.clone(),
            node_type: MaterialNodeType::Time,
            position: (x, y),
        });
        
        // Mark material as dynamic (time-based materials cannot be static)
        // Feature 6: Time-Based Effects - Validates Requirements 6.5
        graph.is_dynamic = true;
        
        // Cache for future use
        self.time_node_id = Some(node_id.clone());
        node_id
    }

    fn set_output(&mut self, graph: &mut MaterialGraph, name: &str, node_id: String) -> Result<(), String> {
        match name {
            "base_color" => graph.outputs.base_color = Some(node_id),
            "emissive" => graph.outputs.emissive = Some(node_id),
            "roughness" => graph.outputs.roughness = Some(node_id),
            "metallic" => graph.outputs.metallic = Some(node_id),
            "normal" => graph.outputs.normal = Some(node_id),
            "opacity" => graph.outputs.opacity = Some(node_id),
            "specular" => graph.outputs.specular = Some(node_id),
            "ambient_occlusion" => graph.outputs.ambient_occlusion = Some(node_id),
            "world_position_offset" => {
                // Phase 7.5: Mark graph as using vertex shader when WorldPositionOffset is connected
                graph.outputs.world_position_offset = Some(node_id);
                graph.uses_vertex_shader = true;
            }
            _ => return Err(format!("Unknown output: {}", name)),
        }
        Ok(())
    }

    /// Create or return cached default UV coordinate node (TextureCoordinate with index 0)
    /// Validates Requirements: 4.3, 5.5
    /// This ensures only one TextureCoordinate node is created per material
    fn create_default_uv_node(&mut self, graph: &mut MaterialGraph) -> String {
        // Return cached node if already created
        if let Some(node_id) = &self.default_uv_node {
            return node_id.clone();
        }

        // Create new TextureCoordinate node with index 0 (default UVs)
        let node_id = self.next_node_id();
        let x = -400;
        let y = (graph.nodes.len() as i32) * 100;

        graph.nodes.push(MaterialNode {
            id: node_id.clone(),
            node_type: MaterialNodeType::TextureCoordinate {
                index: 0,
                tiling: [1.0, 1.0],
            },
            position: (x, y),
        });

        // Cache the node ID for future use
        self.default_uv_node = Some(node_id.clone());
        node_id
    }
    
    /// Resolve a KAIN shader name to a UE5 material function path
    /// 
    /// This method maps KAIN shader names to their corresponding UE5 material function assets.
    /// The function path follows UE5's asset path convention: /Game/Materials/Functions/ShaderName
    /// 
    /// # Arguments
    /// * `shader_name` - The name of the KAIN shader to resolve
    /// 
    /// # Returns
    /// * `Ok(String)` - The resolved UE5 material function path
    /// * `Err(String)` - Error message if shader not found
    /// 
    /// # Examples
    /// ```ignore
    /// // Resolves "MyCustomShader" to "/Game/Materials/Functions/MyCustomShader"
    /// let path = converter.resolve_shader_path("MyCustomShader")?;
    /// ```
    fn resolve_shader_path(&self, shader_name: &str) -> Result<String, String> {
        // For now, we use a simple naming convention:
        // KAIN shader "ShaderName" → UE5 material function "/Game/Materials/Functions/ShaderName"
        // 
        // Future enhancements:
        // - Look up shader definitions in a registry
        // - Validate shader exists before generating call
        // - Support custom shader paths via configuration
        // - Handle shader namespaces and modules
        
        if shader_name.is_empty() {
            return Err("Shader name cannot be empty".to_string());
        }
        
        // Validate shader name (basic identifier rules)
        if !shader_name.chars().all(|c| c.is_alphanumeric() || c == '_') {
            return Err(format!(
                "Invalid shader name '{}': must contain only alphanumeric characters and underscores",
                shader_name
            ));
        }
        
        // Construct UE5 material function path
        // Format: /Game/Materials/Functions/{ShaderName}
        Ok(format!("/Game/Materials/Functions/{}", shader_name))
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
    use kain_core::span::Span;
    use kain_core::ast::Type;

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
                            CallArg { name: None, value: Expr::Float(1.0, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.0, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.0, Span::default()), span: Span::default() },
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

    #[test]
    fn test_custom_hlsl_parsing() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test custom_hlsl() with all parameters
        let def = MaterialGraphDef {
            name: "CustomHLSLMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "custom_effect".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("custom_hlsl".to_string(), Span::default())),
                        args: vec![
                            // First arg: HLSL code string
                            CallArg {
                                name: None,
                                value: Expr::String("float3 result = Input0 * Input1;\nreturn result;".to_string(), Span::default()),
                                span: Span::default(),
                            },
                            // Named arg: output_type
                            CallArg {
                                name: Some("output_type".to_string()),
                                value: Expr::String("float3".to_string(), Span::default()),
                                span: Span::default(),
                            },
                            // Named arg: inputs array
                            CallArg {
                                name: Some("inputs".to_string()),
                                value: Expr::Array(
                                    vec![
                                        Expr::Tuple(
                                            vec![
                                                Expr::Ident("Input0".to_string(), Span::default()),
                                                Expr::String("float3".to_string(), Span::default()),
                                            ],
                                            Span::default(),
                                        ),
                                        Expr::Tuple(
                                            vec![
                                                Expr::Ident("Input1".to_string(), Span::default()),
                                                Expr::String("float3".to_string(), Span::default()),
                                            ],
                                            Span::default(),
                                        ),
                                    ],
                                    Span::default(),
                                ),
                                span: Span::default(),
                            },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("custom_effect".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1); // One custom HLSL node
        
        // Verify the custom HLSL node
        let node = &graph.nodes[0];
        match &node.node_type {
            MaterialNodeType::CustomHLSL { code, output_type, inputs } => {
                assert_eq!(code, "float3 result = Input0 * Input1;\nreturn result;");
                assert!(matches!(output_type, CustomOutputType::Float3));
                assert_eq!(inputs.len(), 2);
                assert_eq!(inputs[0].name, "Input0");
                assert!(matches!(inputs[0].input_type, CustomOutputType::Float3));
                assert_eq!(inputs[1].name, "Input1");
                assert!(matches!(inputs[1].input_type, CustomOutputType::Float3));
            }
            _ => panic!("Expected CustomHLSL node, got {:?}", node.node_type),
        }
    }

    #[test]
    fn test_custom_hlsl_minimal() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test custom_hlsl() with just HLSL code (defaults)
        let def = MaterialGraphDef {
            name: "MinimalCustomHLSL".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "simple".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("custom_hlsl".to_string(), Span::default())),
                        args: vec![
                            CallArg {
                                name: None,
                                value: Expr::String("return float3(1, 0, 0);".to_string(), Span::default()),
                                span: Span::default(),
                            },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("simple".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        
        // Verify defaults
        let node = &graph.nodes[0];
        match &node.node_type {
            MaterialNodeType::CustomHLSL { code, output_type, inputs } => {
                assert_eq!(code, "return float3(1, 0, 0);");
                assert!(matches!(output_type, CustomOutputType::Float3)); // default
                assert_eq!(inputs.len(), 0); // no inputs
            }
            _ => panic!("Expected CustomHLSL node"),
        }
    }

    #[test]
    fn test_call_shader_parsing() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test call_shader() with shader name and parameters
        let def = MaterialGraphDef {
            name: "ShaderCallMaterial".to_string(),
            attributes: vec![],
            inputs: vec![
                MaterialInput {
                    name: "param1".to_string(),
                    ty: Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    default: Some(Expr::Float(1.0, Span::default())),
                    span: Span::default(),
                },
                MaterialInput {
                    name: "param2".to_string(),
                    ty: Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    default: Some(Expr::Float(2.0, Span::default())),
                    span: Span::default(),
                },
            ],
            body: vec![
                MaterialStatement::Let {
                    name: "shader_result".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("call_shader".to_string(), Span::default())),
                        args: vec![
                            // First arg: shader name
                            CallArg {
                                name: None,
                                value: Expr::String("MyCustomShader".to_string(), Span::default()),
                                span: Span::default(),
                            },
                            // Second arg: parameter 1
                            CallArg {
                                name: None,
                                value: Expr::Ident("param1".to_string(), Span::default()),
                                span: Span::default(),
                            },
                            // Third arg: parameter 2
                            CallArg {
                                name: None,
                                value: Expr::Ident("param2".to_string(), Span::default()),
                                span: Span::default(),
                            },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("shader_result".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        // Should have: 2 parameter nodes + 1 MaterialFunctionCall node
        assert_eq!(graph.nodes.len(), 3);
        
        // Find the MaterialFunctionCall node
        let function_call_node = graph.nodes.iter().find(|n| {
            matches!(n.node_type, MaterialNodeType::MaterialFunctionCall { .. })
        });
        
        assert!(function_call_node.is_some(), "MaterialFunctionCall node not found");
        
        let node = function_call_node.unwrap();
        match &node.node_type {
            MaterialNodeType::MaterialFunctionCall { function_path, inputs } => {
                assert_eq!(function_path, "/Game/Materials/Functions/MyCustomShader");
                assert_eq!(inputs.len(), 2); // Two parameters passed
            }
            _ => panic!("Expected MaterialFunctionCall node"),
        }
    }

    #[test]
    fn test_resolve_shader_path() {
        let converter = MaterialGraphConverter::new();
        
        // Test valid shader name
        let result = converter.resolve_shader_path("MyShader");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "/Game/Materials/Functions/MyShader");
        
        // Test shader name with underscores
        let result = converter.resolve_shader_path("My_Custom_Shader");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "/Game/Materials/Functions/My_Custom_Shader");
        
        // Test empty shader name (should fail)
        let result = converter.resolve_shader_path("");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("cannot be empty"));
        
        // Test invalid shader name with special characters (should fail)
        let result = converter.resolve_shader_path("My-Shader!");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Invalid shader name"));
    }

    #[test]
    fn test_uv_scroll() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test uv_scroll(uv, offset_x, offset_y)
        let def = MaterialGraphDef {
            name: "UVScrollMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "uv".to_string(),
                    value: Expr::Float(0.0, Span::default()),
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "scrolled_uv".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("uv_scroll".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("uv".to_string(), Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.1, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.2, Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("scrolled_uv".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        
        // Find the UVScroll node
        let uv_scroll_node = graph.nodes.iter().find(|n| matches!(n.node_type, MaterialNodeType::UVScroll { .. }));
        assert!(uv_scroll_node.is_some(), "UVScroll node not found");
    }

    #[test]
    fn test_uv_scale() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test uv_scale(uv, scale_x, scale_y)
        let def = MaterialGraphDef {
            name: "UVScaleMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "uv".to_string(),
                    value: Expr::Float(0.0, Span::default()),
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "scaled_uv".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("uv_scale".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("uv".to_string(), Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(2.0, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(2.0, Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("scaled_uv".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        
        // Find the UVScale node
        let uv_scale_node = graph.nodes.iter().find(|n| matches!(n.node_type, MaterialNodeType::UVScale { .. }));
        assert!(uv_scale_node.is_some(), "UVScale node not found");
    }

    #[test]
    fn test_uv_rotate() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test uv_rotate(uv, angle)
        let def = MaterialGraphDef {
            name: "UVRotateMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "uv".to_string(),
                    value: Expr::Float(0.0, Span::default()),
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "rotated_uv".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("uv_rotate".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("uv".to_string(), Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(45.0, Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("rotated_uv".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        
        // Find the UVRotate node
        let uv_rotate_node = graph.nodes.iter().find(|n| matches!(n.node_type, MaterialNodeType::UVRotate { .. }));
        assert!(uv_rotate_node.is_some(), "UVRotate node not found");
    }

    #[test]
    fn test_uv_operation_chaining() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test chaining: uv_scale then uv_scroll
        let def = MaterialGraphDef {
            name: "UVChainMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "uv".to_string(),
                    value: Expr::Float(0.0, Span::default()),
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "scaled_uv".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("uv_scale".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("uv".to_string(), Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(2.0, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(2.0, Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "final_uv".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("uv_scroll".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("scaled_uv".to_string(), Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.1, Span::default()), span: Span::default() },
                            CallArg { name: None, value: Expr::Float(0.2, Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("final_uv".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        
        // Should have both UVScale and UVScroll nodes
        let has_scale = graph.nodes.iter().any(|n| matches!(n.node_type, MaterialNodeType::UVScale { .. }));
        let has_scroll = graph.nodes.iter().any(|n| matches!(n.node_type, MaterialNodeType::UVScroll { .. }));
        
        assert!(has_scale, "UVScale node not found in chain");
        assert!(has_scroll, "UVScroll node not found in chain");
        
        // Verify the scroll node uses the scale node as input
        let scroll_node = graph.nodes.iter().find(|n| matches!(n.node_type, MaterialNodeType::UVScroll { .. })).unwrap();
        let scale_node = graph.nodes.iter().find(|n| matches!(n.node_type, MaterialNodeType::UVScale { .. })).unwrap();
        
        match &scroll_node.node_type {
            MaterialNodeType::UVScroll { uv_input, .. } => {
                assert_eq!(uv_input, &scale_node.id, "UVScroll should use UVScale output as input");
            }
            _ => panic!("Expected UVScroll node"),
        }
    }

    #[test]
    fn test_time_based_effects() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test time() function with sine wave animation
        // let pulse = sin(time() * 2.0) * 0.5 + 0.5
        let def = MaterialGraphDef {
            name: "PulsingMaterial".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "t".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("time".to_string(), Span::default())),
                        args: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "speed".to_string(),
                    value: Expr::Float(2.0, Span::default()),
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "t_scaled".to_string(),
                    value: Expr::Binary {
                        left: Box::new(Expr::Ident("t".to_string(), Span::default())),
                        op: BinaryOp::Mul,
                        right: Box::new(Expr::Ident("speed".to_string(), Span::default())),
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "wave".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("sin".to_string(), Span::default())),
                        args: vec![
                            CallArg { name: None, value: Expr::Ident("t_scaled".to_string(), Span::default()), span: Span::default() },
                        ],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "emissive".to_string(),
                    value: Expr::Ident("wave".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        assert_eq!(graph.name, "PulsingMaterial");
        
        // Verify material is marked as dynamic (because it uses time())
        assert!(graph.is_dynamic, "Material should be marked as dynamic when using time()");
        
        // Verify nodes: Time, speed constant, Multiply, Sine
        assert!(graph.nodes.len() >= 4, "Expected at least 4 nodes (Time, Constant, Multiply, Sine), got {}", graph.nodes.len());
        
        // Verify Time node exists
        let has_time_node = graph.nodes.iter().any(|n| matches!(n.node_type, MaterialNodeType::Time));
        assert!(has_time_node, "Expected Time node in graph");
        
        // Verify Sine node exists
        let has_sine_node = graph.nodes.iter().any(|n| matches!(n.node_type, MaterialNodeType::Sine { .. }));
        assert!(has_sine_node, "Expected Sine node in graph");
        
        // Verify emissive output is connected
        assert!(graph.outputs.emissive.is_some(), "Expected emissive output to be connected");
    }

    #[test]
    fn test_time_node_deduplication() {
        let mut converter = MaterialGraphConverter::new();
        
        // Test that multiple time() calls reuse the same Time node
        let def = MaterialGraphDef {
            name: "MultiTimeTest".to_string(),
            attributes: vec![],
            inputs: vec![],
            body: vec![
                MaterialStatement::Let {
                    name: "t1".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("time".to_string(), Span::default())),
                        args: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                MaterialStatement::Let {
                    name: "t2".to_string(),
                    value: Expr::Call {
                        callee: Box::new(Expr::Ident("time".to_string(), Span::default())),
                        args: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
            ],
            outputs: vec![
                MaterialOutput {
                    name: "base_color".to_string(),
                    value: Expr::Ident("t1".to_string(), Span::default()),
                    span: Span::default(),
                },
            ],
            span: Span::default(),
        };
        
        let result = converter.convert(&def);
        assert!(result.is_ok(), "Conversion failed: {:?}", result.err());
        
        let graph = result.unwrap();
        
        // Count Time nodes - should be exactly 1 (deduplication)
        let time_node_count = graph.nodes.iter().filter(|n| matches!(n.node_type, MaterialNodeType::Time)).count();
        assert_eq!(time_node_count, 1, "Expected exactly 1 Time node (deduplication), got {}", time_node_count);
        
        // Verify material is marked as dynamic
        assert!(graph.is_dynamic, "Material should be marked as dynamic");
    }

}
