//! C to KAIN AST transformer
//!
//! Transforms lang-c AST into KAIN AST

use lang_c::ast as c_ast;
use lang_c::span::Span as CSpan;
use lang_c::span::Node;
use kain_core::ast::*;
use kain_core::low_level_abi::default_c_abi_policy;
use kain_core::diagnostic_registry::DiagnosticCode;
use kain_core::effects::Effect;
use kain_core::language_features::{default_language_capabilities, LanguageCapabilities};
use kain_core::low_level_memory_metadata::{
    marker_attr, usize_attr, usize_bool_attr, C_BITFIELD_ATTR, C_PACK_ALIGN_ATTR,
    C_PACKED_ATTR, C_STORAGE_ALIGN_ATTR, C_STORAGE_BITS_ATTR, C_TYPE_ALIGN_ATTR, C_UNION_ATTR,
};
use kain_core::span::Span;
use crate::c::types::CTypeTransformer;
use crate::c::parser::CSourceLayoutMetadata;
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

    /// Source-derived layout metadata such as active pragma-pack state and attribute hints.
    layout_metadata: Option<CSourceLayoutMetadata>,

    /// Synthetic top-level items created while lowering nested anonymous C aggregates.
    pending_items: Vec<Item>,

    /// Dedup set for synthetic anonymous aggregate types.
    emitted_synthetic_type_keys: HashSet<String>,
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
            layout_metadata: None,
            pending_items: Vec::new(),
            emitted_synthetic_type_keys: HashSet::new(),
        }
    }

    pub fn with_language_capabilities_and_layout_metadata(
        language_capabilities: LanguageCapabilities,
        layout_metadata: CSourceLayoutMetadata,
    ) -> Self {
        let mut transformer = Self::with_language_capabilities(language_capabilities);
        transformer.layout_metadata = Some(layout_metadata);
        transformer
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

    fn anonymous_type_name(
        &mut self,
        owner_name: &str,
        field_name: &str,
        kind: &str,
    ) -> String {
        let raw = format!("{}_{}_{}", owner_name, field_name, kind);
        self.rename_type_identifier(&raw)
    }

    fn register_pending_synthetic_item(&mut self, item: Item) {
        let key = match &item {
            Item::Struct(s) => format!("struct:{}", s.name),
            Item::Enum(e) => format!("enum:{}", e.name),
            _ => return,
        };

        if self.emitted_synthetic_type_keys.insert(key) {
            self.pending_items.push(item);
        }
    }

    fn drain_pending_items(&mut self, out: &mut Vec<Item>) {
        out.extend(self.pending_items.drain(..));
    }

    fn memory_lowering_required<T>(&self, context: impl Into<String>) -> Result<T> {
        let context = context.into();
        Err(ImportError::UnsupportedFeature(format!(
            "{}: {}. Add or select a pointer lowering policy before importing this expression form.",
            kain_core::diagnostic_registry::spec_for_code(DiagnosticCode::MemoryLoweringRequired)
                .code_str,
            context
        )))
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

    fn struct_def_for_type(&self, ty: &Type) -> Option<&Struct> {
        let Type::Named { name, .. } = ty else {
            return None;
        };
        self.structs.get(name)
    }

    fn struct_is_union_type(&self, ty: &Type) -> bool {
        self.struct_def_for_type(ty)
            .map(|st| st.attributes.iter().any(|attr| attr.name == C_UNION_ATTR))
            .unwrap_or(false)
    }

    fn normalize_aggregate_fields_for_type(
        &self,
        ty: &Type,
        fields: Vec<(String, Expr)>,
    ) -> Vec<(String, Expr)> {
        if !self.struct_is_union_type(ty) {
            return fields;
        }

        let Some((active_name, active_value)) = fields.last().cloned() else {
            return fields;
        };
        vec![(active_name, active_value)]
    }

    fn lookup_field(&self, ty: &Type, field_name: &str) -> Option<&Field> {
        self.struct_def_for_type(ty)?
            .fields
            .iter()
            .find(|field| field.name == field_name)
    }

    fn expr_field_is_bitfield(&self, expr: &Expr) -> bool {
        let Expr::Field { object, field, .. } = expr else {
            return false;
        };
        let Some(object_ty) = self.infer_expr_type(object) else {
            return false;
        };
        self.lookup_field(&object_ty, field)
            .map(|field_def| field_def.attributes.iter().any(|attr| attr.name == C_BITFIELD_ATTR))
            .unwrap_or(false)
    }

    fn c_storage_layout_for_specifiers(
        &self,
        specifiers: &[Node<c_ast::SpecifierQualifier>],
    ) -> (usize, usize) {
        let abi = default_c_abi_policy();
        let mut saw_char = false;
        let mut saw_short = false;
        let mut saw_long_count = 0usize;
        let mut saw_float = false;
        let mut saw_double = false;
        let mut saw_bool = false;

        for spec in specifiers {
            if let c_ast::SpecifierQualifier::TypeSpecifier(type_spec) = &spec.node {
                match &type_spec.node {
                    c_ast::TypeSpecifier::Char => saw_char = true,
                    c_ast::TypeSpecifier::Short => saw_short = true,
                    c_ast::TypeSpecifier::Long => saw_long_count += 1,
                    c_ast::TypeSpecifier::Float => saw_float = true,
                    c_ast::TypeSpecifier::Double => saw_double = true,
                    c_ast::TypeSpecifier::Bool => saw_bool = true,
                    _ => {}
                }
            }
        }

        let bits = if saw_bool {
            abi.bool_bits
        } else if saw_char {
            abi.char_bits
        } else if saw_short {
            abi.short_bits
        } else if saw_float {
            abi.float_bits
        } else if saw_double {
            abi.double_bits
        } else if saw_long_count >= 2 {
            abi.long_long_bits
        } else if saw_long_count == 1 {
            abi.long_bits
        } else {
            abi.int_bits
        };

        (bits, bits)
    }

    fn bitfield_is_signed(
        &self,
        specifiers: &[Node<c_ast::SpecifierQualifier>],
    ) -> bool {
        let mut saw_unsigned = false;

        for spec in specifiers {
            if let c_ast::SpecifierQualifier::TypeSpecifier(type_spec) = &spec.node {
                match &type_spec.node {
                    c_ast::TypeSpecifier::Unsigned => saw_unsigned = true,
                    _ => {}
                }
            }
        }

        !saw_unsigned
    }

    fn struct_layout_attributes_for_spans(&self, spans: &[CSpan]) -> Vec<Attribute> {
        let Some(metadata) = &self.layout_metadata else {
            return Vec::new();
        };

        let mut attrs = Vec::new();
        let pack_align_bits = spans
            .iter()
            .find_map(|span| metadata.pack_align_bits_for_span(*span));
        let has_explicit_packed = spans
            .iter()
            .any(|span| metadata.has_packed_attr_for_span(*span));
        let explicit_type_align_bits = spans
            .iter()
            .find_map(|span| metadata.explicit_type_align_bits_for_span(*span));

        if has_explicit_packed || matches!(pack_align_bits, Some(8)) {
            attrs.push(marker_attr(C_PACKED_ATTR, Span::default()));
        }
        if let Some(bits) = pack_align_bits {
            attrs.push(usize_attr(C_PACK_ALIGN_ATTR, bits, Span::default()));
        }
        if let Some(bits) = explicit_type_align_bits {
            attrs.push(usize_attr(C_TYPE_ALIGN_ATTR, bits, Span::default()));
        }

        attrs
    }
    
    /// Transform a C translation unit to KAIN program
    pub fn transform(&mut self, tu: c_ast::TranslationUnit) -> Result<Program> {
        let mut items = Vec::new();
        
        for decl in tu.0 {
            let decl_span = decl.span;
            match decl.node {
                c_ast::ExternalDeclaration::FunctionDefinition(func) => {
                    if let Some(item) = self.transform_function(func.node)? {
                        self.drain_pending_items(&mut items);
                        items.push(item);
                    }
                }
                c_ast::ExternalDeclaration::Declaration(decl) => {
                    // Handle structs, enums, typedefs, globals
                    let mut decl_items = self.transform_declaration(decl.node, decl_span)?;
                    self.drain_pending_items(&mut items);
                    items.append(&mut decl_items);
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
    fn transform_declaration(&mut self, decl: c_ast::Declaration, decl_span: CSpan) -> Result<Vec<Item>> {
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
                            if let Some(item) =
                                self.transform_struct_declaration(&struct_type.node, None, &[type_spec.span, decl_span])?
                            {
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
                                    self.transform_struct_declaration(struct_type, Some(name.clone()), &[decl_span])?
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
        metadata_spans: &[CSpan],
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
        let mut struct_attributes = self.struct_layout_attributes_for_spans(metadata_spans);

        if matches!(struct_type.kind.node, c_ast::StructKind::Union) {
            struct_attributes.push(marker_attr(C_UNION_ATTR, Span::default()));
        }
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
                        let mut anonymous_field_struct = None;
                        let mut anonymous_field_enum = None;
                        for spec in &field_decl.specifiers {
                            if let c_ast::SpecifierQualifier::TypeSpecifier(type_spec) = &spec.node {
                                match &type_spec.node {
                                    c_ast::TypeSpecifier::Struct(struct_type)
                                        if struct_type.node.identifier.is_none() =>
                                    {
                                        anonymous_field_struct = Some(struct_type.node.clone());
                                    }
                                    c_ast::TypeSpecifier::Enum(enum_type)
                                        if enum_type.node.identifier.is_none() =>
                                    {
                                        anonymous_field_enum = Some(enum_type.node.clone());
                                    }
                                    _ => {}
                                }
                            }
                        }

                        let field_type = if let Some(struct_type) = anonymous_field_struct {
                            let synthetic_name =
                                self.anonymous_type_name(&name, &field_name, "AnonStruct");
                            if !self.structs.contains_key(&synthetic_name) {
                                if let Some(item) = self.transform_struct_declaration(
                                    &struct_type,
                                    Some(synthetic_name.clone()),
                                    &[],
                                )? {
                                    self.register_pending_synthetic_item(item);
                                }
                            }
                            self.apply_declarator_type(
                                Type::Named {
                                    name: synthetic_name,
                                    generics: Vec::new(),
                                    span: Span::default(),
                                },
                                &field_declarator.node,
                            )?
                        } else if let Some(enum_type) = anonymous_field_enum {
                            let synthetic_name =
                                self.anonymous_type_name(&name, &field_name, "AnonEnum");
                            if !self.enums.contains_key(&synthetic_name) {
                                if let Some(item) = self.transform_enum_declaration(
                                    &enum_type,
                                    Some(synthetic_name.clone()),
                                )? {
                                    self.register_pending_synthetic_item(item);
                                }
                            }
                            self.apply_declarator_type(
                                Type::Named {
                                    name: synthetic_name,
                                    generics: Vec::new(),
                                    span: Span::default(),
                                },
                                &field_declarator.node,
                            )?
                        } else {
                            let field_type =
                                self.extract_type_from_specifier_qualifiers(&field_decl.specifiers)?;
                            self.apply_declarator_type(field_type, &field_declarator.node)?
                        };
                        let mut attributes = Vec::new();
                        let (storage_bits, storage_align_bits) =
                            self.c_storage_layout_for_specifiers(&field_decl.specifiers);
                        attributes.push(usize_attr(
                            C_STORAGE_BITS_ATTR,
                            storage_bits,
                            Span::default(),
                        ));
                        attributes.push(usize_attr(
                            C_STORAGE_ALIGN_ATTR,
                            storage_align_bits,
                            Span::default(),
                        ));
                        if let Some(bit_width) = declarator.node.bit_width.as_deref() {
                            let width = self.extract_const_usize_expr(&bit_width.node, "bitfield width")?;
                            attributes.push(usize_bool_attr(
                                C_BITFIELD_ATTR,
                                width,
                                self.bitfield_is_signed(&field_decl.specifiers),
                                Span::default(),
                            ));
                        }

                        fields.push(Field {
                            name: field_name,
                            ty: field_type,
                            attributes,
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
            attributes: struct_attributes,
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
                Some(self.transform_initializer_for_type(&init.node, Some(&ty))?)
            } else {
                Some(self.storage_default_for_type(&ty))
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
    fn storage_default_for_type(&self, ty: &Type) -> Expr {
        match ty {
            Type::Array(_, _, _) => Expr::Alloca {
                ty: ty.clone(),
                span: Span::default(),
            },
            _ => Expr::Uninit {
                ty: ty.clone(),
                span: Span::default(),
            },
        }
    }

    fn transform_compound_literal_for_type(
        &mut self,
        ty: &Type,
        items: &[Node<c_ast::InitializerListItem>],
    ) -> Result<Expr> {
        match ty {
            Type::Named { .. } => {
                let fields = self.transform_compound_initializer_for_type(ty, items)?;
                Ok(Expr::AggregateInit {
                    ty: ty.clone(),
                    fields: self.normalize_aggregate_fields_for_type(ty, fields),
                    zero_fill_rest: true,
                    span: Span::default(),
                })
            }
            Type::Array(inner, count, _) => {
                let values = self.transform_array_initializer(inner, *count, items)?;
                Ok(Expr::Array(values, Span::default()))
            }
            _ => {
                let mut exprs = Vec::new();
                for item in items {
                    exprs.push(self.transform_initializer(&item.node.initializer.node)?);
                }
                Ok(Expr::Array(exprs, Span::default()))
            }
        }
    }

    fn default_value_for_type(&self, ty: &Type) -> Expr {
        match ty {
            Type::Array(inner, count, _) => Expr::Array(
                (0..*count)
                    .map(|_| self.default_value_for_type(inner))
                    .collect(),
                Span::default(),
            ),
            Type::Named { name, .. } => {
                if self.structs.contains_key(name) {
                    return Expr::AggregateInit {
                        ty: ty.clone(),
                        fields: Vec::new(),
                        zero_fill_rest: true,
                        span: Span::default(),
                    };
                }
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

    fn field_name_for_index(&self, ty: &Type, index: usize) -> Option<String> {
        let Type::Named { name, .. } = ty else {
            return None;
        };
        self.structs
            .get(name)?
            .fields
            .get(index)
            .map(|field| field.name.clone())
    }

    fn transform_compound_initializer_for_type(
        &mut self,
        ty: &Type,
        items: &[Node<c_ast::InitializerListItem>],
    ) -> Result<Vec<(String, Expr)>> {
        let mut fields: Vec<(String, Expr)> = Vec::new();
        let mut cursor = 0usize;

        for item in items {
            let designators = item.node.designation.as_slice();
            let (field_name, remainder, next_cursor) = if let Some(first) = designators.first() {
                match &first.node {
                    c_ast::Designator::Member(ident) => (
                        self.rename_field_identifier(&ident.node.name),
                        &designators[1..],
                        cursor,
                    ),
                    c_ast::Designator::Index(index_expr) => {
                        let index = self.extract_designator_index(&index_expr.node)?;
                        (
                            self.field_name_for_index(ty, index)
                                .unwrap_or_else(|| format!("field_{}", index)),
                            &designators[1..],
                            index.saturating_add(1),
                        )
                    }
                    _ => (format!("field_{}", cursor), &designators[1..], cursor.saturating_add(1)),
                }
            } else {
                let field_name = self.field_name_for_index(ty, cursor)
                    .unwrap_or_else(|| format!("field_{}", cursor));
                (field_name, &designators[0..0], cursor.saturating_add(1))
            };

            let field_ty = self.lookup_field_type(ty, &field_name);
            let current = fields
                .iter()
                .find(|(name, _)| name == &field_name)
                .map(|(_, value)| value.clone())
                .unwrap_or_else(|| {
                    field_ty
                        .as_ref()
                        .map(|field_ty| self.default_value_for_type(field_ty))
                        .unwrap_or_else(|| Expr::None(Span::default()))
                });
            let value = if let Some(field_ty) = field_ty.as_ref() {
                self.apply_designators_to_value(
                    field_ty,
                    current,
                    remainder,
                    &item.node.initializer.node,
                )?
            } else {
                self.transform_initializer(&item.node.initializer.node)?
            };
            Self::upsert_named_expr(&mut fields, field_name, value);
            cursor = next_cursor;
        }

        Ok(self.normalize_aggregate_fields_for_type(ty, fields))
    }

    fn transform_initializer_for_type(
        &mut self,
        init: &c_ast::Initializer,
        ty: Option<&Type>,
    ) -> Result<Expr> {
        match (init, ty) {
            (c_ast::Initializer::Expression(expr), _) => self.transform_expression(&expr.node),
            (c_ast::Initializer::List(items), Some(ty)) => {
                self.transform_compound_literal_for_type(ty, items)
            }
            (c_ast::Initializer::List(items), None) => {
                let mut exprs = Vec::new();
                for item in items {
                    exprs.push(self.transform_initializer(&item.node.initializer.node)?);
                }
                Ok(Expr::Array(exprs, Span::default()))
            }
        }
    }

    fn transform_array_initializer(
        &mut self,
        element_ty: &Type,
        count: usize,
        items: &[Node<c_ast::InitializerListItem>],
    ) -> Result<Vec<Expr>> {
        let mut values = (0..count)
            .map(|_| self.default_value_for_type(element_ty))
            .collect::<Vec<_>>();
        let mut cursor = 0usize;

        for item in items {
            let designators = item.node.designation.as_slice();
            let (target_index, remainder) = if let Some(first) = designators.first() {
                match &first.node {
                    c_ast::Designator::Index(index_expr) => (
                        self.extract_designator_index(&index_expr.node)?,
                        &designators[1..],
                    ),
                    _ => (cursor, &designators[1..]),
                }
            } else {
                (cursor, &designators[0..0])
            };

            let current = values
                .get(target_index)
                .cloned()
                .unwrap_or_else(|| self.default_value_for_type(element_ty));
            let value = self.apply_designators_to_value(
                element_ty,
                current,
                remainder,
                &item.node.initializer.node,
            )?;
            if target_index < values.len() {
                values[target_index] = value;
            } else {
                values.resize_with(target_index, || self.default_value_for_type(element_ty));
                values.push(value);
            }
            cursor = target_index.saturating_add(1);
        }

        Ok(values)
    }

    fn extract_designator_index(&mut self, expr: &c_ast::Expression) -> Result<usize> {
        self.extract_const_usize_expr(expr, "designated initializer index")
    }

    fn extract_const_usize_expr(
        &mut self,
        expr: &c_ast::Expression,
        context: &str,
    ) -> Result<usize> {
        match self.transform_expression(expr)? {
            Expr::Int(value, _) if value >= 0 => Ok(value as usize),
            _ => Err(ImportError::UnsupportedFeature(format!(
                "Non-constant {}",
                context
            ))),
        }
    }

    fn lookup_field_type(&self, ty: &Type, field_name: &str) -> Option<Type> {
        let Type::Named { name, .. } = ty else {
            return None;
        };
        self.structs
            .get(name)?
            .fields
            .iter()
            .find(|field| field.name == field_name)
            .map(|field| field.ty.clone())
    }

    fn apply_designators_to_value(
        &mut self,
        ty: &Type,
        current: Expr,
        designators: &[Node<c_ast::Designator>],
        init: &c_ast::Initializer,
    ) -> Result<Expr> {
        if designators.is_empty() {
            return self.transform_initializer_for_type(init, Some(ty));
        }

        match ty {
            Type::Named { .. } => {
                let (field_name, field_ty) = match &designators[0].node {
                    c_ast::Designator::Member(ident) => {
                        let field_name = self.rename_field_identifier(&ident.node.name);
                        let field_ty = self.lookup_field_type(ty, &field_name);
                        (field_name, field_ty)
                    }
                    c_ast::Designator::Index(index_expr) => {
                        let index = self.extract_designator_index(&index_expr.node)?;
                        let field_name = self.field_name_for_index(ty, index)
                            .unwrap_or_else(|| format!("field_{}", index));
                        let field_ty = self.lookup_field_type(ty, &field_name);
                        (field_name, field_ty)
                    }
                    _ => {
                        return self.transform_initializer_for_type(init, Some(ty));
                    }
                };

                let mut fields = match current {
                    Expr::AggregateInit { fields, .. } => fields,
                    _ => Vec::new(),
                };
                let Some(field_ty) = field_ty else {
                    return self.transform_initializer_for_type(init, Some(ty));
                };
                let nested_current = fields
                    .iter()
                    .find(|(name, _)| name == &field_name)
                    .map(|(_, value)| value.clone())
                    .unwrap_or_else(|| self.default_value_for_type(&field_ty));
                let nested_value = self.apply_designators_to_value(
                    &field_ty,
                    nested_current,
                    &designators[1..],
                    init,
                )?;
                Self::upsert_named_expr(&mut fields, field_name, nested_value);
                Ok(Expr::AggregateInit {
                    ty: ty.clone(),
                    fields: self.normalize_aggregate_fields_for_type(ty, fields),
                    zero_fill_rest: true,
                    span: Span::default(),
                })
            }
            Type::Array(inner, count, _) => {
                let target_index = match &designators[0].node {
                    c_ast::Designator::Index(index_expr) => {
                        self.extract_designator_index(&index_expr.node)?
                    }
                    _ => {
                        return self.transform_initializer_for_type(init, Some(ty));
                    }
                };
                let mut values = match current {
                    Expr::Array(values, _) => values,
                    _ => (0..*count)
                        .map(|_| self.default_value_for_type(inner))
                        .collect::<Vec<_>>(),
                };
                if target_index >= values.len() {
                    values.resize_with(target_index + 1, || self.default_value_for_type(inner));
                }
                let nested_current = values
                    .get(target_index)
                    .cloned()
                    .unwrap_or_else(|| self.default_value_for_type(inner));
                let nested_value = self.apply_designators_to_value(
                    inner,
                    nested_current,
                    &designators[1..],
                    init,
                )?;
                values[target_index] = nested_value;
                Ok(Expr::Array(values, Span::default()))
            }
            _ => self.transform_initializer_for_type(init, Some(ty)),
        }
    }

    fn upsert_named_expr(fields: &mut Vec<(String, Expr)>, field_name: String, value: Expr) {
        if let Some(index) = fields.iter().position(|(name, _)| name == &field_name) {
            fields.remove(index);
        }
        fields.push((field_name, value));
    }

    fn try_lower_allocator_call(&mut self, call: &c_ast::CallExpression) -> Result<Option<Expr>> {
        let c_ast::Expression::Identifier(ident) = &call.callee.node else {
            return Ok(None);
        };
        let name = ident.node.name.as_str();
        if !matches!(name, "malloc" | "calloc" | "realloc") {
            return Ok(None);
        }

        let args = call
            .arguments
            .iter()
            .map(|arg| self.transform_expression(&arg.node))
            .collect::<Result<Vec<_>>>()?;

        let span = Span::default();
        let lowered = match (name, args.as_slice()) {
            ("malloc", [size]) => Expr::Alloc {
                size: Box::new(size.clone()),
                ty: self.infer_heap_type_from_size_expr(size),
                zeroed: false,
                span,
            },
            ("calloc", [count, elem_size]) => Expr::Alloc {
                size: Box::new(Expr::Binary {
                    left: Box::new(count.clone()),
                    op: BinaryOp::Mul,
                    right: Box::new(elem_size.clone()),
                    span,
                }),
                ty: self.infer_heap_type_from_size_expr(elem_size),
                zeroed: true,
                span,
            },
            ("realloc", [pointer, size]) => Expr::Realloc {
                pointer: Box::new(pointer.clone()),
                size: Box::new(size.clone()),
                ty: self.infer_heap_type_from_size_expr(size),
                zeroed_new: false,
                span,
            },
            _ => {
                return Ok(None);
            }
        };

        Ok(Some(lowered))
    }

    fn infer_heap_type_from_size_expr(&self, expr: &Expr) -> Option<Type> {
        match expr {
            Expr::SizeOfType { target, .. } => Some(target.clone()),
            Expr::Binary { left, op: BinaryOp::Mul, right, .. } => {
                self.infer_heap_type_from_size_expr(left)
                    .or_else(|| self.infer_heap_type_from_size_expr(right))
            }
            _ => None,
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
            Expr::Ident(_, _)
                | Expr::Field { .. }
                | Expr::Index { .. }
                | Expr::Deref(_, _)
                | Expr::MemLoad { .. }
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
            let target = self.transform_expression(lhs)?;
            let value = self.transform_expression(rhs)?;
            return self.lower_store_or_assign(target, value);
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
            return self.lower_store_or_assign(target_expr, updated);
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
                Type::Ref { inner, .. } | Type::Ptr { inner, .. } => Some(*inner),
                _ => None,
            },
            Expr::Paren(inner, _) => self.infer_expr_type(inner),
            Expr::Assign { target, .. } => self.infer_expr_type(target),
            Expr::Cast { target, .. } => Some(target.clone()),
            Expr::Array(items, _) => {
                let first = items.first()?;
                let element_ty = self.infer_expr_type(first)?;
                Some(Type::Array(
                    Box::new(element_ty),
                    items.len(),
                    Span::default(),
                ))
            }
            Expr::Struct { name, .. } => Some(Type::Named {
                name: name.clone(),
                generics: Vec::new(),
                span: Span::default(),
            }),
            Expr::AggregateInit { ty, .. } => Some(ty.clone()),
            Expr::Ref { mutable, value, .. } => Some(Type::Ptr {
                mutable: *mutable,
                inner: Box::new(self.infer_expr_type(value)?),
                provenance: PointerProvenance::ImportedC,
                span: Span::default(),
            }),
            Expr::AddrOf {
                value,
                pointee_ty,
                ..
            } => Some(Type::Ptr {
                mutable: false,
                inner: Box::new(
                    pointee_ty
                        .clone()
                        .or_else(|| self.infer_expr_type(value))
                        .unwrap_or(Type::Named {
                            name: "Int".to_string(),
                            generics: vec![],
                            span: Span::default(),
                        }),
                ),
                provenance: PointerProvenance::ImportedC,
                span: Span::default(),
            }),
            Expr::PtrOffset {
                pointer,
                element_ty,
                ..
            } => match self.infer_expr_type(pointer)? {
                Type::Ptr { mutable, inner, provenance, .. } => Some(Type::Ptr {
                    mutable,
                    inner: Box::new(element_ty.clone().unwrap_or(*inner)),
                    provenance,
                    span: Span::default(),
                }),
                Type::Ref { mutable, inner, .. } => Some(Type::Ptr {
                    mutable,
                    inner: Box::new(element_ty.clone().unwrap_or(*inner)),
                    provenance: PointerProvenance::ImportedC,
                    span: Span::default(),
                }),
                _ => None,
            },
            Expr::MemLoad {
                pointer,
                load_ty,
                ..
            } => match self.infer_expr_type(pointer)? {
                Type::Ref { inner, .. } | Type::Ptr { inner, .. } => {
                    Some(load_ty.clone().unwrap_or(*inner))
                }
                _ => None,
            },
            Expr::Deref(inner, _) => match self.infer_expr_type(inner)? {
                Type::Ref { inner, .. } | Type::Ptr { inner, .. } => Some(*inner),
                _ => None,
            },
            _ => None,
        }
    }

    fn is_pointer_like_type(&self, ty: &Type) -> bool {
        matches!(
            ty,
            Type::Ref { .. } | Type::Ptr { .. } | Type::Array(_, _, _) | Type::Slice(_, _)
        )
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
        subtract: bool,
    ) -> Expr {
        let span = Span::default();
        let offset = if subtract {
            Expr::Binary {
                left: Box::new(Expr::Int(0, span)),
                op: BinaryOp::Sub,
                right: Box::new(offset),
                span,
            }
        } else {
            offset
        };

        let element_ty = match self.infer_expr_type(&base) {
            Some(Type::Ptr { inner, .. }) | Some(Type::Ref { inner, .. }) => Some(*inner),
            Some(Type::Array(inner, _, _)) | Some(Type::Slice(inner, _)) => Some(*inner),
            _ => None,
        };

        Expr::PtrOffset {
            pointer: Box::new(base),
            offset: Box::new(offset),
            element_ty,
            span,
        }
    }

    fn lower_memory_load(&self, pointer: Expr) -> Expr {
        let load_ty = match self.infer_expr_type(&pointer) {
            Some(Type::Ref { inner, .. }) | Some(Type::Ptr { inner, .. }) => Some(*inner),
            _ => None,
        };
        Expr::MemLoad {
            pointer: Box::new(pointer),
            load_ty,
            span: Span::default(),
        }
    }

    fn lower_store_or_assign(&self, target: Expr, value: Expr) -> Result<Expr> {
        match target {
            Expr::MemLoad {
                pointer,
                load_ty,
                ..
            } => Ok(Expr::MemStore {
                pointer,
                value: Box::new(value),
                store_ty: load_ty,
                span: Span::default(),
            }),
            other => Ok(Expr::Assign {
                target: Box::new(other),
                value: Box::new(value),
                span: Span::default(),
            }),
        }
    }

    fn maybe_lower_pointer_arithmetic(
        &self,
        operator: &c_ast::BinaryOperator,
        left: Expr,
        right: Expr,
    ) -> Result<Option<Expr>> {
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
            BinOp::Plus if left_is_pointer && self.is_integer_like_expr(&right) => {
                Ok(Some(self.lower_pointer_offset(left, right, false)))
            }
            BinOp::Plus if right_is_pointer && self.is_integer_like_expr(&left) => {
                Ok(Some(self.lower_pointer_offset(right, left, false)))
            }
            BinOp::Minus if left_is_pointer && self.is_integer_like_expr(&right) => {
                Ok(Some(self.lower_pointer_offset(left, right, true)))
            }
            BinOp::Minus if left_is_pointer && right_is_pointer => self.memory_lowering_required(
                "pointer difference requires an explicit raw-memory lowering strategy",
            ),
            BinOp::Plus if left_is_pointer || right_is_pointer => self.memory_lowering_required(
                "pointer arithmetic currently supports only pointer +/- integer forms",
            ),
            BinOp::Minus if left_is_pointer || right_is_pointer => self.memory_lowering_required(
                "pointer arithmetic currently supports only pointer - integer forms",
            ),
            _ => Ok(None),
        }
    }

    fn lower_address_of_expression(&mut self, operand: &c_ast::Expression) -> Result<Expr> {
        use c_ast::BinaryOperator as BinOp;

        match operand {
            c_ast::Expression::BinaryOperator(bin_op)
                if matches!(bin_op.node.operator.node, BinOp::Index) =>
            {
                let object_expr = self.transform_expression(&bin_op.node.lhs.node)?;
                let idx_expr = self.transform_expression(&bin_op.node.rhs.node)?;
                if self
                    .infer_expr_type(&object_expr)
                    .as_ref()
                    .is_some_and(|ty| matches!(ty, Type::Ptr { .. } | Type::Ref { .. }))
                {
                    Ok(self.lower_pointer_offset(object_expr, idx_expr, false))
                } else {
                    let pointee_ty = self.infer_expr_type(&Expr::Index {
                        object: Box::new(object_expr.clone()),
                        index: Box::new(idx_expr.clone()),
                        span: Span::default(),
                    });
                    Ok(Expr::AddrOf {
                        value: Box::new(Expr::Index {
                            object: Box::new(object_expr),
                            index: Box::new(idx_expr),
                            span: Span::default(),
                        }),
                        pointee_ty,
                        span: Span::default(),
                    })
                }
            }
            _ => {
                let value = self.transform_expression(operand)?;
                if self.expr_field_is_bitfield(&value) {
                    let code = kain_core::diagnostic_registry::spec_for_code(
                        DiagnosticCode::MemoryIllegalBitfieldAddress,
                    )
                    .code_str;
                    return Err(ImportError::UnsupportedFeature(format!(
                        "{}: cannot take the address of a C bitfield; lower it through bitfield load/store semantics instead",
                        code
                    )));
                }
                let pointee_ty = self.infer_expr_type(&value);
                Ok(Expr::AddrOf {
                    value: Box::new(value),
                    pointee_ty,
                    span: Span::default(),
                })
            }
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
                        let object_expr = self.transform_expression(&bin_op.node.lhs.node)?;
                        let idx_expr = self.transform_expression(&bin_op.node.rhs.node)?;

                        if self
                            .infer_expr_type(&object_expr)
                            .as_ref()
                            .is_some_and(|ty| matches!(ty, Type::Ptr { .. } | Type::Ref { .. }))
                        {
                            Ok(self.lower_memory_load(self.lower_pointer_offset(
                                object_expr,
                                idx_expr,
                                false,
                            )))
                        } else {
                            Ok(Expr::Index {
                                object: Box::new(object_expr),
                                index: Box::new(idx_expr),
                                span: Span::default(),
                            })
                        }
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
                        )? {
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
                        self.lower_address_of_expression(&unary_op.node.operand.node)
                    }
                    c_ast::UnaryOperator::Indirection => {
                        let value =
                            Box::new(self.transform_expression(&unary_op.node.operand.node)?);
                        Ok(Expr::MemLoad {
                            pointer: value,
                            load_ty: None,
                            span: Span::default(),
                        })
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
                Ok(Expr::SizeOfType {
                    target: target_ty,
                    span: Span::default(),
                })
            }

            // sizeof(expr)
            SizeOfVal(size_of_val) => {
                let lowered = self.transform_expression(&size_of_val.node.0.node)?;
                if let Some(ty) = self.infer_expr_type(&lowered) {
                    Ok(Expr::SizeOfType {
                        target: ty,
                        span: Span::default(),
                    })
                } else {
                    Ok(Expr::Int(
                        self.estimate_sizeof_expression(&size_of_val.node.0.node),
                        Span::default(),
                    ))
                }
            }

            // alignof(type)
            AlignOf(align_of_ty) => {
                let target_ty = self.extract_type_from_type_name(&align_of_ty.node.0.node)?;
                Ok(Expr::AlignOfType {
                    target: target_ty,
                    span: Span::default(),
                })
            }
            
            // Function call
            Call(call) => {
                if let Some(lowered) = self.try_lower_allocator_call(&call.node)? {
                    Ok(lowered)
                } else {
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
                
                self.transform_compound_literal_for_type(&type_name, &compound.node.initializer_list)
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
    
}

impl Default for CTransformer {
    fn default() -> Self {
        Self::new()
    }
}

/// Transform a C translation unit to KAIN program
#[cfg(test)]
pub fn transform(tu: c_ast::TranslationUnit) -> Result<Program> {
    let mut transformer = CTransformer::new();
    transformer.transform(tu)
}

#[cfg(test)]
pub fn transform_with_layout_metadata(
    tu: c_ast::TranslationUnit,
    layout_metadata: CSourceLayoutMetadata,
) -> Result<Program> {
    let mut transformer = CTransformer::with_language_capabilities_and_layout_metadata(
        default_language_capabilities(),
        layout_metadata,
    );
    transformer.transform(tu)
}

#[cfg(test)]
pub fn transform_with_language_capabilities(
    tu: c_ast::TranslationUnit,
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    let mut transformer = CTransformer::with_language_capabilities(language_capabilities);
    transformer.transform(tu)
}

pub fn transform_with_language_capabilities_and_layout_metadata(
    tu: c_ast::TranslationUnit,
    language_capabilities: LanguageCapabilities,
    layout_metadata: CSourceLayoutMetadata,
) -> Result<Program> {
    let mut transformer = CTransformer::with_language_capabilities_and_layout_metadata(
        language_capabilities,
        layout_metadata,
    );
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
    use crate::c::parser::{parse_c_source, parse_c_source_with_metadata};
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
    fn test_transform_lowers_pointer_arithmetic_to_ptr_offset() {
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
        let Stmt::Return(Some(Expr::PtrOffset { pointer, offset, .. }), _) = &func.body.stmts[0] else {
            panic!("expected pointer arithmetic to lower into ptr_offset form");
        };
        assert!(matches!(pointer.as_ref(), Expr::Ident(name, _) if name == "s"));
        assert!(matches!(offset.as_ref(), Expr::Ident(name, _) if name == "i"));
    }

    #[test]
    fn test_transform_rejects_pointer_difference_without_lowering_policy() {
        let source = r#"
            int delta(char *a, char *b) {
                return a - b;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let err = transform(c_ast).expect_err("expected pointer difference to be rejected");
        let rendered = err.to_string();

        assert!(rendered.contains("KAIN-MEM-0001"));
        assert!(rendered.contains("pointer difference"));
    }

    #[test]
    fn test_transform_lowers_deref_and_pointer_subscript_to_memory_ops() {
        let source = r#"
            int read_pair(int *ptr, int i) {
                *ptr = ptr[i];
                return *ptr;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };

        let Stmt::Expr(Expr::MemStore { pointer, value, .. }) = &func.body.stmts[0] else {
            panic!("expected memory store statement");
        };
        assert!(matches!(pointer.as_ref(), Expr::Ident(name, _) if name == "ptr"));
        let Expr::MemLoad { pointer: load_pointer, .. } = value.as_ref() else {
            panic!("expected memory load on store rhs");
        };
        let Expr::PtrOffset { pointer: base, offset, .. } = load_pointer.as_ref() else {
            panic!("expected pointer offset for subscript");
        };
        assert!(matches!(base.as_ref(), Expr::Ident(name, _) if name == "ptr"));
        assert!(matches!(offset.as_ref(), Expr::Ident(name, _) if name == "i"));

        let Stmt::Return(Some(Expr::MemLoad { pointer, .. }), _) = &func.body.stmts[1] else {
            panic!("expected memory load return");
        };
        assert!(matches!(pointer.as_ref(), Expr::Ident(name, _) if name == "ptr"));
    }

    #[test]
    fn test_transform_lowers_address_taken_subscript_to_typed_ptr_offset() {
        let source = r#"
            int *slot(int *ptr, int i) {
                return &ptr[i];
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };
        let Stmt::Return(Some(Expr::PtrOffset { pointer, offset, element_ty, .. }), _) = &func.body.stmts[0] else {
            panic!("expected address-of subscript to lower into ptr_offset");
        };
        assert!(matches!(pointer.as_ref(), Expr::Ident(name, _) if name == "ptr"));
        assert!(matches!(offset.as_ref(), Expr::Ident(name, _) if name == "i"));
        assert!(matches!(element_ty, Some(Type::Named { name, .. }) if name == "Int"));
    }

    #[test]
    fn test_transform_lowers_address_of_local_to_addr_of_expr() {
        let source = r#"
            int *slot(int value) {
                return &value;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function item");
        };
        let Stmt::Return(Some(Expr::AddrOf { value, pointee_ty, .. }), _) = &func.body.stmts[0] else {
            panic!("expected address-of local to lower into addr_of");
        };
        assert!(matches!(value.as_ref(), Expr::Ident(name, _) if name == "value"));
        assert!(matches!(pointee_ty, Some(Type::Named { name, .. }) if name == "Int"));
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

        let Stmt::Return(Some(Expr::SizeOfType { target, .. }), _) = &alloc_fn.body.stmts[0] else {
            panic!("expected sizeof_type return");
        };
        assert!(matches!(target, Type::Named { name, .. } if name == "KainArray"));
    }

    #[test]
    fn test_named_struct_field_with_anonymous_nested_struct_emits_real_type() {
        let source = r#"
            struct du {
                float d;
                struct {
                    int hi;
                    int lo;
                } word;
            };
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();

        assert!(program.items.iter().any(|item| matches!(
            item,
            Item::Struct(s) if s.name == "du_word_AnonStruct"
        )));

        let du_struct = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Struct(s) if s.name == "du" => Some(s),
                _ => None,
            })
            .expect("expected du struct");

        let word_field = du_struct
            .fields
            .iter()
            .find(|field| field.name == "word")
            .expect("expected word field");

        assert!(matches!(
            &word_field.ty,
            Type::Named { name, .. } if name == "du_word_AnonStruct"
        ));
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

        let Stmt::Let { ty: Some(Type::Array(_, count, _)), value: Some(Expr::Alloca { .. }), .. } =
            &run_fn.body.stmts[0]
        else {
            panic!("expected fixed array local");
        };
        assert_eq!(*count, 2);
    }

    #[test]
    fn test_uninitialized_scalar_local_uses_uninit_storage() {
        let source = r#"
            int run() {
                int value;
                return 0;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let run_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Let {
            ty: Some(Type::Named { name, .. }),
            value: Some(Expr::Uninit { ty, .. }),
            ..
        } = &run_fn.body.stmts[0]
        else {
            panic!("expected uninit scalar local");
        };
        assert_eq!(name, "Int");
        assert!(matches!(ty, Type::Named { name, .. } if name == "Int"));
    }

    #[test]
    fn test_alignof_type_preserves_layout_query() {
        let source = r#"
            int align() {
                return __alignof__(int);
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let align_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Return(Some(Expr::AlignOfType { target, .. }), _) = &align_fn.body.stmts[0] else {
            panic!("expected alignof_type return");
        };
        assert!(matches!(target, Type::Named { name, .. } if name == "Int"));
    }

    #[test]
    fn test_sizeof_expr_prefers_inferred_type() {
        let source = r#"
            int size_of_local() {
                int value = 7;
                return sizeof(value);
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let size_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Return(Some(Expr::SizeOfType { target, .. }), _) = &size_fn.body.stmts[1] else {
            panic!("expected sizeof_type on inferred local");
        };
        assert!(matches!(target, Type::Named { name, .. } if name == "Int"));
    }

    #[test]
    fn test_sizeof_expr_handles_aggregate_literal_type() {
        let source = r#"
            struct Pair {
                int left;
                int right;
            };

            int size_pair() {
                return sizeof((struct Pair){ .left = 1, .right = 2 });
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let size_fn = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "size_pair" => Some(f),
                _ => None,
            })
            .expect("expected size_pair function");

        let Stmt::Return(Some(Expr::SizeOfType { target, .. }), _) = &size_fn.body.stmts[0] else {
            panic!("expected sizeof_type on aggregate literal");
        };
        assert!(matches!(target, Type::Named { name, .. } if name == "Pair"));
    }

    #[test]
    fn test_sizeof_expr_handles_array_lvalue_type() {
        let source = r#"
            int size_values() {
                int values[4] = { 1, 2, 3, 4 };
                return sizeof(values);
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let size_fn = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Return(Some(Expr::SizeOfType { target, .. }), _) = &size_fn.body.stmts[1] else {
            panic!("expected sizeof_type on array local");
        };
        assert!(matches!(target, Type::Array(_, 4, _)));
    }

    #[test]
    fn test_calloc_preserves_element_count_semantics() {
        let source = r#"
            int *make_buf(int count) {
                return calloc(count, sizeof(int));
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let make_buf = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Return(Some(Expr::Alloc { size, ty, zeroed, .. }), _) = &make_buf.body.stmts[0] else {
            panic!("expected alloc return");
        };
        assert!(*zeroed, "calloc should preserve zeroed heap semantics");
        assert!(matches!(ty, Some(Type::Named { name, .. }) if name == "Int"));
        let Expr::Binary { left, op: BinaryOp::Mul, right, .. } = &**size else {
            panic!("expected calloc size to remain count * sizeof(type)");
        };
        assert!(matches!(&**left, Expr::Ident(name, _) if name == "count"));
        assert!(matches!(&**right, Expr::SizeOfType { target: Type::Named { name, .. }, .. } if name == "Int"));
    }

    #[test]
    fn test_designated_struct_initializer_becomes_explicit_aggregate_init() {
        let source = r#"
            struct Pair {
                int left;
                int right;
            };

            struct Pair make_pair() {
                return (struct Pair){ .right = 2, .left = 1 };
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let make_pair = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "make_pair" => Some(f),
                _ => None,
            })
            .expect("expected make_pair function");

        let Stmt::Return(Some(Expr::AggregateInit { ty, fields, zero_fill_rest, .. }), _) =
            &make_pair.body.stmts[0]
        else {
            panic!("expected aggregate_init return");
        };
        assert!(*zero_fill_rest);
        assert!(matches!(ty, Type::Named { name, .. } if name == "Pair"));
        assert_eq!(fields.len(), 2);
        assert_eq!(fields[0].0, "right");
        assert_eq!(fields[1].0, "left");
    }

    #[test]
    fn test_nested_designated_struct_initializer_becomes_nested_aggregate_init() {
        let source = r#"
            struct Inner {
                int x;
                int y;
            };

            struct Outer {
                struct Inner inner;
                int z;
            };

            struct Outer make_outer() {
                return (struct Outer){ .inner.x = 1, .inner.y = 2, .z = 3 };
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let make_outer = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "make_outer" => Some(f),
                _ => None,
            })
            .expect("expected make_outer");

        let Stmt::Return(Some(Expr::AggregateInit { fields, .. }), _) = &make_outer.body.stmts[0] else {
            panic!("expected aggregate_init return");
        };

        let inner = fields.iter().find(|(name, _)| name == "inner").expect("missing inner field");
        let Expr::AggregateInit { fields: inner_fields, .. } = &inner.1 else {
            panic!("expected nested aggregate init for inner");
        };
        assert!(inner_fields.iter().any(|(name, _)| name == "x"));
        assert!(inner_fields.iter().any(|(name, _)| name == "y"));
        assert!(fields.iter().any(|(name, _)| name == "z"));
    }

    #[test]
    fn test_designated_array_initializer_becomes_explicit_sparse_array() {
        let source = r#"
            int read_value() {
                int values[4] = { [2] = 7, [0] = 1 };
                return values[2];
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let read_value = match &program.items[0] {
            Item::Function(f) => f,
            _ => panic!("expected function"),
        };

        let Stmt::Let { value: Some(Expr::Array(items, _)), .. } = &read_value.body.stmts[0] else {
            panic!("expected explicit array initializer");
        };
        assert_eq!(items.len(), 4);
        assert!(matches!(&items[0], Expr::Int(1, _)));
        assert!(matches!(&items[1], Expr::Int(0, _)));
        assert!(matches!(&items[2], Expr::Int(7, _)));
    }

    #[test]
    fn test_nested_designated_array_of_struct_initializer_preserves_field_updates() {
        let source = r#"
            struct Pair {
                int left;
                int right;
            };

            int read_value() {
                struct Pair values[3] = { [2].right = 7, [2].left = 4 };
                return values[2].right;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let read_value = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "read_value" => Some(f),
                _ => None,
            })
            .expect("expected read_value");

        let Stmt::Let { value: Some(Expr::Array(items, _)), .. } = &read_value.body.stmts[0] else {
            panic!("expected explicit array initializer");
        };
        let Expr::AggregateInit { fields, .. } = &items[2] else {
            panic!("expected aggregate init at designated array slot");
        };
        assert!(fields.iter().any(|(name, _)| name == "left"));
        assert!(fields.iter().any(|(name, _)| name == "right"));
    }

    #[test]
    fn test_union_declaration_carries_layout_metadata() {
        let source = r#"
            union Number {
                int as_int;
                float as_float;
            };
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let union_decl = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct item for imported union"),
        };

        assert!(union_decl
            .attributes
            .iter()
            .any(|attr| attr.name == C_UNION_ATTR));
    }

    #[test]
    fn test_bitfield_declaration_carries_width_metadata() {
        let source = r#"
            struct Flags {
                unsigned int ready: 1;
                unsigned int mode: 3;
                unsigned int value;
            };
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let flags = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct"),
        };

        let ready = flags.fields.iter().find(|field| field.name == "ready").expect("ready field");
        let mode = flags.fields.iter().find(|field| field.name == "mode").expect("mode field");
        let value = flags.fields.iter().find(|field| field.name == "value").expect("value field");

        assert!(ready
            .attributes
            .iter()
            .any(|attr| attr.name == C_BITFIELD_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(1, _)))
                && matches!(attr.args.get(1), Some(Expr::Bool(false, _)))));
        assert!(mode
            .attributes
            .iter()
            .any(|attr| attr.name == C_BITFIELD_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(3, _)))
                && matches!(attr.args.get(1), Some(Expr::Bool(false, _)))));
        assert!(value.attributes.iter().all(|attr| attr.name != C_BITFIELD_ATTR));
    }

    #[test]
    fn test_signed_bitfield_declaration_carries_signedness_metadata() {
        let source = r#"
            struct Flags {
                signed int delta: 5;
                int implicit_signed: 2;
            };
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let flags = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct"),
        };

        let delta = flags.fields.iter().find(|field| field.name == "delta").expect("delta field");
        let implicit_signed = flags.fields.iter().find(|field| field.name == "implicit_signed").expect("implicit_signed field");

        assert!(delta
            .attributes
            .iter()
            .any(|attr| attr.name == C_BITFIELD_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(5, _)))
                && matches!(attr.args.get(1), Some(Expr::Bool(true, _)))));
        assert!(implicit_signed
            .attributes
            .iter()
            .any(|attr| attr.name == C_BITFIELD_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(2, _)))
                && matches!(attr.args.get(1), Some(Expr::Bool(true, _)))));
    }

    #[test]
    fn test_c_field_storage_metadata_carries_abi_bits_and_alignment() {
        let source = r#"
            struct LayoutProbe {
                unsigned int ready: 1;
                char marker;
                double weight;
            };
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let probe = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct"),
        };

        let ready = probe.fields.iter().find(|field| field.name == "ready").expect("ready");
        let marker = probe.fields.iter().find(|field| field.name == "marker").expect("marker");
        let weight = probe.fields.iter().find(|field| field.name == "weight").expect("weight");

        assert!(ready.attributes.iter().any(|attr| {
            attr.name == C_STORAGE_BITS_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(32, _)))
        }));
        assert!(marker.attributes.iter().any(|attr| {
            attr.name == C_STORAGE_ALIGN_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(8, _)))
        }));
        assert!(weight.attributes.iter().any(|attr| {
            attr.name == C_STORAGE_BITS_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(64, _)))
        }));
    }

    #[test]
    fn test_union_designated_initializer_keeps_only_active_field() {
        let source = r#"
            union Number {
                int as_int;
                float as_float;
            };

            union Number make_number() {
                return (union Number){ .as_int = 7, .as_float = 3 };
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let make_number = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "make_number" => Some(f),
                _ => None,
            })
            .expect("expected make_number");

        let Stmt::Return(Some(Expr::AggregateInit { fields, .. }), _) = &make_number.body.stmts[0] else {
            panic!("expected aggregate_init return");
        };
        assert_eq!(fields.len(), 1);
        assert_eq!(fields[0].0, "as_float");
    }

    #[test]
    fn test_nested_union_designator_preserves_only_selected_member() {
        let source = r#"
            union Number {
                int as_int;
                float as_float;
            };

            struct Wrapper {
                union Number number;
                int tag;
            };

            struct Wrapper make_wrapper() {
                return (struct Wrapper){ .number.as_int = 7, .number.as_float = 3, .tag = 1 };
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let program = transform(c_ast).unwrap();
        let make_wrapper = program
            .items
            .iter()
            .find_map(|item| match item {
                Item::Function(f) if f.name == "make_wrapper" => Some(f),
                _ => None,
            })
            .expect("expected make_wrapper");

        let Stmt::Return(Some(Expr::AggregateInit { fields, .. }), _) = &make_wrapper.body.stmts[0] else {
            panic!("expected aggregate_init return");
        };
        let number = fields.iter().find(|(name, _)| name == "number").expect("missing nested union");
        let Expr::AggregateInit { fields: number_fields, .. } = &number.1 else {
            panic!("expected nested aggregate_init for union");
        };
        assert_eq!(number_fields.len(), 1);
        assert_eq!(number_fields[0].0, "as_float");
    }

    #[test]
    fn test_address_of_bitfield_reports_dedicated_memory_diagnostic() {
        let source = r#"
            struct Flags {
                unsigned int ready: 1;
            };

            int *take_ready(struct Flags f) {
                return &f.ready;
            }
        "#;

        let c_ast = parse_c_source(source).unwrap();
        let err = transform(c_ast).expect_err("bitfield address-of should be rejected");
        let rendered = err.to_string();
        assert!(rendered.contains("KAIN-MEM-0003"));
        assert!(rendered.contains("bitfield"));
    }

    #[test]
    fn test_pragma_pack_attaches_struct_layout_metadata() {
        let source = r#"
            #pragma pack(push, 1)
            struct Packet {
                char tag;
                int value;
            };
            #pragma pack(pop)
        "#;

        let parsed = parse_c_source_with_metadata(source).unwrap();
        let program = transform_with_layout_metadata(parsed.unit, parsed.layout).unwrap();
        let packet = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct item"),
        };

        assert!(packet.attributes.iter().any(|attr| attr.name == C_PACKED_ATTR));
        assert!(packet.attributes.iter().any(|attr| {
            attr.name == C_PACK_ALIGN_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(bits, _)) if *bits == 8)
        }));
    }

    #[test]
    fn test_explicit_aligned_attribute_attaches_type_alignment_metadata() {
        let source = r#"
            struct __attribute__((aligned(16))) Packet {
                char tag;
                int value;
            };
        "#;

        let parsed = parse_c_source_with_metadata(source).unwrap();
        let program = transform_with_layout_metadata(parsed.unit, parsed.layout).unwrap();
        let packet = match &program.items[0] {
            Item::Struct(st) => st,
            _ => panic!("expected struct item"),
        };

        assert!(packet.attributes.iter().any(|attr| {
            attr.name == C_TYPE_ALIGN_ATTR
                && matches!(attr.args.first(), Some(Expr::Int(bits, _)) if *bits == 128)
        }));
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

        let Stmt::Expr(Expr::MemStore { pointer, .. }) = &push_fn.body.stmts[0] else {
            panic!("expected memory store expression");
        };
        let Expr::PtrOffset { offset, .. } = &**pointer else {
            panic!("expected pointer offset store target");
        };
        let Expr::Match { .. } = &**offset else {
            panic!("expected sequenced match-based postfix lowering");
        };
    }
}
