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
use crate::common::identifier_registry::{IdentifierDomain, StableIdentifierRenamer};
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

    /// Stable identifier mapper that keeps declarations/use-sites aligned.
    identifier_renamer: StableIdentifierRenamer,

    /// KAIN language capability profile used for data-driven lowering decisions.
    language_capabilities: LanguageCapabilities,

    /// Synthetic temporary counter for sequence-preserving lowering.
    temp_counter: usize,

    /// Value symbol scopes used for type-directed lowering decisions.
    symbol_scopes: Vec<HashMap<String, Type>>,
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
            identifier_renamer: StableIdentifierRenamer::default(),
            language_capabilities,
            temp_counter: 0,
            symbol_scopes: vec![HashMap::new()],
        }
    }

    fn rename_identifier(&mut self, domain: IdentifierDomain, raw: &str) -> String {
        self.identifier_renamer.resolve(domain, raw)
    }

    fn rename_value_identifier(&mut self, raw: &str) -> String {
        self.rename_identifier(IdentifierDomain::Value, raw)
    }

    fn rename_type_identifier(&mut self, raw: &str) -> String {
        self.rename_identifier(IdentifierDomain::Type, raw)
    }

    fn rename_field_identifier(&mut self, raw: &str) -> String {
        self.rename_identifier(IdentifierDomain::Field, raw)
    }

    fn rename_variant_identifier(&mut self, raw: &str) -> String {
        self.rename_identifier(IdentifierDomain::Variant, raw)
    }

    fn fresh_temp(&mut self, prefix: &str) -> String {
        let name = format!("{}_{}", prefix, self.temp_counter);
        self.temp_counter += 1;
        self.rename_value_identifier(&name)
    }

    fn push_symbol_scope(&mut self) {
        self.symbol_scopes.push(HashMap::new());
    }

    fn pop_symbol_scope(&mut self) {
        if self.symbol_scopes.len() > 1 {
            self.symbol_scopes.pop();
        }
    }

    fn define_symbol_type(&mut self, name: String, ty: Type) {
        if let Some(scope) = self.symbol_scopes.last_mut() {
            scope.insert(name, ty);
        }
    }

    fn lookup_symbol_type(&self, name: &str) -> Option<Type> {
        self.symbol_scopes
            .iter()
            .rev()
            .find_map(|scope| scope.get(name).cloned())
    }

    fn sanitize_type(&mut self, ty: Type) -> Type {
        match ty {
            Type::Named {
                name,
                generics,
                span,
            } => Type::Named {
                name: self.rename_type_identifier(&name),
                generics: generics
                    .into_iter()
                    .map(|inner| self.sanitize_type(inner))
                    .collect(),
                span,
            },
            Type::Tuple(items, span) => Type::Tuple(
                items
                    .into_iter()
                    .map(|inner| self.sanitize_type(inner))
                    .collect(),
                span,
            ),
            Type::Array(inner, len, span) => {
                Type::Array(Box::new(self.sanitize_type(*inner)), len, span)
            }
            Type::Slice(inner, span) => Type::Slice(Box::new(self.sanitize_type(*inner)), span),
            Type::Ref {
                mutable,
                inner,
                lifetime,
                span,
            } => Type::Ref {
                mutable,
                inner: Box::new(self.sanitize_type(*inner)),
                lifetime,
                span,
            },
            Type::Function {
                params,
                return_type,
                effects,
                span,
            } => Type::Function {
                params: params
                    .into_iter()
                    .map(|inner| self.sanitize_type(inner))
                    .collect(),
                return_type: Box::new(self.sanitize_type(*return_type)),
                effects,
                span,
            },
            Type::Option(inner, span) => Type::Option(Box::new(self.sanitize_type(*inner)), span),
            Type::Result(ok, err, span) => Type::Result(
                Box::new(self.sanitize_type(*ok)),
                Box::new(self.sanitize_type(*err)),
                span,
            ),
            Type::Impl {
                trait_name,
                generics,
                span,
            } => Type::Impl {
                trait_name: self.rename_type_identifier(&trait_name),
                generics: generics
                    .into_iter()
                    .map(|inner| self.sanitize_type(inner))
                    .collect(),
                span,
            },
            other => other,
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
        let raw_name = self.extract_function_name(&func.declarator.node)?;
        let name = self.rename_value_identifier(&raw_name);
        
        // Skip if no name (shouldn't happen)
        if name.is_empty() {
            return Ok(None);
        }
        
        self.current_function = Some(name.clone());
        
        // Extract parameters
        let params = self.extract_function_params(&func.declarator.node)?;
        
        // Extract return type from declaration specifiers
        let return_type = self.extract_return_type(&func.specifiers, Some(&func.declarator.node))?;

        self.push_symbol_scope();
        for param in &params {
            self.define_symbol_type(param.name.clone(), param.ty.clone());
        }

        // Transform function body
        let body = self.transform_compound_statement(&func.statement.node)?;
        self.pop_symbol_scope();

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
    fn extract_function_params(&mut self, declarator: &c_ast::Declarator) -> Result<Vec<Param>> {
        use c_ast::DerivedDeclarator::*;
        
        for derived in &declarator.derived {
            if let Function(func_decl) = &derived.node {
                let mut params = Vec::new();
                
                for param_decl in &func_decl.node.parameters {
                    // Extract parameter name
                    let param_name = if let Some(ref decl) = param_decl.node.declarator {
                        let raw = self.extract_declarator_name(&decl.node)?;
                        self.rename_value_identifier(&raw)
                    } else {
                        // Anonymous parameter
                        format!("param_{}", params.len())
                    };
                    
                    // Extract parameter type
                    let param_type = if let Some(ref decl) = param_decl.node.declarator {
                        self.extract_type_from_declaration(&param_decl.node.specifiers, &decl.node)?
                    } else {
                        self.extract_type_from_specifiers(&param_decl.node.specifiers)?
                    };
                    
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
    fn extract_return_type(
        &mut self,
        specifiers: &[Node<c_ast::DeclarationSpecifier>],
        declarator: Option<&c_ast::Declarator>,
    ) -> Result<Type> {
        let base = self.extract_type_from_specifiers(specifiers)?;
        if let Some(declarator) = declarator {
            self.apply_declarator_type(base, declarator)
        } else {
            Ok(base)
        }
    }
    
    /// Extract type from declaration specifiers
    fn extract_type_from_specifiers(&mut self, specifiers: &[Node<c_ast::DeclarationSpecifier>]) -> Result<Type> {
        use c_ast::DeclarationSpecifier::*;
        
        for spec in specifiers {
            if let TypeSpecifier(type_spec) = &spec.node {
                let ty = self.type_transformer.transform_type_specifier(&type_spec.node)?;
                return Ok(self.sanitize_type(ty));
            }
        }
        
        // Default to void/unit if no type specifier found
        Ok(Type::Unit(Span::default()))
    }

    fn extract_type_from_declaration(
        &mut self,
        specifiers: &[Node<c_ast::DeclarationSpecifier>],
        declarator: &c_ast::Declarator,
    ) -> Result<Type> {
        let base = self.extract_type_from_specifiers(specifiers)?;
        self.apply_declarator_type(base, declarator)
    }

    /// Extract type from specifier qualifiers (used in casts/compound literals).
    fn extract_type_from_specifier_qualifiers(
        &mut self,
        specifiers: &[Node<c_ast::SpecifierQualifier>],
    ) -> Result<Type> {
        for spec in specifiers {
            if let c_ast::SpecifierQualifier::TypeSpecifier(type_spec) = &spec.node {
                let ty = self.type_transformer.transform_type_specifier(&type_spec.node)?;
                return Ok(self.sanitize_type(ty));
            }
        }

        Ok(Type::Unit(Span::default()))
    }

    fn extract_type_from_type_name(&mut self, type_name: &c_ast::TypeName) -> Result<Type> {
        let base = self.extract_type_from_specifier_qualifiers(&type_name.specifiers)?;
        if let Some(ref declarator) = type_name.declarator {
            self.apply_declarator_type(base, &declarator.node)
        } else {
            Ok(base)
        }
    }

    fn apply_declarator_type(
        &mut self,
        mut ty: Type,
        declarator: &c_ast::Declarator,
    ) -> Result<Type> {
        for derived in declarator.derived.iter().rev() {
            ty = match &derived.node {
                c_ast::DerivedDeclarator::Pointer(qualifiers) => {
                    let qualifiers = qualifiers
                        .iter()
                        .filter_map(|qualifier| {
                            if let c_ast::PointerQualifier::TypeQualifier(type_qualifier) =
                                &qualifier.node
                            {
                                Some(type_qualifier.node.clone())
                            } else {
                                None
                            }
                        })
                        .collect::<Vec<_>>();
                    self.type_transformer.transform_pointer(ty, &qualifiers)
                }
                c_ast::DerivedDeclarator::Array(array_decl) => {
                    let size = self.extract_array_size(&array_decl.node.size);
                    self.type_transformer.transform_array(ty, size)
                }
                c_ast::DerivedDeclarator::Function(_) | c_ast::DerivedDeclarator::KRFunction(_) => {
                    ty
                }
                c_ast::DerivedDeclarator::Block(qualifiers) => {
                    let qualifiers = qualifiers
                        .iter()
                        .filter_map(|qualifier| {
                            if let c_ast::PointerQualifier::TypeQualifier(type_qualifier) =
                                &qualifier.node
                            {
                                Some(type_qualifier.node.clone())
                            } else {
                                None
                            }
                        })
                        .collect::<Vec<_>>();
                    self.type_transformer.transform_pointer(ty, &qualifiers)
                }
            };
        }

        Ok(self.sanitize_type(ty))
    }

    fn extract_array_size(&self, size: &c_ast::ArraySize) -> Option<usize> {
        let expr = match size {
            c_ast::ArraySize::VariableExpression(expr)
            | c_ast::ArraySize::StaticExpression(expr) => Some(&expr.node),
            c_ast::ArraySize::Unknown | c_ast::ArraySize::VariableUnknown => None,
        }?;

        self.extract_const_usize(expr)
    }

    fn extract_const_usize(&self, expr: &c_ast::Expression) -> Option<usize> {
        match expr {
            c_ast::Expression::Constant(constant) => {
                if let c_ast::Constant::Integer(int) = &constant.node {
                    parse_c_integer_literal(&int.number)
                } else {
                    None
                }
            }
            c_ast::Expression::UnaryOperator(unary) => {
                if matches!(unary.node.operator.node, c_ast::UnaryOperator::Plus) {
                    self.extract_const_usize(&unary.node.operand.node)
                } else {
                    None
                }
            }
            _ => None,
        }
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

                self.push_symbol_scope();
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
                self.pop_symbol_scope();

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
        let mut anonymous_struct = None;
        let mut anonymous_enum = None;

        for spec in &decl.specifiers {
            match &spec.node {
                TypeSpecifier(type_spec) => match &type_spec.node {
                    c_ast::TypeSpecifier::Struct(struct_type)
                        if struct_type.node.identifier.is_none() =>
                    {
                        anonymous_struct = Some(struct_type.node.clone());
                    }
                    c_ast::TypeSpecifier::Enum(enum_type) if enum_type.node.identifier.is_none() => {
                        anonymous_enum = Some(enum_type.node.clone());
                    }
                    _ => {}
                },
                _ => {}
            }
        }
        
        // Check for struct/enum/typedef in specifiers
        for spec in &decl.specifiers {
            match &spec.node {
                TypeSpecifier(type_spec) => {
                    match &type_spec.node {
                        c_ast::TypeSpecifier::Struct(struct_type) => {
                            if let Some(item) = self.transform_struct_declaration(&struct_type.node, None)? {
                                items.push(item);
                            }
                        }
                        c_ast::TypeSpecifier::Enum(enum_type) => {
                            if let Some(item) = self.transform_enum_declaration(&enum_type.node, None)? {
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
                            let raw_name =
                                self.extract_declarator_name(&init_decl.node.declarator.node)?;
                            let name = self.rename_type_identifier(&raw_name);
                            if let Some(struct_type) = &anonymous_struct {
                                if let Some(item) =
                                    self.transform_struct_declaration(struct_type, Some(name.clone()))?
                                {
                                    items.push(item);
                                }
                            }
                            if let Some(enum_type) = &anonymous_enum {
                                if let Some(item) =
                                    self.transform_enum_declaration(enum_type, Some(name.clone()))?
                                {
                                    items.push(item);
                                }
                            }
                            let ty = if anonymous_struct.is_some() || anonymous_enum.is_some() {
                                Type::Named {
                                    name: name.clone(),
                                    generics: Vec::new(),
                                    span: Span::default(),
                                }
                            } else {
                                self.extract_type_from_declaration(
                                    &decl.specifiers,
                                    &init_decl.node.declarator.node,
                                )?
                            };
                            self.typedefs.insert(raw_name.clone(), ty.clone());
                            if raw_name != name {
                                self.typedefs.insert(name.clone(), ty.clone());
                            }
                            self.type_transformer.add_typedef(raw_name.clone(), ty.clone());
                            if raw_name != name {
                                self.type_transformer.add_typedef(name.clone(), ty.clone());
                            }

                            if !matches!(&ty, Type::Named { name: ty_name, .. } if ty_name == &name) {
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
                }
                _ => {}
            }
        }
        
        // Handle global variable declarations
        if items.is_empty() {
            for init_decl in &decl.declarators {
                let raw_name = self.extract_declarator_name(&init_decl.node.declarator.node)?;
                let name = self.rename_value_identifier(&raw_name);
                let ty =
                    self.extract_type_from_declaration(&decl.specifiers, &init_decl.node.declarator.node)?;
                
                // Extract initializer if present
                let value = if let Some(ref init) = init_decl.node.initializer {
                    self.transform_initializer(&init.node)?
                } else {
                    // Default value based on type
                    self.default_value_for_type(&ty)
                };
                
                items.push(Item::Const(Const {
                    name,
                    ty: ty.clone(),
                    value,
                    visibility: Visibility::Public,
                    span: Span::default(),
                }));
                self.define_symbol_type(raw_name, ty);
            }
        }
        
        Ok(items)
    }
    
    /// Transform struct declaration
    fn transform_struct_declaration(
        &mut self,
        struct_type: &c_ast::StructType,
        fallback_name: Option<String>,
    ) -> Result<Option<Item>> {
        // Get struct name
        let (raw_name, name) = if let Some(ref ident) = struct_type.identifier {
            let raw_name = ident.node.name.clone();
            let sanitized = self.rename_type_identifier(&raw_name);
            (raw_name, sanitized)
        } else if let Some(name) = fallback_name {
            (name.clone(), name)
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
                for declarator in &field_decl.declarators {
                    if let Some(ref field_declarator) = declarator.node.declarator {
                        let raw_field_name = self.extract_declarator_name(&field_declarator.node)?;
                        let field_name = self.rename_field_identifier(&raw_field_name);
                        let field_type =
                            self.extract_type_from_specifier_qualifiers(&field_decl.specifiers)?;
                        let field_type =
                            self.apply_declarator_type(field_type, &field_declarator.node)?;

                        fields.push(Field {
                            name: field_name,
                            ty: field_type,
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
        
        self.structs.insert(raw_name.clone(), struct_def.clone());
        if raw_name != name {
            self.structs.insert(name, struct_def.clone());
        }
        
        Ok(Some(Item::Struct(struct_def)))
    }
    
    /// Transform enum declaration
    fn transform_enum_declaration(
        &mut self,
        enum_type: &c_ast::EnumType,
        fallback_name: Option<String>,
    ) -> Result<Option<Item>> {
        // Get enum name
        let (raw_name, name) = if let Some(ref ident) = enum_type.identifier {
            let raw_name = ident.node.name.clone();
            let sanitized = self.rename_type_identifier(&raw_name);
            (raw_name, sanitized)
        } else if let Some(name) = fallback_name {
            (name.clone(), name)
        } else {
            // Anonymous enum, skip for now
            return Ok(None);
        };
        
        // Get enum variants
        let mut variants = Vec::new();
        
        for enumerator in &enum_type.enumerators {
            let variant_name =
                self.rename_variant_identifier(&enumerator.node.identifier.node.name);

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
        
        self.enums.insert(raw_name.clone(), enum_def.clone());
        if raw_name != name {
            self.enums.insert(name, enum_def.clone());
        }
        
        Ok(Some(Item::Enum(enum_def)))
    }
    
    /// Transform local variable declaration
    fn transform_local_declaration(&mut self, decl: &c_ast::Declaration) -> Result<Vec<Stmt>> {
        let mut stmts = Vec::new();
        
        for init_decl in &decl.declarators {
            let raw_name = self.extract_declarator_name(&init_decl.node.declarator.node)?;
            let name = self.rename_value_identifier(&raw_name);
            let ty =
                self.extract_type_from_declaration(&decl.specifiers, &init_decl.node.declarator.node)?;
            
            let value = if let Some(ref init) = init_decl.node.initializer {
                Some(self.transform_initializer(&init.node)?)
            } else {
                Some(self.default_value_for_type(&ty))
            };

            self.define_symbol_type(name.clone(), ty.clone());
            
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
            Type::Array(inner, count, _) => Expr::Array(
                (0..*count)
                    .map(|_| self.default_value_for_type(inner))
                    .collect(),
                Span::default(),
            ),
            Type::Named { name, .. } => {
                match name.as_str() {
                    "Int" | "i32" | "i64" => Expr::Int(0, Span::default()),
                    "Float" | "f32" | "f64" => Expr::Float(0.0, Span::default()),
                    "Bool" => Expr::Bool(false, Span::default()),
                    "Char" => Expr::String("\0".to_string(), Span::default()),
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
                let ty = self.extract_type_from_type_name(&cast.node.type_name.node);
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

    fn lower_inc_dec(
        &mut self,
        operand: &c_ast::Expression,
        increment: bool,
        prefix: bool,
    ) -> Result<Expr> {
        let operand_expr = self.transform_expression(operand)?;
        if !matches!(
            operand_expr,
            Expr::Ident(_, _) | Expr::Field { .. } | Expr::Index { .. } | Expr::Deref(_, _)
        ) {
            return Err(ImportError::UnsupportedFeature(
                "Increment/decrement target must be assignable".to_string(),
            ));
        }

        let span = Span::default();
        let binding = self.fresh_temp("__kain_c_incdec");
        let bound_ident = Expr::Ident(binding.clone(), span);
        let updated = Expr::Binary {
            left: Box::new(bound_ident.clone()),
            op: if increment { BinaryOp::Add } else { BinaryOp::Sub },
            right: Box::new(Expr::Int(1, span)),
            span,
        };
        let assign_expr = Expr::Assign {
            target: Box::new(operand_expr.clone()),
            value: Box::new(updated.clone()),
            span,
        };
        let result_expr = if prefix { updated } else { bound_ident };
        let sequenced = Expr::Index {
            object: Box::new(Expr::Array(vec![assign_expr, result_expr], span)),
            index: Box::new(Expr::Int(1, span)),
            span,
        };

        Ok(Expr::Match {
            scrutinee: Box::new(operand_expr),
            arms: vec![MatchArm {
                pattern: Pattern::Binding {
                    name: binding,
                    mutable: false,
                    span,
                },
                guard: None,
                body: sequenced,
                span,
            }],
            span,
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

    fn infer_expr_type(&self, expr: &Expr) -> Option<Type> {
        match expr {
            Expr::Int(_, _) => Some(Type::Named {
                name: "Int".to_string(),
                generics: Vec::new(),
                span: Span::default(),
            }),
            Expr::Float(_, _) => Some(Type::Named {
                name: "Float".to_string(),
                generics: Vec::new(),
                span: Span::default(),
            }),
            Expr::String(value, _) => {
                if value.chars().count() <= 1 {
                    Some(Type::Named {
                        name: "Char".to_string(),
                        generics: Vec::new(),
                        span: Span::default(),
                    })
                } else {
                    Some(Type::Ref {
                        mutable: true,
                        inner: Box::new(Type::Named {
                            name: "Char".to_string(),
                            generics: Vec::new(),
                            span: Span::default(),
                        }),
                        lifetime: None,
                        span: Span::default(),
                    })
                }
            }
            Expr::Bool(_, _) => Some(Type::Named {
                name: "Bool".to_string(),
                generics: Vec::new(),
                span: Span::default(),
            }),
            Expr::Ident(name, _) => self.lookup_symbol_type(name),
            Expr::Field { object, field, .. } => {
                let object_ty = self.infer_expr_type(object)?;
                let struct_name = match object_ty {
                    Type::Named { name, .. } => Some(name),
                    Type::Ref { inner, .. } => match *inner {
                        Type::Named { name, .. } => Some(name),
                        _ => None,
                    },
                    _ => None,
                }?;

                let struct_def = self.structs.get(&struct_name)?;
                struct_def
                    .fields
                    .iter()
                    .find(|candidate| candidate.name == *field)
                    .map(|candidate| candidate.ty.clone())
            }
            Expr::Index { object, .. } => match self.infer_expr_type(object)? {
                Type::Array(inner, _, _) | Type::Slice(inner, _) => Some(*inner),
                Type::Ref { inner, .. } => Some(*inner),
                _ => None,
            },
            Expr::Cast { target, .. } => Some(target.clone()),
            Expr::Ref { mutable, value, .. } => Some(Type::Ref {
                mutable: *mutable,
                inner: Box::new(self.infer_expr_type(value)?),
                lifetime: None,
                span: Span::default(),
            }),
            Expr::Deref(inner, _) => match self.infer_expr_type(inner)? {
                Type::Ref { inner, .. } => Some(*inner),
                _ => None,
            },
            _ => None,
        }
    }

    fn is_pointer_like_type(&self, ty: &Type) -> bool {
        matches!(ty, Type::Ref { .. } | Type::Array(_, _, _) | Type::Slice(_, _))
    }

    fn is_integer_like_expr(&self, expr: &Expr) -> bool {
        matches!(expr, Expr::Int(_, _))
            || matches!(
                self.infer_expr_type(expr),
                Some(Type::Named { name, .. }) if name == "Int"
            )
    }

    fn lower_pointer_offset(
        &self,
        base: Expr,
        offset: Expr,
        mutable: bool,
        subtract: bool,
    ) -> Expr {
        let span = Span::default();
        let index = if subtract {
            Expr::Binary {
                left: Box::new(Expr::Int(0, span)),
                op: BinaryOp::Sub,
                right: Box::new(offset),
                span,
            }
        } else {
            offset
        };

        Expr::Ref {
            mutable,
            value: Box::new(Expr::Index {
                object: Box::new(base),
                index: Box::new(index),
                span,
            }),
            span,
        }
    }

    fn maybe_lower_pointer_arithmetic(
        &self,
        operator: &c_ast::BinaryOperator,
        left: Expr,
        right: Expr,
    ) -> Option<Expr> {
        use c_ast::BinaryOperator as BinOp;

        let left_ty = self.infer_expr_type(&left);
        let right_ty = self.infer_expr_type(&right);

        let left_is_pointer = left_ty
            .as_ref()
            .is_some_and(|ty| self.is_pointer_like_type(ty));
        let right_is_pointer = right_ty
            .as_ref()
            .is_some_and(|ty| self.is_pointer_like_type(ty));

        match operator {
            BinOp::Plus if left_is_pointer && self.is_integer_like_expr(&right) => Some(
                self.lower_pointer_offset(
                    left,
                    right,
                    matches!(left_ty, Some(Type::Ref { mutable: true, .. })),
                    false,
                ),
            ),
            BinOp::Plus if right_is_pointer && self.is_integer_like_expr(&left) => Some(
                self.lower_pointer_offset(
                    right,
                    left,
                    matches!(right_ty, Some(Type::Ref { mutable: true, .. })),
                    false,
                ),
            ),
            BinOp::Minus if left_is_pointer && self.is_integer_like_expr(&right) => Some(
                self.lower_pointer_offset(
                    left,
                    right,
                    matches!(left_ty, Some(Type::Ref { mutable: true, .. })),
                    true,
                ),
            ),
            _ => None,
        }
    }

    fn decode_c_escape_sequence(chars: &[char], idx: &mut usize) -> Option<char> {
        let next = *chars.get(*idx)?;
        *idx += 1;

        match next {
            '\'' => Some('\''),
            '"' => Some('"'),
            '?' => Some('?'),
            '\\' => Some('\\'),
            'a' => Some('\u{0007}'),
            'b' => Some('\u{0008}'),
            'f' => Some('\u{000C}'),
            'n' => Some('\n'),
            'r' => Some('\r'),
            't' => Some('\t'),
            'v' => Some('\u{000B}'),
            '0'..='7' => {
                let mut digits = String::from(next);
                while digits.len() < 3 {
                    let Some(peek) = chars.get(*idx) else {
                        break;
                    };
                    if !peek.is_ascii_digit() || *peek > '7' {
                        break;
                    }
                    digits.push(*peek);
                    *idx += 1;
                }
                u32::from_str_radix(&digits, 8).ok().and_then(char::from_u32)
            }
            'x' => {
                let mut digits = String::new();
                while let Some(peek) = chars.get(*idx) {
                    if !peek.is_ascii_hexdigit() {
                        break;
                    }
                    digits.push(*peek);
                    *idx += 1;
                }
                if digits.is_empty() {
                    None
                } else {
                    u32::from_str_radix(&digits, 16).ok().and_then(char::from_u32)
                }
            }
            'u' | 'U' => {
                let width = if next == 'u' { 4 } else { 8 };
                let mut digits = String::new();
                for _ in 0..width {
                    let Some(peek) = chars.get(*idx) else {
                        break;
                    };
                    if !peek.is_ascii_hexdigit() {
                        break;
                    }
                    digits.push(*peek);
                    *idx += 1;
                }
                if digits.is_empty() {
                    None
                } else {
                    u32::from_str_radix(&digits, 16).ok().and_then(char::from_u32)
                }
            }
            other => Some(other),
        }
    }

    fn decode_c_literal_body(&self, body: &str) -> String {
        let chars = body.chars().collect::<Vec<_>>();
        let mut decoded = String::new();
        let mut idx = 0;

        while idx < chars.len() {
            let ch = chars[idx];
            idx += 1;
            if ch == '\\' {
                if let Some(decoded_ch) = Self::decode_c_escape_sequence(&chars, &mut idx) {
                    decoded.push(decoded_ch);
                }
            } else {
                decoded.push(ch);
            }
        }

        decoded
    }

    fn strip_c_literal_delimiters<'a>(&self, token: &'a str, delimiter: char) -> Option<&'a str> {
        let start = token.find(delimiter)?;
        let end = token.rfind(delimiter)?;
        if end <= start {
            return None;
        }
        Some(&token[start + 1..end])
    }

    fn decode_c_string_literal(&self, parts: &[String]) -> String {
        parts.iter()
            .filter_map(|part| self.strip_c_literal_delimiters(part, '"'))
            .map(|body| self.decode_c_literal_body(body))
            .collect::<Vec<_>>()
            .join("")
    }

    fn decode_c_char_literal(&self, token: &str) -> String {
        self.strip_c_literal_delimiters(token, '\'')
            .map(|body| self.decode_c_literal_body(body))
            .unwrap_or_default()
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
                    self.decode_c_string_literal(&string_lit.node.iter().cloned().collect::<Vec<_>>()),
                    Span::default(),
                ))
            }
            
            // Identifier
            Identifier(ident) => {
                Ok(Expr::Ident(
                    self.rename_value_identifier(&ident.node.name),
                    Span::default(),
                ))
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
                        let left_expr = self.transform_expression(&bin_op.node.lhs.node)?;
                        let right_expr = self.transform_expression(&bin_op.node.rhs.node)?;
                        if let Some(lowered) = self.maybe_lower_pointer_arithmetic(
                            &bin_op.node.operator.node,
                            left_expr.clone(),
                            right_expr.clone(),
                        ) {
                            return Ok(lowered);
                        }

                        let left = Box::new(left_expr);
                        let right = Box::new(right_expr);
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
                    c_ast::UnaryOperator::Address => {
                        let value =
                            Box::new(self.transform_expression(&unary_op.node.operand.node)?);
                        Ok(Expr::Ref {
                            mutable: false,
                            value,
                            span: Span::default(),
                        })
                    }
                    c_ast::UnaryOperator::Indirection => {
                        let value =
                            Box::new(self.transform_expression(&unary_op.node.operand.node)?);
                        Ok(Expr::Deref(value, Span::default()))
                    }
                    c_ast::UnaryOperator::PreIncrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, true, true)
                    }
                    c_ast::UnaryOperator::PostIncrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, true, false)
                    }
                    c_ast::UnaryOperator::PreDecrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, false, true)
                    }
                    c_ast::UnaryOperator::PostDecrement => {
                        self.lower_inc_dec(&unary_op.node.operand.node, false, false)
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
                let target_ty = self.extract_type_from_type_name(&size_of_ty.node.0.node)?;
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
                let field = self.rename_field_identifier(&member.node.identifier.node.name);
                
                Ok(Expr::Field {
                    object,
                    field,
                    span: Span::default(),
                })
            }
            
            // Cast
            Cast(cast) => {
                let value = Box::new(self.transform_expression(&cast.node.expression.node)?);
                let target = self.extract_type_from_type_name(&cast.node.type_name.node)?;
                
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
                let type_name = self.extract_type_from_type_name(&compound.node.type_name.node)?;
                
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
                Ok(Expr::String(self.decode_c_char_literal(ch), Span::default()))
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
                    self.rename_field_identifier(&ident.node.name)
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

fn parse_c_integer_literal(number: &str) -> Option<usize> {
    let digits = number
        .trim()
        .trim_end_matches(|c: char| matches!(c, 'u' | 'U' | 'l' | 'L'));

    if let Some(hex) = digits.strip_prefix("0x").or_else(|| digits.strip_prefix("0X")) {
        usize::from_str_radix(hex, 16).ok()
    } else if let Some(bin) = digits.strip_prefix("0b").or_else(|| digits.strip_prefix("0B")) {
        usize::from_str_radix(bin, 2).ok()
    } else if digits.len() > 1 && digits.starts_with('0') {
        usize::from_str_radix(&digits[1..], 8).ok()
    } else {
        digits.parse::<usize>().ok()
    }
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
    fn test_transform_decodes_c_string_literals() {
        let source = r#"
            char *banner(void) {
                return "line\n";
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };
        let Stmt::Return(Some(Expr::String(value, _)), _) = &func.body.stmts[0] else {
            panic!("expected decoded string literal return");
        };
        assert_eq!(value, "line\n");
    }

    #[test]
    fn test_transform_decodes_c_char_literals() {
        let source = r#"
            char nul(void) {
                return '\0';
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };
        let Stmt::Return(Some(Expr::String(value, _)), _) = &func.body.stmts[0] else {
            panic!("expected decoded char literal return");
        };
        assert_eq!(value, "\0");
    }

    #[test]
    fn test_transform_lowers_pointer_arithmetic_to_ref_index() {
        let source = r#"
            char *advance(char *s, int i) {
                return s + i;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };
        let Stmt::Return(Some(Expr::Ref { mutable, value, .. }), _) = &func.body.stmts[0] else {
            panic!("expected pointer arithmetic to lower into ref-index form");
        };
        assert!(*mutable);

        let Expr::Index { object, index, .. } = value.as_ref() else {
            panic!("expected ref(index)");
        };
        assert!(matches!(object.as_ref(), Expr::Ident(name, _) if name == "s"));
        assert!(matches!(index.as_ref(), Expr::Ident(name, _) if name == "i"));
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

    #[test]
    fn test_transform_address_of_and_indirection() {
        let source = r#"
            int bump(int x) {
                int *p = &x;
                return *p + 1;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let result = transform(c_ast);
        assert!(result.is_ok());
    }

    #[test]
    fn test_sanitizes_reserved_value_and_field_identifiers() {
        let source = r#"
            struct Packet {
                int type;
            };

            int apply(int in) {
                struct Packet packet;
                int type = 3;
                return packet.type + in + type;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let packet_struct = program
            .items
            .iter()
            .find_map(|item| {
                if let Item::Struct(s) = item {
                    Some(s)
                } else {
                    None
                }
            })
            .expect("expected struct");
        assert_eq!(packet_struct.fields[0].name, "type_");

        let apply_fn = program
            .items
            .iter()
            .find_map(|item| {
                if let Item::Function(f) = item {
                    Some(f)
                } else {
                    None
                }
            })
            .expect("expected function");

        assert_eq!(apply_fn.params[0].name, "in_");

        let local_type_binding = apply_fn
            .body
            .stmts
            .iter()
            .find_map(|stmt| {
                if let Stmt::Let {
                    pattern: Pattern::Binding { name, .. },
                    ..
                } = stmt
                {
                    Some(name.as_str())
                } else {
                    None
                }
            })
            .expect("expected let binding");
        assert_eq!(local_type_binding, "packet");
        assert!(apply_fn
            .body
            .stmts
            .iter()
            .any(|stmt| matches!(
                stmt,
                Stmt::Let {
                    pattern: Pattern::Binding { name, .. },
                    ..
                } if name == "type_"
            )));
    }

    #[test]
    fn test_sanitizes_reserved_function_name_and_call_use_site() {
        let source = r#"
            int type(void) {
                return 1;
            }

            int run(void) {
                return type();
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        assert_eq!(program.items.len(), 2);

        let producer = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };
        assert_eq!(producer.name, "type_");

        let consumer = match &program.items[1] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Return(Some(Expr::Call { callee, .. }), _) = &consumer.body.stmts[0] else {
            panic!("expected return call expression");
        };
        let Expr::Ident(name, _) = &**callee else {
            panic!("expected identifier callee");
        };
        assert_eq!(name, "type_");
    }

    #[test]
    fn test_typedef_anonymous_struct_preserves_layout_for_sizeof() {
        let source = r#"
            typedef struct {
                int *data;
                int len;
                int cap;
            } KainArray;

            int alloc_size() {
                return sizeof(KainArray);
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        assert!(program.items.iter().any(|item| matches!(item, Item::Struct(s) if s.name == "KainArray")));

        let alloc_fn = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "alloc_size" => Some(f),
                _ => None,
            })
            .expect("expected alloc_size function");

        let Stmt::Return(Some(Expr::Int(size, _)), _) = &alloc_fn.body.stmts[0] else {
            panic!("expected integer sizeof return");
        };
        assert_eq!(*size, 24);
    }

    #[test]
    fn test_local_fixed_array_gets_real_storage_default() {
        let source = r#"
            int run() {
                char buf[2];
                buf[0] = 'a';
                return 0;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let run_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Let { ty: Some(Type::Array(_, count, _)), value: Some(Expr::Array(items, _)), .. } =
            &run_fn.body.stmts[0]
        else {
            panic!("expected fixed array local");
        };
        assert_eq!(*count, 2);
        assert_eq!(items.len(), 2);
    }

    #[test]
    fn test_post_increment_preserves_old_index_value() {
        let source = r#"
            int push(int *arr, int len, int value) {
                arr[len++] = value;
                return len;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let push_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Expr(Expr::Assign { target, .. }) = &push_fn.body.stmts[0] else {
            panic!("expected assignment expression");
        };
        let Expr::Index { index, .. } = &**target else {
            panic!("expected indexed assignment target");
        };
        let Expr::Match { .. } = &**index else {
            panic!("expected sequenced match-based postfix lowering");
        };
    }
}
