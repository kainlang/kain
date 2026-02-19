use crate::material_graph::*;

pub struct MaterialNodeBuilder {
    next_id: usize,
    nodes: Vec<MaterialNode>,
}

impl MaterialNodeBuilder {
    pub fn new() -> Self {
        Self {
            next_id: 0,
            nodes: Vec::new(),
        }
    }

    fn next_id(&mut self) -> String {
        let id = format!("node_{}", self.next_id);
        self.next_id += 1;
        id
    }

    pub fn texture_sample_param2d(&mut self, param_name: &str, default_texture: Option<&str>, uv: Option<&str>, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::TextureSampleParameter2D {
                param_name: param_name.to_string(),
                default_texture: default_texture.map(|s| s.to_string()),
                uv_input: uv.map(|s| s.to_string()),
            },
            position: (x, y),
        });
        id
    }

    pub fn texture_sample(&mut self, texture: Option<&str>, uv: Option<&str>, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::TextureSample {
                texture_input: texture.map(|s| s.to_string()),
                uv_input: uv.map(|s| s.to_string()),
            },
            position: (x, y),
        });
        id
    }

    pub fn scalar_param(&mut self, name: &str, default: f32, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::ScalarParameter {
                name: name.to_string(),
                default,
            },
            position: (x, y),
        });
        id
    }

    pub fn vector_param(&mut self, name: &str, default: [f32; 3], x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::VectorParameter {
                name: name.to_string(),
                default,
            },
            position: (x, y),
        });
        id
    }

    pub fn multiply(&mut self, a: &str, b: &str, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::Multiply {
                a: a.to_string(),
                b: b.to_string(),
            },
            position: (x, y),
        });
        id
    }

    pub fn add(&mut self, a: &str, b: &str, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::Add {
                a: a.to_string(),
                b: b.to_string(),
            },
            position: (x, y),
        });
        id
    }

    pub fn lerp(&mut self, a: &str, b: &str, alpha: &str, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::Lerp {
                a: a.to_string(),
                b: b.to_string(),
                alpha: alpha.to_string(),
            },
            position: (x, y),
        });
        id
    }

    pub fn constant_float(&mut self, value: f32, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode {
            id: id.clone(),
            node_type: MaterialNodeType::ConstantFloat { value },
            position: (x, y),
        });
        id
    }

    pub fn build(self) -> Vec<MaterialNode> {
        self.nodes
    }
}
