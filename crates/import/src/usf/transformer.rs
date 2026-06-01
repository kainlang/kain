//! USF → KAIN AST Transformer
//!
//! Transforms tree-sitter HLSL AST into KAIN AST.
//!
//! ## Pipeline
//!
//! ```text
//! tree_sitter::Tree (HLSL AST)
//!     ↓
//! UsfTransformer::transform()
//!     ↓
//! kain_core::ast::Program
//! ```

use kain_core::ast::*;
use kain_core::span::Span;
use tree_sitter::{Node, Tree};

pub struct UsfTransformer<'a> {
    /// Source code being transformed
    source: &'a str,

    /// Tree-sitter parse tree
    tree: Tree,
}

impl<'a> UsfTransformer<'a> {
    pub fn new(source: &'a str, tree: Tree) -> Self {
        Self { source, tree }
    }

    /// Transform tree-sitter Tree → KAIN Program
    pub fn transform(self) -> crate::Result<Program> {
        let mut items = Vec::new();
        let span = Span::default();

        let root = self.tree.root_node();
        let mut cursor = root.walk();

        // Walk the tree and transform top-level declarations
        for child in root.children(&mut cursor) {
            if let Some(item) = self.transform_node(&child)? {
                items.push(item);
            }
        }

        Ok(Program { items, span })
    }

    /// Transform a tree-sitter node → KAIN Item
    fn transform_node(&self, node: &Node) -> crate::Result<Option<Item>> {
        match node.kind() {
            "function_definition" => Ok(Some(self.transform_function(node)?)),
            "struct_specifier" => Ok(Some(self.transform_struct(node)?)),
            "declaration" => Ok(self.transform_declaration(node)?),
            "comment" | "preproc_include" | "preproc_def" => Ok(None), // Skip
            _ => {
                // Unknown node type - skip for now
                Ok(None)
            }
        }
    }

    /// Transform function definition
    fn transform_function(&self, node: &Node) -> crate::Result<Item> {
        let span = Span::default();

        // Extract function name
        let name = self
            .extract_function_name(node)
            .unwrap_or_else(|| "unnamed".to_string());

        // Extract parameters
        let params = self.extract_function_params(node)?;

        // Extract return type
        let return_type = self.extract_return_type(node)?;

        // Extract body
        let body = self.extract_function_body(node)?;

        Ok(Item::Function(Function {
            name,
            generics: vec![],
            where_clause: None,
            params,
            return_type: Some(return_type),
            effects: vec![],
            body,
            visibility: Visibility::Public,
            attributes: vec![],
            span,
        }))
    }

    /// Transform struct definition
    fn transform_struct(&self, node: &Node) -> crate::Result<Item> {
        let span = Span::default();

        // Extract struct name
        let name = self
            .extract_struct_name(node)
            .unwrap_or_else(|| "UnnamedStruct".to_string());

        // Extract fields
        let fields = self.extract_struct_fields(node)?;

        Ok(Item::Struct(Struct {
            name,
            generics: vec![],
            where_clause: None,
            fields,
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span,
        }))
    }

    /// Transform declaration (global variables, uniforms, etc.)
    fn transform_declaration(&self, _node: &Node) -> crate::Result<Option<Item>> {
        // TODO: Handle global variable declarations
        // For now, skip them
        Ok(None)
    }

    // ── Helper Methods ────────────────────────────────────────────────────────

    /// Extract function name from function_definition node
    fn extract_function_name(&self, node: &Node) -> Option<String> {
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "identifier" {
                return Some(self.node_text(&child));
            }
        }
        None
    }

    /// Extract function parameters
    fn extract_function_params(&self, node: &Node) -> crate::Result<Vec<Param>> {
        let mut params = Vec::new();
        let span = Span::default();

        // Find parameter_list node
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "parameter_list" {
                let mut param_cursor = child.walk();
                for param_node in child.children(&mut param_cursor) {
                    if param_node.kind() == "parameter_declaration" {
                        let name = self
                            .extract_param_name(&param_node)
                            .unwrap_or_else(|| "param".to_string());
                        let ty = self.extract_param_type(&param_node)?;

                        params.push(Param {
                            name,
                            ty,
                            mutable: false,
                            default: None,
                            span,
                        });
                    }
                }
            }
        }

        Ok(params)
    }

    /// Extract parameter name
    fn extract_param_name(&self, node: &Node) -> Option<String> {
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "identifier" {
                return Some(self.node_text(&child));
            }
        }
        None
    }

    /// Extract parameter type
    fn extract_param_type(&self, node: &Node) -> crate::Result<Type> {
        let span = Span::default();
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "type_identifier" || child.kind() == "primitive_type" {
                return Ok(self.map_hlsl_type(&self.node_text(&child), span));
            }
        }
        Ok(Type::Infer(span))
    }

    /// Extract return type
    fn extract_return_type(&self, node: &Node) -> crate::Result<Type> {
        let span = Span::default();
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "type_identifier" || child.kind() == "primitive_type" {
                return Ok(self.map_hlsl_type(&self.node_text(&child), span));
            }
        }
        Ok(Type::Unit(span))
    }

    /// Extract function body
    fn extract_function_body(&self, node: &Node) -> crate::Result<Block> {
        let span = Span::default();

        // Find compound_statement (function body)
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "compound_statement" {
                return self.transform_compound_statement(&child);
            }
        }

        // No body found - return empty block
        Ok(Block {
            stmts: vec![],
            span,
        })
    }

    /// Transform compound statement (block)
    fn transform_compound_statement(&self, node: &Node) -> crate::Result<Block> {
        let span = Span::default();
        let mut stmts = Vec::new();

        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if let Some(stmt) = self.transform_statement(&child)? {
                stmts.push(stmt);
            }
        }

        Ok(Block { stmts, span })
    }

    /// Transform statement
    fn transform_statement(&self, node: &Node) -> crate::Result<Option<Stmt>> {
        match node.kind() {
            "return_statement" => Ok(Some(self.transform_return_statement(node)?)),
            "expression_statement" => Ok(Some(self.transform_expression_statement(node)?)),
            "declaration" => Ok(self.transform_declaration_statement(node)?),
            "{" | "}" => Ok(None), // Skip braces
            _ => Ok(None),         // Skip unknown statements for now
        }
    }

    /// Transform return statement
    fn transform_return_statement(&self, node: &Node) -> crate::Result<Stmt> {
        let span = Span::default();

        // Extract return expression
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() != "return" && child.kind() != ";" {
                let expr = self.transform_expression(&child)?;
                return Ok(Stmt::Return(Some(expr), span));
            }
        }

        Ok(Stmt::Return(None, span))
    }

    /// Transform expression statement
    fn transform_expression_statement(&self, node: &Node) -> crate::Result<Stmt> {
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() != ";" {
                let expr = self.transform_expression(&child)?;
                return Ok(Stmt::Expr(expr));
            }
        }

        let span = Span::default();
        Ok(Stmt::Expr(Expr::None(span)))
    }

    /// Transform declaration statement
    fn transform_declaration_statement(&self, _node: &Node) -> crate::Result<Option<Stmt>> {
        // TODO: Handle local variable declarations
        Ok(None)
    }

    /// Transform expression
    fn transform_expression(&self, node: &Node) -> crate::Result<Expr> {
        let span = Span::default();

        match node.kind() {
            "identifier" => {
                let name = self.node_text(node);
                Ok(Expr::Ident(name, span))
            }
            "number_literal" => {
                let text = self.node_text(node);
                if text.contains('.') {
                    Ok(Expr::Float(text.parse().unwrap_or(0.0), span))
                } else {
                    Ok(Expr::Int(text.parse().unwrap_or(0), span))
                }
            }
            _ => Ok(Expr::None(span)), // Placeholder for complex expressions
        }
    }

    /// Extract struct name
    fn extract_struct_name(&self, node: &Node) -> Option<String> {
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "type_identifier" {
                return Some(self.node_text(&child));
            }
        }
        None
    }

    /// Extract struct fields
    fn extract_struct_fields(&self, node: &Node) -> crate::Result<Vec<Field>> {
        let mut fields = Vec::new();
        let span = Span::default();

        // Find field_declaration_list
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "field_declaration_list" {
                let mut field_cursor = child.walk();
                for field_node in child.children(&mut field_cursor) {
                    if field_node.kind() == "field_declaration" {
                        let name = self
                            .extract_field_name(&field_node)
                            .unwrap_or_else(|| "field".to_string());
                        let ty = self.extract_field_type(&field_node)?;

                        fields.push(Field {
                            name,
                            ty,
                            attributes: vec![],
                            visibility: Visibility::Public,
                            default: None,
                            weak: false,
                            span,
                        });
                    }
                }
            }
        }

        Ok(fields)
    }

    /// Extract field name
    fn extract_field_name(&self, node: &Node) -> Option<String> {
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "field_identifier" {
                return Some(self.node_text(&child));
            }
        }
        None
    }

    /// Extract field type
    fn extract_field_type(&self, node: &Node) -> crate::Result<Type> {
        let span = Span::default();
        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            if child.kind() == "type_identifier" || child.kind() == "primitive_type" {
                return Ok(self.map_hlsl_type(&self.node_text(&child), span));
            }
        }
        Ok(Type::Infer(span))
    }

    /// Map HLSL type → KAIN type
    fn map_hlsl_type(&self, hlsl_type: &str, span: Span) -> Type {
        match hlsl_type {
            "float" => Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span,
            },
            "float2" => Type::Named {
                name: "Vec2".to_string(),
                generics: vec![],
                span,
            },
            "float3" => Type::Named {
                name: "Vec3".to_string(),
                generics: vec![],
                span,
            },
            "float4" => Type::Named {
                name: "Vec4".to_string(),
                generics: vec![],
                span,
            },
            "int" => Type::Named {
                name: "Int".to_string(),
                generics: vec![],
                span,
            },
            "uint" => Type::Named {
                name: "UInt".to_string(),
                generics: vec![],
                span,
            },
            "bool" => Type::Named {
                name: "Bool".to_string(),
                generics: vec![],
                span,
            },
            "float4x4" => Type::Named {
                name: "Mat4".to_string(),
                generics: vec![],
                span,
            },
            "float3x3" => Type::Named {
                name: "Mat3".to_string(),
                generics: vec![],
                span,
            },
            "float2x2" => Type::Named {
                name: "Mat2".to_string(),
                generics: vec![],
                span,
            },
            "Texture2D" => Type::Named {
                name: "Sampler2D".to_string(),
                generics: vec![],
                span,
            },
            "Texture3D" => Type::Named {
                name: "Sampler3D".to_string(),
                generics: vec![],
                span,
            },
            "RWTexture2D" => Type::Named {
                name: "RWTexture2D".to_string(),
                generics: vec![],
                span,
            },
            "void" => Type::Unit(span),
            _ => Type::Named {
                name: hlsl_type.to_string(),
                generics: vec![],
                span,
            },
        }
    }

    /// Get text content of a node
    fn node_text(&self, node: &Node) -> String {
        node.utf8_text(self.source.as_bytes())
            .unwrap_or("")
            .to_string()
    }
}
