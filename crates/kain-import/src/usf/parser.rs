// ============================================================================
// USF Parser - HLSL/USF → KAIN AST
// ============================================================================
// Parses Unreal Engine 5 USF shaders into KAIN AST for research/training.
//
// Supported:
// - Compute shaders ([numthreads] functions)
// - Vertex/Fragment shaders
// - cbuffer declarations (uniform buffers)
// - Texture/Sampler declarations
// - Function bodies (statements, expressions)
// - HLSL intrinsics (saturate, pow, length, etc.)
//
// Architecture:
// - Lexer: HLSL tokenization (C-style, not Python-style indentation)
// - Parser: Recursive descent → KAIN AST
// - Semantic Mapper: HLSL types/intrinsics → KAIN stdlib equivalents
// ============================================================================

use kain_core::ast::*;
use kain_core::span::Span;
use kain_core::effects::Effect;
use crate::usf::types::*;
use crate::usf::UsfImportError;
use std::collections::HashMap;

/// USF Parser - converts HLSL/USF tokens into KAIN AST
pub struct UsfParser {
    tokens: Vec<UsfToken>,
    pos: usize,
    filename: String,
    
    /// Track current shader type being parsed
    current_shader_type: Option<ShaderType>,
    
    /// HLSL intrinsic → KAIN stdlib function mapping
    intrinsic_map: HashMap<String, String>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ShaderType {
    Compute,
    Vertex,
    Fragment,
    Surface,
}

impl UsfParser {
    pub fn new(tokens: Vec<UsfToken>, filename: String) -> Self {
        Self {
            tokens,
            pos: 0,
            filename,
            current_shader_type: None,
            intrinsic_map: Self::build_intrinsic_map(),
        }
    }
    
    /// Build HLSL intrinsic → KAIN stdlib mapping
    fn build_intrinsic_map() -> HashMap<String, String> {
        let mut map = HashMap::new();
        
        // Math functions (1:1 mapping to KAIN stdlib)
        map.insert("saturate".to_string(), "saturate".to_string());
        map.insert("pow".to_string(), "pow".to_string());
        map.insert("sqrt".to_string(), "sqrt".to_string());
        map.insert("abs".to_string(), "abs".to_string());
        map.insert("min".to_string(), "min".to_string());
        map.insert("max".to_string(), "max".to_string());
        map.insert("clamp".to_string(), "clamp".to_string());
        map.insert("lerp".to_string(), "lerp".to_string());
        map.insert("dot".to_string(), "dot".to_string());
        map.insert("cross".to_string(), "cross".to_string());
        map.insert("normalize".to_string(), "normalize".to_string());
        map.insert("length".to_string(), "length".to_string());
        map.insert("distance".to_string(), "distance".to_string());
        map.insert("floor".to_string(), "floor".to_string());
        map.insert("ceil".to_string(), "ceil".to_string());
        map.insert("frac".to_string(), "frac".to_string());
        map.insert("sin".to_string(), "sin".to_string());
        map.insert("cos".to_string(), "cos".to_string());
        map.insert("tan".to_string(), "tan".to_string());
        map.insert("asin".to_string(), "asin".to_string());
        map.insert("acos".to_string(), "acos".to_string());
        map.insert("atan".to_string(), "atan".to_string());
        map.insert("atan2".to_string(), "atan2".to_string());
        map.insert("exp".to_string(), "exp".to_string());
        map.insert("log".to_string(), "log".to_string());
        map.insert("log2".to_string(), "log2".to_string());
        map.insert("exp2".to_string(), "exp2".to_string());
        map.insert("reflect".to_string(), "reflect".to_string());
        map.insert("refract".to_string(), "refract".to_string());
        map.insert("step".to_string(), "step".to_string());
        map.insert("smoothstep".to_string(), "smoothstep".to_string());
        
        map
    }
    
    // ========================================================================
    // Main Entry Point
    // ========================================================================
    
    /// Parse USF file into KAIN Program
    pub fn parse_shader(&mut self) -> Result<Program, UsfImportError> {
        let mut items = Vec::new();
        
        while !self.is_at_end() {
            // Skip preprocessor directives (handled by preprocessor module)
            if self.check_keyword("include") || self.check_keyword("define") {
                self.skip_preprocessor_line();
                continue;
            }
            
            // Parse top-level declarations
            if let Some(item) = self.parse_top_level_item()? {
                items.push(item);
            }
        }
        
        Ok(Program {
            items,
            span: Span::new(0, self.tokens.last().map(|t| t.span.end).unwrap_or(0)),
        })
    }
    
    /// Parse top-level item (function, cbuffer, texture, etc.)
    fn parse_top_level_item(&mut self) -> Result<Option<Item>, UsfImportError> {
        // Skip semicolons at top level
        if self.check_punct(";") {
            self.advance();
            return Ok(None);
        }
        
        // cbuffer → struct with @uniform
        if self.check_keyword("cbuffer") {
            return Ok(Some(self.parse_cbuffer()?));
        }
        
        // Texture/Sampler declarations → skip (handled as extern in KAIN)
        if self.is_texture_type() || self.is_sampler_type() {
            self.skip_texture_declaration();
            return Ok(None);
        }
        
        // Function declaration
        if self.is_function_start() {
            return Ok(Some(self.parse_function()?));
        }
        
        // Unknown - skip and continue
        self.advance();
        Ok(None)
    }
    
    // ========================================================================
    // cbuffer Parsing (Uniform Buffers)
    // ========================================================================
    
    /// Parse cbuffer → KAIN struct with @uniform
    fn parse_cbuffer(&mut self) -> Result<Item, UsfImportError> {
        let start = self.current_span();
        self.expect_keyword("cbuffer")?;
        
        let name = self.expect_identifier()?;
        
        // Optional register binding: cbuffer MyBuffer : register(b0)
        if self.check_punct(":") {
            self.advance();
            self.expect_keyword("register")?;
            self.expect_punct("(")?;
            self.expect_identifier()?; // register slot (b0, b1, etc.)
            self.expect_punct(")")?;
        }
        
        self.expect_punct("{")?;
        
        let mut fields = Vec::new();
        while !self.check_punct("}") && !self.is_at_end() {
            let field = self.parse_struct_field()?;
            fields.push(field);
            
            // Optional semicolon after field
            if self.check_punct(";") {
                self.advance();
            }
        }
        
        self.expect_punct("}")?;
        
        // Optional semicolon after cbuffer
        if self.check_punct(";") {
            self.advance();
        }
        
        let span = Span::new(start.start, self.previous_span().end);
        
        Ok(Item::Struct(Struct {
            name,
            generics: vec![],
            fields,
            visibility: Visibility::Public,
            attributes: vec![
                Attribute {
                    name: "uniform".to_string(),
                    args: vec![],
                    span: span.clone(),
                }
            ],
            span,
        }))
    }
    
    /// Parse struct field (used in cbuffer and struct declarations)
    fn parse_struct_field(&mut self) -> Result<StructField, UsfImportError> {
        let start = self.current_span();
        
        let ty = self.parse_type()?;
        let name = self.expect_identifier()?;
        
        // Optional array size: float myArray[16]
        let ty = if self.check_punct("[") {
            self.advance();
            let size_expr = self.parse_expression()?;
            self.expect_punct("]")?;
            
            Type::Array {
                element: Box::new(ty),
                size: Some(Box::new(size_expr)),
                span: Span::new(start.start, self.previous_span().end),
            }
        } else {
            ty
        };
        
        // Optional semantic: float4 position : SV_Position
        if self.check_punct(":") {
            self.advance();
            self.expect_identifier()?; // Skip semantic
        }
        
        let span = Span::new(start.start, self.previous_span().end);
        
        Ok(StructField {
            name,
            ty,
            visibility: Visibility::Public,
            attributes: vec![],
            default: None,
            span,
        })
    }
    
    // ========================================================================
    // Function Parsing
    // ========================================================================
    
    /// Parse function declaration
    fn parse_function(&mut self) -> Result<Item, UsfImportError> {
        let start = self.current_span();
        
        // Check for [numthreads] attribute (compute shader)
        let mut attributes = vec![];
        if self.check_punct("[") {
            attributes.extend(self.parse_attributes()?);
        }
        
        // Return type
        let return_type = self.parse_type()?;
        
        // Function name
        let name = self.expect_identifier()?;
        
        // Parameters
        self.expect_punct("(")?;
        let params = self.parse_parameter_list()?;
        self.expect_punct(")")?;
        
        // Optional semantic: float4 main() : SV_Target
        if self.check_punct(":") {
            self.advance();
            self.expect_identifier()?; // Skip semantic
        }
        
        // Function body
        let body = self.parse_block()?;
        
        let span = Span::new(start.start, self.previous_span().end);
        
        // Determine shader type from attributes
        let shader_type = self.infer_shader_type(&attributes, &name);
        self.current_shader_type = shader_type;
        
        // Convert to KAIN shader if it's a shader entry point
        if let Some(shader_type) = shader_type {
            Ok(Item::Shader(Shader {
                name,
                shader_type: match shader_type {
                    ShaderType::Compute => "compute".to_string(),
                    ShaderType::Vertex => "vertex".to_string(),
                    ShaderType::Fragment => "fragment".to_string(),
                    ShaderType::Surface => "surface".to_string(),
                },
                params,
                body,
                attributes,
                span,
            }))
        } else {
            // Regular function
            Ok(Item::Function(Function {
                name,
                generics: vec![],
                params,
                return_type: Some(return_type),
                effects: vec![Effect::Pure], // HLSL functions are pure by default
                body,
                visibility: Visibility::Public,
                attributes,
                span,
            }))
        }
    }
    
    /// Infer shader type from attributes and function name
    fn infer_shader_type(&self, attributes: &[Attribute], name: &str) -> Option<ShaderType> {
        // Check for [numthreads] attribute
        for attr in attributes {
            if attr.name == "numthreads" {
                return Some(ShaderType::Compute);
            }
        }
        
        // Check function name patterns
        if name.contains("VS") || name.contains("Vertex") {
            return Some(ShaderType::Vertex);
        }
        if name.contains("PS") || name.contains("Pixel") || name.contains("Fragment") {
            return Some(ShaderType::Fragment);
        }
        
        None
    }
    
    /// Parse [numthreads(x, y, z)] or other attributes
    fn parse_attributes(&mut self) -> Result<Vec<Attribute>, UsfImportError> {
        let mut attributes = vec![];
        
        while self.check_punct("[") {
            let start = self.current_span();
            self.advance(); // [
            
            let name = self.expect_identifier()?;
            
            let mut args = vec![];
            if self.check_punct("(") {
                self.advance();
                
                while !self.check_punct(")") && !self.is_at_end() {
                    args.push(self.parse_expression()?);
                    
                    if self.check_punct(",") {
                        self.advance();
                    }
                }
                
                self.expect_punct(")")?;
            }
            
            self.expect_punct("]")?;
            
            let span = Span::new(start.start, self.previous_span().end);
            attributes.push(Attribute { name, args, span });
        }
        
        Ok(attributes)
    }
    
    /// Parse parameter list
    fn parse_parameter_list(&mut self) -> Result<Vec<Param>, UsfImportError> {
        let mut params = vec![];
        
        while !self.check_punct(")") && !self.is_at_end() {
            let start = self.current_span();
            
            // Optional 'in', 'out', 'inout' modifiers
            let mutable = if self.check_keyword("out") || self.check_keyword("inout") {
                self.advance();
                true
            } else {
                if self.check_keyword("in") {
                    self.advance();
                }
                false
            };
            
            let ty = self.parse_type()?;
            let name = self.expect_identifier()?;
            
            // Optional semantic: float4 position : SV_Position
            if self.check_punct(":") {
                self.advance();
                self.expect_identifier()?; // Skip semantic
            }
            
            let span = Span::new(start.start, self.previous_span().end);
            
            params.push(Param {
                name,
                ty,
                mutable,
                default: None,
                span,
            });
            
            if self.check_punct(",") {
                self.advance();
            }
        }
        
        Ok(params)
    }
    
    // ========================================================================
    // Type Parsing
    // ========================================================================
    
    /// Parse HLSL type → KAIN type
    fn parse_type(&mut self) -> Result<Type, UsfImportError> {
        let start = self.current_span();
        
        let type_name = self.expect_identifier()?;
        
        // Map HLSL types to KAIN types
        let kain_type = match type_name.as_str() {
            // Scalar types
            "void" => "Void",
            "bool" => "Bool",
            "int" => "Int",
            "uint" => "UInt",
            "float" => "Float",
            "double" => "Double",
            "half" => "Float", // half → Float in KAIN
            
            // Vector types
            "float2" => "Vec2",
            "float3" => "Vec3",
            "float4" => "Vec4",
            "int2" => "IVec2",
            "int3" => "IVec3",
            "int4" => "IVec4",
            "uint2" => "UVec2",
            "uint3" => "UVec3",
            "uint4" => "UVec4",
            
            // Matrix types
            "float2x2" => "Mat2",
            "float3x3" => "Mat3",
            "float4x4" => "Mat4",
            "matrix" => "Mat4",
            
            // Texture types (opaque handles)
            "Texture2D" | "Texture3D" | "TextureCube" | "Texture2DArray" => "Texture",
            "RWTexture2D" | "RWTexture3D" => "RWTexture",
            
            // Buffer types
            "Buffer" | "StructuredBuffer" => "Buffer",
            "RWBuffer" | "RWStructuredBuffer" => "RWBuffer",
            
            // Sampler types
            "SamplerState" | "SamplerComparisonState" => "Sampler",
            
            // Keep custom types as-is
            _ => &type_name,
        };
        
        let span = Span::new(start.start, self.previous_span().end);
        
        Ok(Type::Named {
            name: kain_type.to_string(),
            generics: vec![],
            span,
        })
    }
    
    // ========================================================================
    // Statement Parsing
    // ========================================================================
    
    /// Parse block { statements }
    fn parse_block(&mut self) -> Result<Block, UsfImportError> {
        let start = self.current_span();
        self.expect_punct("{")?;
        
        let mut statements = vec![];
        
        while !self.check_punct("}") && !self.is_at_end() {
            statements.push(self.parse_statement()?);
        }
        
        self.expect_punct("}")?;
        
        let span = Span::new(start.start, self.previous_span().end);
        
        Ok(Block { statements, span })
    }
    
    /// Parse statement
    fn parse_statement(&mut self) -> Result<Stmt, UsfImportError> {
        let start = self.current_span();
        
        // Return statement
        if self.check_keyword("return") {
            self.advance();
            
            let value = if !self.check_punct(";") {
                Some(self.parse_expression()?)
            } else {
                None
            };
            
            self.expect_punct(";")?;
            
            return Ok(Stmt::Return {
                value,
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        // If statement
        if self.check_keyword("if") {
            return self.parse_if_statement();
        }
        
        // For loop
        if self.check_keyword("for") {
            return self.parse_for_loop();
        }
        
        // While loop
        if self.check_keyword("while") {
            return self.parse_while_loop();
        }
        
        // Break/Continue
        if self.check_keyword("break") {
            self.advance();
            self.expect_punct(";")?;
            return Ok(Stmt::Break {
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        if self.check_keyword("continue") {
            self.advance();
            self.expect_punct(";")?;
            return Ok(Stmt::Continue {
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        // Variable declaration: float x = 5.0;
        if self.is_type_start() {
            return self.parse_variable_declaration();
        }
        
        // Expression statement (assignment, function call, etc.)
        let expr = self.parse_expression()?;
        self.expect_punct(";")?;
        
        Ok(Stmt::Expr {
            expr,
            span: Span::new(start.start, self.previous_span().end),
        })
    }
    
    /// Parse if statement
    fn parse_if_statement(&mut self) -> Result<Stmt, UsfImportError> {
        let start = self.current_span();
        self.expect_keyword("if")?;
        
        self.expect_punct("(")?;
        let condition = self.parse_expression()?;
        self.expect_punct(")")?;
        
        let then_block = self.parse_block()?;
        
        let else_block = if self.check_keyword("else") {
            self.advance();
            
            // else if
            if self.check_keyword("if") {
                let else_if = self.parse_if_statement()?;
                Some(Block {
                    statements: vec![else_if],
                    span: self.previous_span(),
                })
            } else {
                Some(self.parse_block()?)
            }
        } else {
            None
        };
        
        Ok(Stmt::If {
            condition,
            then_block,
            else_block,
            span: Span::new(start.start, self.previous_span().end),
        })
    }
    
    /// Parse for loop
    fn parse_for_loop(&mut self) -> Result<Stmt, UsfImportError> {
        let start = self.current_span();
        self.expect_keyword("for")?;
        
        self.expect_punct("(")?;
        
        // Init statement
        let init = if !self.check_punct(";") {
            Some(Box::new(self.parse_statement()?))
        } else {
            self.advance(); // consume ;
            None
        };
        
        // Condition
        let condition = if !self.check_punct(";") {
            Some(self.parse_expression()?)
        } else {
            None
        };
        self.expect_punct(";")?;
        
        // Increment
        let increment = if !self.check_punct(")") {
            Some(self.parse_expression()?)
        } else {
            None
        };
        self.expect_punct(")")?;
        
        let body = self.parse_block()?;
        
        // Convert C-style for loop to KAIN while loop with init/increment
        let mut statements = vec![];
        
        if let Some(init) = init {
            statements.push(*init);
        }
        
        let while_body = if let Some(increment) = increment {
            let mut body_stmts = body.statements;
            body_stmts.push(Stmt::Expr {
                expr: increment,
                span: self.previous_span(),
            });
            Block {
                statements: body_stmts,
                span: body.span,
            }
        } else {
            body
        };
        
        statements.push(Stmt::While {
            condition: condition.unwrap_or_else(|| {
                Expr::Literal {
                    value: Literal::Bool(true),
                    span: self.previous_span(),
                }
            }),
            body: while_body,
            span: Span::new(start.start, self.previous_span().end),
        });
        
        Ok(Stmt::Block {
            block: Block {
                statements,
                span: Span::new(start.start, self.previous_span().end),
            },
            span: Span::new(start.start, self.previous_span().end),
        })
    }
    
    /// Parse while loop
    fn parse_while_loop(&mut self) -> Result<Stmt, UsfImportError> {
        let start = self.current_span();
        self.expect_keyword("while")?;
        
        self.expect_punct("(")?;
        let condition = self.parse_expression()?;
        self.expect_punct(")")?;
        
        let body = self.parse_block()?;
        
        Ok(Stmt::While {
            condition,
            body,
            span: Span::new(start.start, self.previous_span().end),
        })
    }
    
    /// Parse variable declaration
    fn parse_variable_declaration(&mut self) -> Result<Stmt, UsfImportError> {
        let start = self.current_span();
        
        let ty = self.parse_type()?;
        let name = self.expect_identifier()?;
        
        let value = if self.check_punct("=") {
            self.advance();
            Some(self.parse_expression()?)
        } else {
            None
        };
        
        self.expect_punct(";")?;
        
        Ok(Stmt::Let {
            name,
            ty: Some(ty),
            value,
            mutable: false, // HLSL variables are mutable by default, but we mark as immutable for safety
            span: Span::new(start.start, self.previous_span().end),
        })
    }
    
    // ========================================================================
    // Expression Parsing
    // ========================================================================
    
    /// Parse expression (precedence climbing)
    fn parse_expression(&mut self) -> Result<Expr, UsfImportError> {
        self.parse_assignment()
    }
    
    /// Parse assignment (lowest precedence)
    fn parse_assignment(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let expr = self.parse_ternary()?;
        
        // Assignment operators: =, +=, -=, *=, /=, etc.
        if self.check_punct("=") || self.check_punct("+=") || self.check_punct("-=") 
            || self.check_punct("*=") || self.check_punct("/=") {
            let op = self.current_token_text();
            self.advance();
            
            let value = self.parse_assignment()?;
            
            return Ok(Expr::Assign {
                target: Box::new(expr),
                value: Box::new(value),
                op: Some(op),
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        Ok(expr)
    }
    
    /// Parse ternary operator: condition ? true_expr : false_expr
    fn parse_ternary(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let expr = self.parse_logical_or()?;
        
        if self.check_punct("?") {
            self.advance();
            
            let true_expr = self.parse_expression()?;
            self.expect_punct(":")?;
            let false_expr = self.parse_expression()?;
            
            return Ok(Expr::If {
                condition: Box::new(expr),
                then_expr: Box::new(true_expr),
                else_expr: Some(Box::new(false_expr)),
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        Ok(expr)
    }
    
    /// Parse logical OR: ||
    fn parse_logical_or(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_logical_and()?;
        
        while self.check_punct("||") {
            self.advance();
            let right = self.parse_logical_and()?;
            left = Expr::Binary {
                left: Box::new(left),
                op: BinaryOp::Or,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse logical AND: &&
    fn parse_logical_and(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_equality()?;
        
        while self.check_punct("&&") {
            self.advance();
            let right = self.parse_equality()?;
            left = Expr::Binary {
                left: Box::new(left),
                op: BinaryOp::And,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse equality: ==, !=
    fn parse_equality(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_comparison()?;
        
        while self.check_punct("==") || self.check_punct("!=") {
            let op = if self.check_punct("==") {
                BinaryOp::Eq
            } else {
                BinaryOp::Ne
            };
            self.advance();
            
            let right = self.parse_comparison()?;
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse comparison: <, >, <=, >=
    fn parse_comparison(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_additive()?;
        
        while self.check_punct("<") || self.check_punct(">") 
            || self.check_punct("<=") || self.check_punct(">=") {
            let op = match self.current_token_text().as_str() {
                "<" => BinaryOp::Lt,
                ">" => BinaryOp::Gt,
                "<=" => BinaryOp::Le,
                ">=" => BinaryOp::Ge,
                _ => unreachable!(),
            };
            self.advance();
            
            let right = self.parse_additive()?;
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse additive: +, -
    fn parse_additive(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_multiplicative()?;
        
        while self.check_punct("+") || self.check_punct("-") {
            let op = if self.check_punct("+") {
                BinaryOp::Add
            } else {
                BinaryOp::Sub
            };
            self.advance();
            
            let right = self.parse_multiplicative()?;
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse multiplicative: *, /, %
    fn parse_multiplicative(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut left = self.parse_unary()?;
        
        while self.check_punct("*") || self.check_punct("/") || self.check_punct("%") {
            let op = match self.current_token_text().as_str() {
                "*" => BinaryOp::Mul,
                "/" => BinaryOp::Div,
                "%" => BinaryOp::Mod,
                _ => unreachable!(),
            };
            self.advance();
            
            let right = self.parse_unary()?;
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
                span: Span::new(start.start, self.previous_span().end),
            };
        }
        
        Ok(left)
    }
    
    /// Parse unary: -, !, ++, --
    fn parse_unary(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        
        if self.check_punct("-") || self.check_punct("!") {
            let op = if self.check_punct("-") {
                UnaryOp::Neg
            } else {
                UnaryOp::Not
            };
            self.advance();
            
            let expr = self.parse_unary()?;
            return Ok(Expr::Unary {
                op,
                expr: Box::new(expr),
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        // Prefix increment/decrement
        if self.check_punct("++") || self.check_punct("--") {
            let op = self.current_token_text();
            self.advance();
            
            let expr = self.parse_postfix()?;
            
            // Convert ++x to x = x + 1
            return Ok(Expr::Assign {
                target: Box::new(expr.clone()),
                value: Box::new(Expr::Binary {
                    left: Box::new(expr),
                    op: if op == "++" { BinaryOp::Add } else { BinaryOp::Sub },
                    right: Box::new(Expr::Literal {
                        value: Literal::Int(1),
                        span: self.previous_span(),
                    }),
                    span: self.previous_span(),
                }),
                op: None,
                span: Span::new(start.start, self.previous_span().end),
            });
        }
        
        self.parse_postfix()
    }
    
    /// Parse postfix: function calls, array access, member access, ++, --
    fn parse_postfix(&mut self) -> Result<Expr, UsfImportError> {
        let start = self.current_span();
        let mut expr = self.parse_primary()?;
        
        loop {
            // Function call: func(args)
            if self.check_punct("(") {
                self.advance();
                
                let mut args = vec![];
                while !self.check_punct(")") && !self.is_at_end() {
                    args.push(self.parse_expression()?);
                    
                    if self.check_punct(",") {
                        self.advance();
                    }
                }
                
                self.expect_punct(")")?;
                
                // Map HLSL intrinsics to KAIN stdlib
                expr = self.map_intrinsic_call(expr, args)?;
                
                continue;
            }
            
            // Array access: arr[index]
            if self.check_punct("[") {
                self.advance();
                let index = self.parse_expression()?;
                self.expect_punct("]")?;
                
                expr = Expr::Index {
                    target: Box::new(expr),
                    index: Box::new(index),
                    span: Span::new(start.start, self.previous_span().end),
                };
                continue;
            }
            
            // Member access: obj.field or vec.x
            if self.check_punct(".") {
                self.advance();
                let field = self.expect_identifier()?;
                
                expr = Expr::FieldAccess {
                    target: Box::new(expr),
                    field,
                    span: Span::new(start.start, self.previous_span().end),
                };
                continue;
            }
            
            // Postfix increment/decrement
            if self.check_punct("++") || self.check_punct("--") {
                let op = self.current_token_text();
                self.advance();
