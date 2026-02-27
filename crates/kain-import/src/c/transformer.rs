//! C to KAIN AST transformer
//!
//! Transforms lang-c AST into KAIN AST

use lang_c::ast as c_ast;
use lang_c::span::Node;
use kain_core::ast::*;
use kain_core::effects::Effect;
use kain_core::language_features::{default_language_capabilities, LanguageCapabilities};
use kain_core::span::Span;
use crate::c::types::CTypeTransformer;
use crate::common::c_registry::{
    resolve_c_binary_operator,
    resolve_c_compound_assignment_binary_operator,
    CBinaryOperatorResolution,
};
use crate::{ImportError, Result};
use std::collections::{HashMap, HashSet};

/// C to KAIN AST transformer
pub struct CTransformer {
    /// Type transformer
    type_transformer: CTypeTransformer,
    
    /// Current function context
    current_function: Option<String>,
    
    /// Struct definitions
    structs: HashMap<String, Struct>,
    
    /// Enum definitions
    enums: HashMap<String, Enum>,
    
    /// Typedef mappings
    typedefs: HashMap<String, Type>,

    /// KAIN language capability profile used for data-driven lowering decisions.
    language_capabilities: LanguageCapabilities,
}

impl CTransformer {
    pub fn new() -> Self {
        Self::with_language_capabilities(default_language_capabilities())
    }

    pub fn with_language_capabilities(language_capabilities: LanguageCapabilities) -> Self {
        Self {
            type_transformer: CTypeTransformer::new(),
            current_function: None,
            structs: HashMap::new(),
            enums: HashMap::new(),
            typedefs: HashMap::new(),
            language_capabilities,
        }
    }
    
    /// Transform a C translation unit to KAIN program
    pub fn transform(&mut self, tu: c_ast::TranslationUnit) -> Result<Program> {
        let mut items = Vec::new();
        
        for decl in tu.0 {
            match decl.node {
                c_ast::ExternalDeclaration::FunctionDefinition(func) => {
                    if let Some(item) = self.transform_function(func.node)? {
                        items.push(item);
                    }
                }
                c_ast::ExternalDeclaration::Declaration(decl) => {
                    // Handle structs, enums, typedefs, globals
                    items.extend(self.transform_declaration(decl.node)?);
                }
                _ => {
                    // Skip other declarations for now
                }
            }
        }
        
        Ok(Program { 
            items,
            span: Span::default(),
        })
    }
    
    /// Transform a function definition
    fn transform_function(&mut self, func: c_ast::FunctionDefinition) -> Result<Option<Item>> {
        // Extract function name
        let name = self.extract_function_name(&func.declarator.node)?;
        
        // Skip if no name (shouldn't happen)
        if name.is_empty() {
            return Ok(None);
        }
        
        self.current_function = Some(name.clone());
        
        // Extract parameters
        let params = self.extract_function_params(&func.declarator.node)?;
        
        // Extract return type from declaration specifiers
        let return_type = self.extract_return_type(&func.specifiers)?;
        
        // Transform function body
        let body = self.transform_compound_statement(&func.statement.node)?;
        
        self.current_function = None;
        
        Ok(Some(Item::Function(Function {
            name,
            generics: Vec::new(),
            params,
            return_type: Some(return_type),
            body,
            effects: vec![Effect::Unsafe], // C is always unsafe
            attributes: Vec::new(),
            visibility: Visibility::Public,
            span: Span::default(),
        })))
    }
    
    /// Extract function parameters from declarator
    fn extract_function_params(&self, declarator: &c_ast::Declarator) -> Result<Vec<Param>> {
        use c_ast::DerivedDeclarator::*;
        
        for derived in &declarator.derived {
            if let Function(func_decl) = &derived.node {
                let mut params = Vec::new();
                
                for param_decl in &func_decl.node.parameters {
                    // Extract parameter name
                    let param_name = if let Some(ref decl) = param_decl.node.declarator {
                        self.extract_declarator_name(&decl.node)?
                    } else {
                        // Anonymous parameter
                        format!("param_{}", params.len())
                    };
                    
                    // Extract parameter type
                    let param_type = self.extract_type_from_specifiers(&param_decl.node.specifiers)?;
                    
                    params.push(Param {
                        name: param_name,
                        ty: param_type,
                        mutable: true, // C parameters are mutable by default
                        default: None,
                        span: Span::default(),
                    });
                }
                
                return Ok(params);
            }
        }
        
        Ok(Vec::new())
    }
    
    /// Extract return type from declaration specifiers
    fn extract_return_type(&self, specifiers: &[Node<c_ast::DeclarationSpecifier>]) -> Result<Type> {
        self.extract_type_from_specifiers(specifiers)
    }
    
    /// Extract type from declaration specifiers
    fn extract_type_from_specifiers(&self, specifiers: &[Node<c_ast::DeclarationSpecifier>]) -> Result<Type> {
        use c_ast::DeclarationSpecifier::*;
        
        for spec in specifiers {
            if let TypeSpecifier(type_spec) = &spec.node {
                return self.type_transformer.transform_type_specifier(&type_spec.node);
            }
        }
        
        // Default to void/unit if no type specifier found
        Ok(Type::Unit(Span::default()))
    }

    /// Extract type from specifier qualifiers (used in casts/compound literals).
    fn extract_type_from_specifier_qualifiers(
        &self,
        specifiers: &[Node<c_ast::SpecifierQualifier>],
    ) -> Result<Type> {
        for spec in specifiers {
            if let c_ast::SpecifierQualifier::TypeSpecifier(type_spec) = &spec.node {
                return self.type_transformer.transform_type_specifier(&type_spec.node);
            }
        }

        Ok(Type::Unit(Span::default()))
    }
    
    /// Extract name from declarator
    fn extract_declarator_name(&self, declarator: &c_ast::Declarator) -> Result<String> {
        use c_ast::DeclaratorKind::*;
        
        match &declarator.kind.node {
            Identifier(ident) => Ok(ident.node.name.clone()),
            Declarator(inner) => self.extract_declarator_name(&inner.node),
            _ => Ok(String::new()),
        }
    }
    
    /// Transform a compound statement (block)
    fn transform_compound_statement(&mut self, stmt: &c_ast::Statement) -> Result<Block> {
        use c_ast::Statement::*;
        
        match stmt {
            Compound(items) => {
                let mut stmts = Vec::new();
                
                for item in items {
                    match &item.node {
                        c_ast::BlockItem::Statement(s) => {
                            if let Some(kain_stmt) = self.transform_statement(&s.node)? {
                                stmts.push(kain_stmt);
                            }
                        }
                        c_ast::BlockItem::Declaration(d) => {
                            // Handle local variable declarations
                            stmts.extend(self.transform_local_declaration(&d.node)?);
                        }
                        _ => {}
                    }
                }
                
                Ok(Block {
                    stmts,
                    span: Span::default(),
                })
            }
            _ => {
                // Single statement, wrap in block
                let stmt = self.transform_statement(stmt)?;
                Ok(Block {
                    stmts: stmt.into_iter().collect(),
                    span: Span::default(),
                })
            }
        }
    }
    
    /// Transform a declaration (struct, enum, typedef, global)
    fn transform_declaration(&mut self, decl: c_ast::Declaration) -> Result<Vec<Item>> {
        use c_ast::DeclarationSpecifier::*;
        
        let mut items = Vec::new();
        
        // Check for struct/enum/typedef in specifiers
        for spec in &decl.specifiers {
            match &spec.node {
                TypeSpecifier(type_spec) => {
                    match &type_spec.node {
                        c_ast::TypeSpecifier::Struct(struct_type) => {
                            if let Some(item) = self.transform_struct_declaration(&struct_type.node)? {
                                items.push(item);
                            }
                        }
                        c_ast::TypeSpecifier::Enum(enum_type) => {
                            if let Some(item) = self.transform_enum_declaration(&enum_type.node)? {
                                items.push(item);
                            }
                        }
                        _ => {}
                    }
                }
                StorageClass(storage) => {
                    // Check for typedef
                    if let c_ast::StorageClassSpecifier::Typedef = &storage.node {
                        // Handle typedef
                        for init_decl in &decl.declarators {
                            let name = self.extract_declarator_name(&init_decl.node.declarator.node)?;
                            let mut ty = self.extract_type_from_specifiers(&decl.specifiers)?;
                            if matches!(
                                &ty,
                                Type::Named { name, .. } if name == "AnonymousStruct" || name == "AnonymousEnum"
                            ) {
                                ty = Type::Named {
                                    name: name.clone(),
                                    generics: Vec::new(),
                                    span: Span::default(),
                                };
                            }
                            self.typedefs.insert(name.clone(), ty.clone());
                            self.type_transformer.add_typedef(name.clone(), ty.clone());
                            
                            items.push(Item::TypeAlias(TypeAlias {
                                name,
                                generics: Vec::new(),
                                target: ty,
                                visibility: Visibility::Public,
                                span: Span::default(),
                            }));
                        }
                    }
                }
                _ => {}
            }
        }
        
        // Handle global variable declarations
        if items.is_empty() {
            for init_decl in &decl.declarators {
                let name = self.extract_declarator_name(&init_decl.node.declarator.node)?;
                let ty = self.extract_type_from_specifiers(&decl.specifiers)?;
                
                // Extract initializer if present
                let value = if let Some(ref init) = init_decl.node.initializer {
                    self.transform_initializer(&init.node)?
                } else {
                    // Default value based on type
                    self.default_value_for_type(&ty)
                };
                
                items.push(Item::Const(Const {
                    name,
                    ty,
                    value,
                    visibility: Visibility::Public,
                    span: Span::default(),
                }));
            }
        }
        
        Ok(items)
    }
    
    /// Transform struct declaration
    fn transform_struct_declaration(&mut self, struct_type: &c_ast::StructType) -> Result<Option<Item>> {
        // Get struct name
        let name = if let Some(ref ident) = struct_type.identifier {
            ident.node.name.clone()
        } else {
            // Anonymous struct, skip for now
            return Ok(None);
        };
        
        // Get struct fields
        let mut fields = Vec::new();
        
        if let Some(ref declarations) = struct_type.declarations {
            for decl in declarations {
                let c_ast::StructDeclaration::Field(field_decl) = &decl.node else {
                    continue;
                };

                let field_decl = &field_decl.node;
                let field_type = self.extract_type_from_specifier_qualifiers(&field_decl.specifiers)?;

                for declarator in &field_decl.declarators {
                    if let Some(ref field_decl) = declarator.node.declarator {
                        let field_name = self.extract_declarator_name(&field_decl.node)?;

                        fields.push(Field {
                            name: field_name,
                            ty: field_type.clone(),
                            attributes: Vec::new(),
                            visibility: Visibility::Public,
                            default: None,
                            weak: false,
                            span: Span::default(),
                        });
                    }
                }
            }
        }
        
        let struct_def = Struct {
            name: name.clone(),
            generics: Vec::new(),
            fields,
            methods: Vec::new(),
            attributes: Vec::new(),
            visibility: Visibility::Public,
            span: Span::default(),
        };
        
        self.structs.insert(name, struct_def.clone());
        
        Ok(Some(Item::Struct(struct_def)))
    }
    
    /// Transform enum declaration
    fn transform_enum_declaration(&mut self, enum_type: &c_ast::EnumType) -> Result<Option<Item>> {
        // Get enum name
        let name = if let Some(ref ident) = enum_type.identifier {
            ident.node.name.clone()
        } else {
            // Anonymous enum, skip for now
            return Ok(None);
        };
        
        // Get enum variants
        let mut variants = Vec::new();
        
        for enumerator in &enum_type.enumerators {
            let variant_name = enumerator.node.identifier.node.name.clone();

            variants.push(Variant {
                name: variant_name,
                fields: VariantFields::Unit,
                span: Span::default(),
            });
        }
        
        let enum_def = Enum {
            name: name.clone(),
            generics: Vec::new(),
            variants,
            visibility: Visibility::Public,
            span: Span::default(),
        };
        
        self.enums.insert(name, enum_def.clone());
        
        Ok(Some(Item::Enum(enum_def)))
    }
    
    /// Transform local variable declaration
    fn transform_local_declaration(&mut self, decl: &c_ast::Declaration) -> Result<Vec<Stmt>> {
        let mut stmts = Vec::new();
        
        for init_decl in &decl.declarators {
            let name = self.extract_declarator_name(&init_decl.node.declarator.node)?;
            let ty = self.extract_type_from_specifiers(&decl.specifiers)?;
            
            let value = if let Some(ref init) = init_decl.node.initializer {
                Some(self.transform_initializer(&init.node)?)
            } else {
                None
            };
            
            stmts.push(Stmt::Let {
                pattern: Pattern::Binding {
                    name,
                    mutable: true,
                    span: Span::default(),
                },
                ty: Some(ty),
                value,
                span: Span::default(),
            });
        }
        
        Ok(stmts)
    }
    
    /// Transform initializer expression
    fn transform_initializer(&mut self, init: &c_ast::Initializer) -> Result<Expr> {
        use c_ast::Initializer::*;
        
        match init {
            Expression(expr) => self.transform_expression(&expr.node),
            List(items) => {
                // Array/struct initializer list
                let mut exprs = Vec::new();
                for item in items {
                    exprs.push(self.transform_initializer(&item.node.initializer.node)?);
                }
                Ok(Expr::Array(exprs, Span::default()))
            }
        }
    }
    
    /// Default value for a type
    fn default_value_for_type(&self, ty: &Type) -> Expr {
        match ty {
            Type::Named { name, .. } => {
                match name.as_str() {
                    "Int" | "i32" | "i64" => Expr::Int(0, Span::default()),
                    "Float" | "f32" | "f64" => Expr::Float(0.0, Span::default()),
                    "Bool" => Expr::Bool(false, Span::default()),
                    "Char" => Expr::String(String::new(), Span::default()),
                    _ => Expr::None(Span::default()),
                }
            }
            Type::Unit(_) => Expr::Tuple(Vec::new(), Span::default()),
            _ => Expr::None(Span::default()),
        }
    }

    fn estimate_sizeof_named_type(&self, name: &str, visited: &mut HashSet<String>) -> i64 {
        if !visited.insert(name.to_string()) {
            return 8;
        }

        let value = match name {
            "Bool" => 1,
            "Char" => 1,
            "Int" | "Float" => 8,
            other => {
                if let Some(struct_def) = self.structs.get(other) {
                    struct_def
                        .fields
                        .iter()
                        .map(|field| self.estimate_sizeof_type_inner(&field.ty, visited))
                        .sum()
                } else if let Some(alias_ty) = self.typedefs.get(other) {
                    self.estimate_sizeof_type_inner(alias_ty, visited)
                } else {
                    8
                }
            }
        };

        visited.remove(name);
        value
    }

    fn estimate_sizeof_type_inner(&self, ty: &Type, visited: &mut HashSet<String>) -> i64 {
        match ty {
            Type::Unit(_) => 0,
            Type::Named { name, .. } => self.estimate_sizeof_named_type(name, visited),
            Type::Array(inner, count, _) => {
                let elem = self.estimate_sizeof_type_inner(inner, visited);
                (elem * *count as i64).max(0)
            }
            Type::Slice(_, _) => 8,
            Type::Ref { .. } => 8,
            Type::Tuple(items, _) => items
                .iter()
                .map(|item| self.estimate_sizeof_type_inner(item, visited))
                .sum(),
            _ => 8,
        }
    }

    fn estimate_sizeof_type(&self, ty: &Type) -> i64 {
        let mut visited = HashSet::new();
        self.estimate_sizeof_type_inner(ty, &mut visited)
    }

    fn estimate_sizeof_expression(&mut self, expr: &c_ast::Expression) -> i64 {
        use c_ast::Expression::*;

        match expr {
            Identifier(ident) => {
                let name = ident.node.name.as_str();
                if self.structs.contains_key(name) {
                    self.estimate_sizeof_type(&Type::Named {
                        name: name.to_string(),
                        generics: Vec::new(),
                        span: Span::default(),
                    })
                } else if let Some(alias_ty) = self.typedefs.get(name) {
                    self.estimate_sizeof_type(alias_ty)
                } else {
                    8
                }
            }
            Cast(cast) => {
                let ty =
                    self.extract_type_from_specifier_qualifiers(&cast.node.type_name.node.specifiers);
                match ty {
                    Ok(ty) => self.estimate_sizeof_type(&ty),
                    Err(_) => 8,
                }
            }
            Constant(constant) => match &constant.node {
                c_ast::Constant::Character(_) => 1,
                c_ast::Constant::Integer(_) => 8,
                c_ast::Constant::Float(_) => 8,
            },
            StringLiteral(_) => 8,
            _ => 8,
        }
    }

    fn lower_inc_dec(&mut self, operand: &c_ast::Expression, increment: bool) -> Result<Expr> {
        let operand_expr = self.transform_expression(operand)?;
        let updated = Expr::Binary {
            left: Box::new(operand_expr.clone()),
            op: if increment { BinaryOp::Add } else { BinaryOp::Sub },
            right: Box::new(Expr::Int(1, Span::default())),
            span: Span::default(),
        };

        Ok(Expr::Assign {
            target: Box::new(operand_expr),
            value: Box::new(updated),
            span: Span::default(),
        })
    }

    fn ensure_binary_op_supported(&self, op: BinaryOp) -> Result<()> {
        if !self.language_capabilities.supports_parser_binary_op(op) {
            return Err(ImportError::UnsupportedFeature(format!(
                "Binary operator '{:?}' is not enabled by parser capabilities",
                op
            )));
        }

        if !self.language_capabilities.supports_runtime_binary_op(op) {
            return Err(ImportError::UnsupportedFeature(format!(
                "Binary operator '{:?}' is not enabled by runtime capabilities",
                op
            )));
        }

        Ok(())
    }

    fn lower_assignment_expression(
        &mut self,
        operator: &c_ast::BinaryOperator,
        lhs: &c_ast::Expression,
        rhs: &c_ast::Expression,
    ) -> Result<Expr> {
        if matches!(operator, c_ast::BinaryOperator::Assign) {
            let target = Box::new(self.transform_expression(lhs)?);
            let value = Box::new(self.transform_expression(rhs)?);
            return Ok(Expr::Assign {
                target,
                value,
                span: Span::default(),
            });
        }

        if let Some(lowered_op) = resolve_c_compound_assignment_binary_operator(operator) {
            self.ensure_binary_op_supported(lowered_op)?;
            let target_expr = self.transform_expression(lhs)?;
            let value_expr = self.transform_expression(rhs)?;
            let updated = Expr::Binary {
                left: Box::new(target_expr.clone()),
                op: lowered_op,
                right: Box::new(value_expr),
                span: Span::default(),
            };
            return Ok(Expr::Assign {
                target: Box::new(target_expr),
                value: Box::new(updated),
                span: Span::default(),
            });
        }

        Err(ImportError::UnsupportedFeature(format!(
            "Binary assignment operator is not representable in KAIN AST: {:?}",
            operator
        )))
    }
    
    /// Extract function name from declarator
    fn extract_function_name(&self, declarator: &c_ast::Declarator) -> Result<String> {
        use c_ast::DeclaratorKind::*;
        
        match &declarator.kind.node {
            Identifier(ident) => Ok(ident.node.name.clone()),
            Declarator(inner) => self.extract_function_name(&inner.node),
            _ => Ok(String::new()),
        }
    }
    
    /// Transform a statement
    fn transform_statement(&mut self, stmt: &c_ast::Statement) -> Result<Option<Stmt>> {
        use c_ast::Statement::*;
        
        match stmt {
            Return(expr) => {
                let value = if let Some(e) = expr {
                    Some(self.transform_expression(&e.node)?)
                } else {
                    None
                };
                Ok(Some(Stmt::Return(value, Span::default())))
            }
            
            If(if_stmt) => {
                let condition = self.transform_expression(&if_stmt.node.condition.node)?;
                let then_body = self.transform_compound_statement(&if_stmt.node.then_statement.node)?;
                
                let else_branch = if let Some(ref else_stmt) = if_stmt.node.else_statement {
                    Some(self.transform_compound_statement(&else_stmt.node)?)
                } else {
                    None
                };
                
                // Convert to expression statement with if expression
                Ok(Some(Stmt::Expr(Expr::If {
                    condition: Box::new(condition),
                    then_branch: then_body,
                    else_branch: else_branch.map(|b| Box::new(ElseBranch::Else(b))),
                    span: Span::default(),
                })))
            }
            
            While(while_stmt) => {
                let condition = self.transform_expression(&while_stmt.node.expression.node)?;
                let body = self.transform_compound_statement(&while_stmt.node.statement.node)?;
                
                Ok(Some(Stmt::While {
                    condition,
                    body,
                    span: Span::default(),
                }))
            }
            
            For(for_stmt) => {
                // C for loop: for(init; cond; step) body
                // Transform to KAIN while loop with init before and step at end
                let mut stmts = Vec::new();
                
                // Handle init
                match &for_stmt.node.initializer.node {
                    c_ast::ForInitializer::Declaration(decl) => {
                        stmts.extend(self.transform_local_declaration(&decl.node)?);
                    }
                    c_ast::ForInitializer::Expression(expr) => {
                        stmts.push(Stmt::Expr(self.transform_expression(&expr.node)?));
                    }
                    _ => {}
                }
                
                // Build while loop with condition
                let condition = if let Some(ref cond) = for_stmt.node.condition {
                    self.transform_expression(&cond.node)?
                } else {
                    Expr::Bool(true, Span::default())
                };
                
                // Transform body and add step at end
                let mut body_stmts = self.transform_compound_statement(&for_stmt.node.statement.node)?.stmts;
                
                if let Some(ref step) = for_stmt.node.step {
                    body_stmts.push(Stmt::Expr(self.transform_expression(&step.node)?));
                }
                
                stmts.push(Stmt::While {
                    condition,
                    body: Block {
                        stmts: body_stmts,
                        span: Span::default(),
                    },
                    span: Span::default(),
                });
                
                // Return block as expression statement
                Ok(Some(Stmt::Expr(Expr::Block(
                    Block {
                        stmts,
                        span: Span::default(),
                    },
                    Span::default(),
                ))))
            }
            
            Expression(expr) => {
                if let Some(e) = expr {
                    Ok(Some(Stmt::Expr(self.transform_expression(&e.node)?)))
                } else {
                    Ok(None)
                }
            }
            
            Break => Ok(Some(Stmt::Break(None, Span::default()))),
            
            Continue => Ok(Some(Stmt::Continue(Span::default()))),
            
            Compound(_) => {
                let block = self.transform_compound_statement(stmt)?;
                Ok(Some(Stmt::Expr(Expr::Block(block, Span::default()))))
            }
            
            _ => {
                // Skip unsupported statements
                Ok(None)
            }
        }
    }
    
    /// Transform an expression
    fn transform_expression(&mut self, expr: &c_ast::Expression) -> Result<Expr> {
        use c_ast::Expression::*;
        
        match expr {
            // Literals
            Constant(constant) => self.transform_constant(&constant.node),
            
            StringLiteral(string_lit) => {
                Ok(Expr::String(
                    string_lit.node.iter().cloned().collect::<Vec<_>>().join(""),
                    Span::default(),
                ))
            }
            
            // Identifier
            Identifier(ident) => {
                Ok(Expr::Ident(ident.node.name.clone(), Span::default()))
            }
            
            // Binary operations
            BinaryOperator(bin_op) => {
                use c_ast::BinaryOperator as BinOp;
                
                // Handle special cases: Index and Assign
                match &bin_op.node.operator.node {
                    BinOp::Index => {
                        // Array subscript: lhs[rhs]
                        let object = Box::new(self.transform_expression(&bin_op.node.lhs.node)?);
                        let idx = Box::new(self.transform_expression(&bin_op.node.rhs.node)?);
                        
                        Ok(Expr::Index {
                            object,
                            index: idx,
                            span: Span::default(),
                        })
                    }
                    BinOp::Assign
                    | BinOp::AssignPlus
                    | BinOp::AssignMinus
                    | BinOp::AssignMultiply
                    | BinOp::AssignDivide
                    | BinOp::AssignModulo
                    | BinOp::AssignShiftLeft
                    | BinOp::AssignShiftRight
                    | BinOp::AssignBitwiseAnd
                    | BinOp::AssignBitwiseOr
                    | BinOp::AssignBitwiseXor => self.lower_assignment_expression(
                        &bin_op.node.operator.node,
                        &bin_op.node.lhs.node,
                        &bin_op.node.rhs.node,
                    ),
                    _ => {
                        // Regular binary operation
                        let left = Box::new(self.transform_expression(&bin_op.node.lhs.node)?);
                        let right = Box::new(self.transform_expression(&bin_op.node.rhs.node)?);
                        let op = self.transform_binary_operator(&bin_op.node.operator.node)?;
                        
                        Ok(Expr::Binary {
                            left,
                            op,
                            right,
                            span: Span::default(),
                        })
                    }
                }
            }
            
            // Unary operations
            UnaryOperator(unary_op) => {
                match unary_op.node.operator.node {
                    c_ast::UnaryOperator::PostIncrement | c_ast::UnaryOperator::PreIncrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, true)
                    }
                    c_ast::UnaryOperator::PostDecrement | c_ast::UnaryOperator::PreDecrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, false)
                    }
                    c_ast::UnaryOperator::Plus => {
                        self.transform_expression(&unary_op.node.operand.node)
                    }
                    _ => {
                        let operand =
                            Box::new(self.transform_expression(&unary_op.node.operand.node)?);
                        let op = self.transform_unary_operator(&unary_op.node.operator.node)?;

                        Ok(Expr::Unary {
                            op,
                            operand,
                            span: Span::default(),
                        })
                    }
                }
            }

            // sizeof(type)
            SizeOfTy(size_of_ty) => {
                let target_ty =
                    self.extract_type_from_specifier_qualifiers(&size_of_ty.node.0.node.specifiers)?;
                Ok(Expr::Int(
                    self.estimate_sizeof_type(&target_ty),
                    Span::default(),
                ))
            }

            // sizeof(expr)
            SizeOfVal(size_of_val) => Ok(Expr::Int(
                self.estimate_sizeof_expression(&size_of_val.node.0.node),
                Span::default(),
            )),

            // alignof(type) -> conservative machine-word approximation.
            AlignOf(_) => Ok(Expr::Int(8, Span::default())),
            
            // Function call
            Call(call) => {
                let callee = Box::new(self.transform_expression(&call.node.callee.node)?);
                let mut args = Vec::new();
                
                for arg in &call.node.arguments {
                    args.push(CallArg {
                        name: None,
                        value: self.transform_expression(&arg.node)?,
                        span: Span::default(),
                    });
                }
                
                Ok(Expr::Call {
                    callee,
                    args,
                    span: Span::default(),
                })
            }
            
            // Member access
            Member(member) => {
                let object = Box::new(self.transform_expression(&member.node.expression.node)?);
                let field = member.node.identifier.node.name.clone();
                
                Ok(Expr::Field {
                    object,
                    field,
                    span: Span::default(),
                })
            }
            
            // Cast
            Cast(cast) => {
                let value = Box::new(self.transform_expression(&cast.node.expression.node)?);
                let target = self.extract_type_from_specifier_qualifiers(&cast.node.type_name.node.specifiers)?;
                
                Ok(Expr::Cast {
                    value,
                    target,
                    span: Span::default(),
                })
            }
            
            // Conditional (ternary)
            Conditional(cond) => {
                let condition = Box::new(self.transform_expression(&cond.node.condition.node)?);
                let then_expr = self.transform_expression(&cond.node.then_expression.node)?;
                let else_expr = self.transform_expression(&cond.node.else_expression.node)?;
                
                Ok(Expr::If {
                    condition,
                    then_branch: Block {
                        stmts: vec![Stmt::Expr(then_expr)],
                        span: Span::default(),
                    },
                    else_branch: Some(Box::new(ElseBranch::Else(Block {
                        stmts: vec![Stmt::Expr(else_expr)],
                        span: Span::default(),
                    }))),
                    span: Span::default(),
                })
            }

            // Comma operator evaluates left-to-right and yields last expression.
            Comma(exprs) => {
                if let Some(last) = exprs.last() {
                    self.transform_expression(&last.node)
                } else {
                    Ok(Expr::Int(0, Span::default()))
                }
            }
            
            // Compound literal (struct initialization)
            CompoundLiteral(compound) => {
                // Extract type name
                let type_name = self.extract_type_from_specifier_qualifiers(&compound.node.type_name.node.specifiers)?;
                
                if let Type::Named { name, .. } = type_name {
                    // Transform initializer list to struct fields
                    let fields = self.transform_compound_initializer(&compound.node.initializer_list)?;
                    
                    Ok(Expr::Struct {
                        name,
                        fields,
                        span: Span::default(),
                    })
                } else {
                    // Array or other compound literal
                    let mut exprs = Vec::new();
                    for item in &compound.node.initializer_list {
                        exprs.push(self.transform_initializer(&item.node.initializer.node)?);
                    }
                    Ok(Expr::Array(exprs, Span::default()))
                }
            }
            
            _ => {
                // Unsupported expression, return placeholder
                Err(ImportError::UnsupportedFeature(format!("Expression: {:?}", expr)))
            }
        }
    }
    
    /// Transform a constant
    fn transform_constant(&self, constant: &c_ast::Constant) -> Result<Expr> {
        use c_ast::Constant::*;
        
        match constant {
            Integer(int) => {
                // Parse integer value
                let value = int.number.parse::<i64>()
                    .unwrap_or(0);
                Ok(Expr::Int(value, Span::default()))
            }
            Float(float) => {
                // Parse float value
                let value = float.number.parse::<f64>()
                    .unwrap_or(0.0);
                Ok(Expr::Float(value, Span::default()))
            }
            Character(ch) => {
                Ok(Expr::String(ch.clone(), Span::default()))
            }
        }
    }
    
    /// Transform binary operator
    fn transform_binary_operator(&self, op: &c_ast::BinaryOperator) -> Result<BinaryOp> {
        let mapped = match resolve_c_binary_operator(op) {
            CBinaryOperatorResolution::Supported(mapped) => mapped,
            CBinaryOperatorResolution::UnsupportedAssignment => {
                return Err(ImportError::UnsupportedFeature(format!(
                    "Binary assignment operator is not representable in KAIN AST: {:?}",
                    op
                )));
            }
            CBinaryOperatorResolution::Unsupported => {
                return Err(ImportError::UnsupportedFeature(format!("Binary operator: {:?}", op)));
            }
        };

        self.ensure_binary_op_supported(mapped)?;
        Ok(mapped)
    }
    
    /// Transform unary operator
    fn transform_unary_operator(&self, op: &c_ast::UnaryOperator) -> Result<UnaryOp> {
        use c_ast::UnaryOperator::*;
        
        Ok(match op {
            Minus => UnaryOp::Neg,
            Negate => UnaryOp::Not,
            Complement => UnaryOp::BitNot,
            Address => UnaryOp::Ref,
            Indirection => UnaryOp::Deref,
            _ => return Err(ImportError::UnsupportedFeature(format!("Unary operator: {:?}", op))),
        })
    }
    
    /// Transform compound initializer to struct fields
    fn transform_compound_initializer(
        &mut self,
        items: &[Node<c_ast::InitializerListItem>],
    ) -> Result<Vec<(String, Expr)>> {
        let mut fields = Vec::new();

        for (idx, item) in items.iter().enumerate() {
            // Check for designated initializer
            let field_name = if !item.node.designation.is_empty() {
                // Extract field name from designator
                if let c_ast::Designator::Member(ident) = &item.node.designation[0].node {
                    ident.node.name.clone()
                } else {
                    format!("field_{}", idx)
                }
            } else {
                format!("field_{}", idx)
            };

            let value = self.transform_initializer(&item.node.initializer.node)?;
            fields.push((field_name, value));
        }

        Ok(fields)
    }
}

impl Default for CTransformer {
    fn default() -> Self {
        Self::new()
    }
}

/// Transform a C translation unit to KAIN program
pub fn transform(tu: c_ast::TranslationUnit) -> Result<Program> {
    let mut transformer = CTransformer::new();
    transformer.transform(tu)
}

pub fn transform_with_language_capabilities(
    tu: c_ast::TranslationUnit,
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    let mut transformer = CTransformer::with_language_capabilities(language_capabilities);
    transformer.transform(tu)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::c::parser::parse_c_source;
    use kain_core::language_features::{LanguageCapability, LanguageCapabilities};
    
    #[test]
    fn test_transform_simple_function() {
        let source = r#"
            int add(int a, int b) {
                return a + b;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Function(func) = &program.items[0] {
            assert_eq!(func.name, "add");
            assert_eq!(func.params.len(), 2);
            assert_eq!(func.params[0].name, "a");
            assert_eq!(func.params[1].name, "b");
        } else {
            panic!("Expected function item");
        }
    }

    #[test]
    fn test_transform_bitwise_binary_operators() {
        let source = r#"
            int blend(int a, int b) {
                return (a & b) | (a ^ b) << 1;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);

        assert!(result.is_ok(), "expected bitwise operators to transform successfully");
    }

    #[test]
    fn test_transform_respects_capability_profile() {
        let source = r#"
            int mask(int a, int b) {
                return a & b;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let caps = LanguageCapabilities::default()
            .with_override(LanguageCapability::ParserBitwiseAnd, false);
        let result = transform_with_language_capabilities(c_ast, caps);

        assert!(result.is_err(), "expected transform to fail when bitwise '&' is disabled");
    }
    
    #[test]
    fn test_transform_struct() {
        let source = r#"
            struct Point {
                int x;
                int y;
            };
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Struct(s) = &program.items[0] {
            assert_eq!(s.name, "Point");
            assert_eq!(s.fields.len(), 2);
            assert_eq!(s.fields[0].name, "x");
            assert_eq!(s.fields[1].name, "y");
        } else {
            panic!("Expected struct item");
        }
    }
    
    #[test]
    fn test_transform_enum() {
        let source = r#"
            enum Color {
                RED,
                GREEN,
                BLUE
            };
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Enum(e) = &program.items[0] {
            assert_eq!(e.name, "Color");
            assert_eq!(e.variants.len(), 3);
            assert_eq!(e.variants[0].name, "RED");
            assert_eq!(e.variants[1].name, "GREEN");
            assert_eq!(e.variants[2].name, "BLUE");
        } else {
            panic!("Expected enum item");
        }
    }
    
    #[test]
    fn test_transform_typedef() {
        let source = r#"
            typedef int MyInt;
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::TypeAlias(alias) = &program.items[0] {
            assert_eq!(alias.name, "MyInt");
        } else {
            panic!("Expected type alias item");
        }
    }
    
    #[test]
    fn test_transform_if_statement() {
        let source = r#"
            int max(int a, int b) {
                if (a > b) {
                    return a;
                } else {
                    return b;
                }
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Function(func) = &program.items[0] {
            assert_eq!(func.name, "max");
            assert!(!func.body.stmts.is_empty());
        } else {
            panic!("Expected function item");
        }
    }
    
    #[test]
    fn test_transform_while_loop() {
        let source = r#"
            int sum(int n) {
                int result = 0;
                int i = 0;
                while (i < n) {
                    result = result + i;
                    i = i + 1;
                }
                return result;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Function(func) = &program.items[0] {
            assert_eq!(func.name, "sum");
            assert!(!func.body.stmts.is_empty());
        } else {
            panic!("Expected function item");
        }
    }
    
    #[test]
    fn test_transform_for_loop() {
        let source = r#"
            int factorial(int n) {
                int result = 1;
                for (int i = 1; i <= n; i = i + 1) {
                    result = result * i;
                }
                return result;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
        
        if let Item::Function(func) = &program.items[0] {
            assert_eq!(func.name, "factorial");
            assert!(!func.body.stmts.is_empty());
        } else {
            panic!("Expected function item");
        }
    }
    
    #[test]
    fn test_transform_binary_operations() {
        let source = r#"
            int calculate(int a, int b) {
                int sum = a + b;
                int diff = a - b;
                int prod = a * b;
                int quot = a / b;
                int rem = a % b;
                return sum;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
    }
    
    #[test]
    fn test_transform_function_call() {
        let source = r#"
            int helper(int x) {
                return x * 2;
            }
            
            int main() {
                int result = helper(5);
                return result;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 2);
    }
    
    #[test]
    fn test_transform_array_access() {
        let source = r#"
            int get_element(int arr[], int index) {
                return arr[index];
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 1);
    }
    
    #[test]
    fn test_transform_struct_member_access() {
        let source = r#"
            struct Point {
                int x;
                int y;
            };
            
            int get_x(struct Point p) {
                return p.x;
            }
        "#;
        
        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        
        assert!(result.is_ok());
        let program = result.unwrap();
        assert_eq!(program.items.len(), 2);
    }

    #[test]
    fn test_transform_compound_assignments_by_lowering() {
        let source = r#"
            int crunch(int a, int b) {
                a <<= 1;
                a |= b;
                a ^= b;
                a &= b;
                a %= b;
                return a;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        assert!(result.is_ok());
    }
}
