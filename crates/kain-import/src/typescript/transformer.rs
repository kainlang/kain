//! TypeScript AST -> KAIN AST transformation.
//!
//! The implementation intentionally follows the stateful shape used by the Rust
//! importer instead of the earlier stateless scaffold. That gives the TypeScript
//! path stable identifier renaming, local type scopes, and accumulated
//! diagnostics for unsupported constructs.

use std::collections::HashMap;

use kain_core::ast::*;
use kain_core::effects::Effect;
use kain_core::span::Span;
use swc_ecma_ast as ts;

use crate::common::identifier_registry::{IdentifierDomain, StableIdentifierRenamer};
use crate::common::ImportContext;
use crate::{ImportError, Result};

use super::types::TypeMapper;

#[derive(Debug, Clone, Copy)]
enum JsxFallbackMode {
    PlaceholderString,
}

#[derive(Debug, Clone, Copy)]
enum SpreadFallbackMode {
    KeepExplicitFields,
}

#[derive(Debug, Clone, Copy)]
struct LoweringPolicy {
    jsx_fallback: JsxFallbackMode,
    object_spread: SpreadFallbackMode,
    call_spread: SpreadFallbackMode,
    jsx_placeholder_prefix: &'static str,
}

impl Default for LoweringPolicy {
    fn default() -> Self {
        Self {
            jsx_fallback: JsxFallbackMode::PlaceholderString,
            object_spread: SpreadFallbackMode::KeepExplicitFields,
            call_spread: SpreadFallbackMode::KeepExplicitFields,
            jsx_placeholder_prefix: "__jsx__",
        }
    }
}

pub struct TypeScriptTransformer {
    type_mapper: TypeMapper,
    identifier_renamer: StableIdentifierRenamer,
    policy: LoweringPolicy,
    _context: ImportContext,
    local_types: Vec<HashMap<String, Type>>,
    current_class: Option<String>,
    temp_counter: usize,
    pub diagnostics: Vec<String>,
}

impl TypeScriptTransformer {
    pub fn new() -> Self {
        Self {
            type_mapper: TypeMapper::new(),
            identifier_renamer: StableIdentifierRenamer::default(),
            policy: LoweringPolicy::default(),
            _context: ImportContext::default(),
            local_types: vec![HashMap::new()],
            current_class: None,
            temp_counter: 0,
            diagnostics: Vec::new(),
        }
    }

    pub fn transform(&mut self, module: ts::Module) -> Result<Program> {
        let mut items = Vec::new();
        for item in module.body {
            items.extend(self.transform_module_item(item)?);
        }
        Ok(Program {
            items,
            span: Span::default(),
        })
    }

    fn transform_module_item(&mut self, item: ts::ModuleItem) -> Result<Vec<Item>> {
        match item {
            ts::ModuleItem::ModuleDecl(decl) => self.transform_module_decl(decl),
            ts::ModuleItem::Stmt(stmt) => self.transform_top_level_stmt(stmt),
        }
    }

    fn transform_module_decl(&mut self, decl: ts::ModuleDecl) -> Result<Vec<Item>> {
        match decl {
            ts::ModuleDecl::ExportDecl(export) => self.transform_decl(export.decl),
            ts::ModuleDecl::Import(import) => {
                self.note(format!(
                    "import from '{}' skipped (module resolution is not implemented yet)",
                    import.src.value
                ));
                Ok(Vec::new())
            }
            ts::ModuleDecl::ExportDefaultDecl(export) => self.transform_export_default_decl(export),
            ts::ModuleDecl::ExportDefaultExpr(_) => {
                self.note("default export expression skipped".to_string());
                Ok(Vec::new())
            }
            ts::ModuleDecl::ExportNamed(_) => {
                self.note("named export list skipped".to_string());
                Ok(Vec::new())
            }
            other => {
                self.note(format!("module declaration {:?} skipped", other));
                Ok(Vec::new())
            }
        }
    }

    fn transform_export_default_decl(&mut self, export: ts::ExportDefaultDecl) -> Result<Vec<Item>> {
        match export.decl {
            ts::DefaultDecl::Fn(func) => {
                let name = func
                    .ident
                    .as_ref()
                    .map(|ident| self.rename_value(&ident.sym))
                    .unwrap_or_else(|| "default_export".to_string());
                self.transform_top_level_function(
                    name,
                    &func.function,
                    Visibility::Public,
                    func.function
                        .type_params
                        .as_deref()
                        .map(Self::map_generics)
                        .unwrap_or_default(),
                )
            }
            ts::DefaultDecl::Class(class) => {
                let ident = class.ident.unwrap_or_else(|| ts::Ident::new_no_ctxt("DefaultExport".into(), class.class.span));
                self.transform_class_decl(ts::ClassDecl {
                    ident,
                    declare: false,
                    class: class.class,
                })
            }
            ts::DefaultDecl::TsInterfaceDecl(interface) => {
                Ok(vec![Item::Struct(self.transform_interface(*interface)?)])
            }
        }
    }

    fn transform_top_level_stmt(&mut self, stmt: ts::Stmt) -> Result<Vec<Item>> {
        match stmt {
            ts::Stmt::Decl(decl) => self.transform_decl(decl),
            other => {
                self.note(format!("top-level statement {:?} skipped", other));
                Ok(Vec::new())
            }
        }
    }

    fn transform_decl(&mut self, decl: ts::Decl) -> Result<Vec<Item>> {
        match decl {
            ts::Decl::Fn(func) => self.transform_fn_decl(func),
            ts::Decl::TsInterface(interface) => {
                Ok(vec![Item::Struct(self.transform_interface(*interface)?)])
            }
            ts::Decl::TsEnum(ts_enum) => Ok(vec![Item::Enum(self.transform_enum(*ts_enum)?)]),
            ts::Decl::TsTypeAlias(alias) => {
                Ok(vec![Item::TypeAlias(self.transform_type_alias(*alias)?)])
            }
            ts::Decl::Class(class) => self.transform_class_decl(class),
            ts::Decl::Var(var) => self.transform_top_level_var_decl(*var),
            other => {
                self.note(format!("declaration {:?} skipped", other));
                Ok(Vec::new())
            }
        }
    }

    fn transform_fn_decl(&mut self, func: ts::FnDecl) -> Result<Vec<Item>> {
        let name = self.rename_value(&func.ident.sym);
        self.transform_top_level_function(
            name,
            &func.function,
            Visibility::Public,
            func.function
                .type_params
                .as_deref()
                .map(Self::map_generics)
                .unwrap_or_default(),
        )
    }

    fn transform_top_level_function(
        &mut self,
        name: String,
        function: &ts::Function,
        visibility: Visibility,
        generics: Vec<Generic>,
    ) -> Result<Vec<Item>> {
        if let Some(component) =
            self.try_transform_component(&name, function, visibility, generics.clone())?
        {
            return Ok(vec![Item::Component(component)]);
        }

        Ok(vec![Item::Function(self.transform_function_like(
            name,
            function,
            visibility,
            false,
            generics,
        )?)])
    }

    fn transform_interface(&mut self, interface: ts::TsInterfaceDecl) -> Result<Struct> {
        let span = Span::default();
        let name = self.rename_type(&interface.id.sym);
        let generics = interface
            .type_params
            .as_deref()
            .map(Self::map_generics)
            .unwrap_or_default();
        let mut fields = Vec::new();

        if !interface.extends.is_empty() {
            self.note(format!(
                "interface {} extends clauses are noted but not modeled directly",
                name
            ));
        }

        for member in interface.body.body {
            match member {
                ts::TsTypeElement::TsPropertySignature(prop) => {
                    if let Some(field_name) = self.expr_to_field_name(&prop.key) {
                        let mut field_type = prop
                            .type_ann
                            .as_ref()
                            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
                            .transpose()?
                            .unwrap_or(Type::Infer(span));

                        if prop.optional {
                            field_type = Type::Option(Box::new(field_type), span);
                        }

                        let mut attributes = Vec::new();
                        if prop.readonly {
                            attributes.push(Attribute {
                                name: "readonly".to_string(),
                                args: Vec::new(),
                                span,
                            });
                        }

                        fields.push(Field {
                            name: self.rename_field(&field_name),
                            ty: field_type,
                            visibility: Visibility::Public,
                            attributes,
                            default: None,
                            weak: false,
                            span,
                        });
                    } else {
                        self.note("computed interface property skipped".to_string());
                    }
                }
                ts::TsTypeElement::TsMethodSignature(method) => {
                    if let Some(field_name) = self.expr_to_field_name(&method.key) {
                        let params = self.map_ts_fn_params(&method.params)?;
                        let return_type = method
                            .type_ann
                            .as_ref()
                            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
                            .transpose()?
                            .unwrap_or(Type::Unit(span));
                        let field_type = Type::Function {
                            params: params.iter().map(|p| p.ty.clone()).collect(),
                            return_type: Box::new(return_type),
                            effects: Vec::new(),
                            span,
                        };
                        fields.push(Field {
                            name: self.rename_field(&field_name),
                            ty: field_type,
                            visibility: Visibility::Public,
                            attributes: Vec::new(),
                            default: None,
                            weak: false,
                            span,
                        });
                    } else {
                        self.note("computed interface method skipped".to_string());
                    }
                }
                other => self.note(format!("interface member {:?} skipped", other)),
            }
        }

        Ok(Struct {
            name,
            fields,
            visibility: Visibility::Public,
            generics,
            methods: Vec::new(),
            attributes: Vec::new(),
            span,
        })
    }

    fn transform_enum(&mut self, ts_enum: ts::TsEnumDecl) -> Result<Enum> {
        let span = Span::default();
        let name = self.rename_type(&ts_enum.id.sym);
        let variants = ts_enum
            .members
            .into_iter()
            .map(|member| Variant {
                name: self.rename_variant(member.id.as_ref()),
                fields: VariantFields::Unit,
                span,
            })
            .collect();

        Ok(Enum {
            name,
            variants,
            visibility: Visibility::Public,
            generics: Vec::new(),
            span,
        })
    }

    fn transform_type_alias(&mut self, alias: ts::TsTypeAliasDecl) -> Result<TypeAlias> {
        let span = Span::default();
        Ok(TypeAlias {
            name: self.rename_type(&alias.id.sym),
            generics: alias
                .type_params
                .as_deref()
                .map(Self::map_generics)
                .unwrap_or_default(),
            target: self.type_mapper.map_type(&alias.type_ann, span)?,
            visibility: Visibility::Public,
            span,
        })
    }

    fn transform_class_decl(&mut self, class: ts::ClassDecl) -> Result<Vec<Item>> {
        let span = Span::default();
        let class_name = self.rename_type(&class.ident.sym);
        let generics = class
            .class
            .type_params
            .as_deref()
            .map(Self::map_generics)
            .unwrap_or_default();
        let mut fields = Vec::new();
        let mut methods = Vec::new();

        self.current_class = Some(class_name.clone());

        for member in class.class.body {
            match member {
                ts::ClassMember::ClassProp(prop) => {
                    if let Some(field_name) = self.prop_name_to_string(&prop.key) {
                        let ty = prop
                            .type_ann
                            .as_ref()
                            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
                            .transpose()?
                            .unwrap_or(Type::Infer(span));
                        fields.push(Field {
                            name: self.rename_field(&field_name),
                            ty,
                            visibility: accessibility_to_visibility(prop.accessibility),
                            attributes: Vec::new(),
                            default: prop
                                .value
                                .as_deref()
                                .map(|expr| self.transform_expr(expr))
                                .transpose()?,
                            weak: false,
                            span,
                        });
                    } else {
                        self.note("computed class property skipped".to_string());
                    }
                }
                ts::ClassMember::Method(method) => {
                    if let Some(name) = self.prop_name_to_string(&method.key) {
                        methods.push(self.transform_class_method(name, *method.function, method.is_static)?);
                    } else {
                        self.note("computed class method skipped".to_string());
                    }
                }
                ts::ClassMember::Constructor(ctor) => {
                    methods.push(self.transform_constructor(&class_name, ctor)?);
                }
                other => self.note(format!("class member {:?} skipped", other)),
            }
        }

        self.current_class = None;

        let struct_item = Item::Struct(Struct {
            name: class_name.clone(),
            fields,
            visibility: Visibility::Public,
            generics: generics.clone(),
            methods: Vec::new(),
            attributes: Vec::new(),
            span,
        });

        if methods.is_empty() {
            return Ok(vec![struct_item]);
        }

        let impl_item = Item::Impl(Impl {
            generics,
            trait_name: None,
            target_type: Type::Named {
                name: class_name,
                generics: Vec::new(),
                span,
            },
            methods,
            span,
        });

        Ok(vec![struct_item, impl_item])
    }

    fn transform_top_level_var_decl(&mut self, decl: ts::VarDecl) -> Result<Vec<Item>> {
        let span = Span::default();
        let mut items = Vec::new();

        for declarator in decl.decls {
            let Some(name) = self.pat_binding_name(&declarator.name) else {
                self.note("destructured top-level variable skipped".to_string());
                continue;
            };

            let ty = self
                .pat_type_annotation(&declarator.name)
                .map(|ann| self.type_mapper.map_type(ann, span))
                .transpose()?
                .unwrap_or(Type::Infer(span));

            let value = declarator
                .init
                .as_deref()
                .map(|expr| self.transform_expr(expr))
                .transpose()?;

            match decl.kind {
                ts::VarDeclKind::Const => {
                    if let Some(value) = value {
                        items.push(Item::Const(Const {
                            name: self.rename_value(&name),
                            ty,
                            value,
                            visibility: Visibility::Private,
                            span,
                        }));
                    } else {
                        self.note(format!("const {} skipped because it has no initializer", name));
                    }
                }
                _ => {
                    self.note(format!(
                        "top-level {:?} declaration '{}' skipped (KAIN has no top-level mutable binding item)",
                        decl.kind, name
                    ));
                }
            }
        }

        Ok(items)
    }

    fn transform_class_method(
        &mut self,
        name: String,
        function: ts::Function,
        is_static: bool,
    ) -> Result<Function> {
        let method_name = self.rename_value(&name);
        let mut func = self.transform_function_like(
            method_name,
            &function,
            Visibility::Public,
            !is_static,
            function
                .type_params
                .as_deref()
                .map(Self::map_generics)
                .unwrap_or_default(),
        )?;
        if is_static {
            func.attributes.push(Attribute {
                name: "static".to_string(),
                args: Vec::new(),
                span: Span::default(),
            });
        }
        Ok(func)
    }

    fn transform_constructor(&mut self, class_name: &str, ctor: ts::Constructor) -> Result<Function> {
        let span = Span::default();
        let mut params = Vec::new();

        for param in ctor.params {
            match param {
                ts::ParamOrTsParamProp::Param(param) => {
                    params.extend(self.map_params(&[param])?);
                }
                ts::ParamOrTsParamProp::TsParamProp(prop) => match prop.param {
                    ts::TsParamPropParam::Ident(ident) => {
                        let name = self.rename_value(&ident.id.sym);
                        let ty = ident
                            .type_ann
                            .as_ref()
                            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
                            .transpose()?
                            .unwrap_or(Type::Infer(span));
                        params.push(Param {
                            name,
                            ty,
                            mutable: false,
                            default: None,
                            span,
                        });
                    }
                    _ => self.note("unsupported constructor parameter property skipped".to_string()),
                },
            }
        }

        let body = if let Some(block) = &ctor.body {
            self.push_scope();
            for param in &params {
                self.define(&param.name, param.ty.clone());
            }
            let block = self.transform_block_stmt(block)?;
            self.pop_scope();
            block
        } else {
            Block { stmts: Vec::new(), span }
        };

        Ok(Function {
            name: "new".to_string(),
            generics: Vec::new(),
            params,
            return_type: Some(Type::Named {
                name: class_name.to_string(),
                generics: Vec::new(),
                span,
            }),
            effects: Vec::new(),
            body,
            visibility: Visibility::Public,
            attributes: vec![Attribute {
                name: "constructor".to_string(),
                args: Vec::new(),
                span,
            }],
            span,
        })
    }

    fn try_transform_component(
        &mut self,
        name: &str,
        function: &ts::Function,
        visibility: Visibility,
        _generics: Vec<Generic>,
    ) -> Result<Option<Component>> {
        if !looks_like_component_name(name) {
            return Ok(None);
        }

        let Some(block) = &function.body else {
            return Ok(None);
        };

        let Some((body, prefix_len)) = self.extract_component_render_body(&block.stmts)? else {
            return Ok(None);
        };

        let mut state = Vec::new();
        let mut methods = Vec::new();

        self.push_scope();
        let props = self.map_params(&function.params)?;
        for prop in &props {
            self.define(&prop.name, prop.ty.clone());
        }

        for stmt in &block.stmts[..prefix_len] {
            if !self.try_hoist_component_stmt(stmt, &mut state, &mut methods)? {
                self.pop_scope();
                return Ok(None);
            }
        }

        let mut effects = Vec::new();
        if function.is_async {
            effects.push(Effect::Async);
        }
        effects.push(Effect::Reactive);

        self.pop_scope();

        Ok(Some(Component {
            name: name.to_string(),
            props,
            state,
            methods,
            effects,
            body,
            visibility,
            attributes: Vec::new(),
            span: Span::default(),
        }))
    }

    fn extract_component_render_body(
        &mut self,
        stmts: &[ts::Stmt],
    ) -> Result<Option<(JSXNode, usize)>> {
        let Some(last_stmt) = stmts.last() else {
            return Ok(None);
        };

        let ts::Stmt::Return(ret) = last_stmt else {
            return Ok(None);
        };
        let Some(expr) = ret.arg.as_deref() else {
            return Ok(None);
        };

        let Some(body) = self.try_transform_jsx_root(expr)? else {
            return Ok(None);
        };

        Ok(Some((body, stmts.len() - 1)))
    }

    fn try_hoist_component_stmt(
        &mut self,
        stmt: &ts::Stmt,
        state: &mut Vec<StateDecl>,
        methods: &mut Vec<Function>,
    ) -> Result<bool> {
        match stmt {
            ts::Stmt::Decl(ts::Decl::Var(var)) => self.hoist_component_var_decl(var, state, methods),
            ts::Stmt::Decl(ts::Decl::Fn(func)) => {
                let name = self.rename_value(&func.ident.sym);
                let method = self.transform_function_like(
                    name.clone(),
                    &func.function,
                    Visibility::Private,
                    false,
                    func.function
                        .type_params
                        .as_deref()
                        .map(Self::map_generics)
                        .unwrap_or_default(),
                )?;
                methods.push(method);
                self.define(&name, Type::Infer(Span::default()));
                Ok(true)
            }
            ts::Stmt::Empty(_) => Ok(true),
            _ => Ok(false),
        }
    }

    fn hoist_component_var_decl(
        &mut self,
        decl: &ts::VarDecl,
        state: &mut Vec<StateDecl>,
        methods: &mut Vec<Function>,
    ) -> Result<bool> {
        for declarator in &decl.decls {
            if self.try_hoist_use_state(declarator, state, methods)? {
                continue;
            }

            match &declarator.name {
                ts::Pat::Ident(ident) => {
                    let name = self.rename_value(&ident.id.sym);
                    let span = Span::default();

                    if let Some(init) = declarator.init.as_deref() {
                        match init {
                            ts::Expr::Arrow(arrow) => {
                                methods.push(self.transform_named_arrow_method(&name, arrow)?);
                                self.define(&name, Type::Infer(span));
                                continue;
                            }
                            ts::Expr::Fn(func) => {
                                let function = self.transform_function_like(
                                    name.clone(),
                                    &func.function,
                                    Visibility::Private,
                                    false,
                                    func.function
                                        .type_params
                                        .as_deref()
                                        .map(Self::map_generics)
                                        .unwrap_or_default(),
                                )?;
                                methods.push(function);
                                self.define(&name, Type::Infer(span));
                                continue;
                            }
                            _ => {}
                        }
                    }

                    let ty = ident
                        .type_ann
                        .as_ref()
                        .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
                        .transpose()?
                        .unwrap_or(Type::Infer(span));
                    let initial = declarator
                        .init
                        .as_deref()
                        .map(|expr| self.transform_expr(expr))
                        .transpose()?
                        .unwrap_or(Expr::None(span));

                    state.push(StateDecl {
                        name: name.clone(),
                        ty: ty.clone(),
                        initial,
                        weak: false,
                        attributes: Vec::new(),
                        span,
                    });
                    self.define(&name, ty);
                }
                _ => return Ok(false),
            }
        }

        Ok(true)
    }

    fn try_hoist_use_state(
        &mut self,
        declarator: &ts::VarDeclarator,
        state: &mut Vec<StateDecl>,
        methods: &mut Vec<Function>,
    ) -> Result<bool> {
        let ts::Pat::Array(array_pat) = &declarator.name else {
            return Ok(false);
        };
        let Some(init) = declarator.init.as_deref() else {
            return Ok(false);
        };
        let ts::Expr::Call(call) = init else {
            return Ok(false);
        };
        if !self.call_callee_name(call).is_some_and(|name| name.ends_with("useState")) {
            return Ok(false);
        }

        let Some(first) = array_pat.elems.first().and_then(|pat| pat.as_ref()) else {
            return Ok(false);
        };
        let ts::Pat::Ident(state_ident) = first else {
            return Ok(false);
        };

        let state_name = self.rename_value(&state_ident.id.sym);
        let span = Span::default();
        let ty = state_ident
            .type_ann
            .as_ref()
            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
            .transpose()?
            .or_else(|| {
                call.type_args.as_ref().and_then(|type_args| {
                    type_args
                        .params
                        .first()
                        .and_then(|param| self.type_mapper.map_type(param, span).ok())
                })
            })
            .unwrap_or(Type::Infer(span));
        let initial = call
            .args
            .first()
            .map(|arg| self.transform_expr(&arg.expr))
            .transpose()?
            .unwrap_or(Expr::None(span));

        state.push(StateDecl {
            name: state_name.clone(),
            ty: ty.clone(),
            initial,
            weak: false,
            attributes: Vec::new(),
            span,
        });
        self.define(&state_name, ty.clone());

        if let Some(Some(ts::Pat::Ident(setter_ident))) = array_pat.elems.get(1) {
            let setter_name = self.rename_value(&setter_ident.id.sym);
            methods.push(self.make_state_setter(&setter_name, &state_name, ty));
            self.define(&setter_name, Type::Infer(span));
        }

        Ok(true)
    }

    fn transform_named_arrow_method(&mut self, name: &str, arrow: &ts::ArrowExpr) -> Result<Function> {
        let span = Span::default();
        let params = arrow
            .params
            .iter()
            .map(|pat| self.map_pat_to_param(pat))
            .collect::<Result<Vec<_>>>()?;
        let return_type = arrow
            .return_type
            .as_ref()
            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
            .transpose()?;

        self.push_scope();
        for param in &params {
            self.define(&param.name, param.ty.clone());
        }
        let body = match &*arrow.body {
            ts::BlockStmtOrExpr::BlockStmt(block) => self.transform_block_stmt(block)?,
            ts::BlockStmtOrExpr::Expr(expr) => Block {
                stmts: vec![Stmt::Return(Some(self.transform_expr(expr)?), span)],
                span,
            },
        };
        self.pop_scope();

        Ok(Function {
            name: name.to_string(),
            generics: Vec::new(),
            params,
            return_type,
            effects: Vec::new(),
            body,
            visibility: Visibility::Private,
            attributes: Vec::new(),
            span,
        })
    }

    fn make_state_setter(&self, setter_name: &str, state_name: &str, ty: Type) -> Function {
        let span = Span::default();
        Function {
            name: setter_name.to_string(),
            generics: Vec::new(),
            params: vec![Param {
                name: "value".to_string(),
                ty,
                mutable: false,
                default: None,
                span,
            }],
            return_type: None,
            effects: vec![Effect::Reactive],
            body: Block {
                stmts: vec![Stmt::Expr(Expr::Assign {
                    target: Box::new(Expr::Ident(state_name.to_string(), span)),
                    value: Box::new(Expr::Ident("value".to_string(), span)),
                    span,
                })],
                span,
            },
            visibility: Visibility::Private,
            attributes: Vec::new(),
            span,
        }
    }

    fn transform_function_like(
        &mut self,
        name: String,
        function: &ts::Function,
        visibility: Visibility,
        inject_self: bool,
        generics: Vec<Generic>,
    ) -> Result<Function> {
        let span = Span::default();
        let mut params = Vec::new();

        if inject_self {
            let class_name = self.current_class.clone().unwrap_or_else(|| "Self".to_string());
            params.push(Param {
                name: "_self".to_string(),
                ty: Type::Named {
                    name: class_name,
                    generics: Vec::new(),
                    span,
                },
                mutable: true,
                default: None,
                span,
            });
        }

        params.extend(self.map_params(&function.params)?);

        let return_type = function
            .return_type
            .as_ref()
            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
            .transpose()?;

        let mut effects = Vec::new();
        if function.is_async {
            effects.push(Effect::Async);
        }

        self.push_scope();
        for param in &params {
            self.define(&param.name, param.ty.clone());
        }
        let body = if let Some(block) = &function.body {
            self.transform_block_stmt(block)?
        } else {
            Block {
                stmts: Vec::new(),
                span,
            }
        };
        self.pop_scope();

        Ok(Function {
            name,
            generics,
            params,
            return_type,
            effects,
            body,
            visibility,
            attributes: Vec::new(),
            span,
        })
    }

    fn map_params(&mut self, params: &[ts::Param]) -> Result<Vec<Param>> {
        let mut out = Vec::new();
        for param in params {
            out.push(self.map_pat_to_param(&param.pat)?);
        }
        Ok(out)
    }

    fn map_ts_fn_params(&mut self, params: &[ts::TsFnParam]) -> Result<Vec<Param>> {
        let mut out = Vec::new();
        for param in params {
            let param = match param {
                ts::TsFnParam::Ident(ident) => self.binding_ident_to_param(ident)?,
                ts::TsFnParam::Array(array) => self.pattern_to_param_name(&ts::Pat::Array(array.clone().into()), array.type_ann.as_deref().map(|ann| &*ann.type_ann))?,
                ts::TsFnParam::Object(object) => self.pattern_to_param_name(&ts::Pat::Object(object.clone().into()), object.type_ann.as_deref().map(|ann| &*ann.type_ann))?,
                ts::TsFnParam::Rest(rest) => self.pattern_to_param_name(&ts::Pat::Rest(rest.clone().into()), rest.type_ann.as_deref().map(|ann| &*ann.type_ann))?,
            };
            out.push(param);
        }
        Ok(out)
    }

    fn map_pat_to_param(&mut self, pat: &ts::Pat) -> Result<Param> {
        match pat {
            ts::Pat::Ident(ident) => self.binding_ident_to_param(ident),
            ts::Pat::Assign(assign) => {
                let mut param = self.map_pat_to_param(&assign.left)?;
                param.default = Some(self.transform_expr(&assign.right)?);
                Ok(param)
            }
            ts::Pat::Rest(rest) => self.pattern_to_param_name(pat, rest.type_ann.as_deref().map(|ann| &*ann.type_ann)),
            ts::Pat::Array(array) => self.pattern_to_param_name(pat, array.type_ann.as_deref().map(|ann| &*ann.type_ann)),
            ts::Pat::Object(object) => self.pattern_to_param_name(pat, object.type_ann.as_deref().map(|ann| &*ann.type_ann)),
            ts::Pat::Expr(_) | ts::Pat::Invalid(_) => self.pattern_to_param_name(pat, None),
        }
    }

    fn binding_ident_to_param(&mut self, ident: &ts::BindingIdent) -> Result<Param> {
        let span = Span::default();
        let ty = ident
            .type_ann
            .as_ref()
            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
            .transpose()?
            .unwrap_or(Type::Infer(span));

        Ok(Param {
            name: self.rename_value(&ident.id.sym),
            ty,
            mutable: false,
            default: None,
            span,
        })
    }

    fn pattern_to_param_name(&mut self, pat: &ts::Pat, ty_ann: Option<&ts::TsType>) -> Result<Param> {
        let span = Span::default();
        let ty = ty_ann
            .map(|ann| self.type_mapper.map_type(ann, span))
            .transpose()?
            .unwrap_or(Type::Infer(span));
        let name = self.pat_binding_name(pat).unwrap_or_else(|| self.fresh_temp("arg"));
        if !matches!(pat, ts::Pat::Ident(_)) {
            self.note(format!("pattern parameter '{}' simplified to a binding", name));
        }
        Ok(Param {
            name: self.rename_value(&name),
            ty,
            mutable: false,
            default: None,
            span,
        })
    }

    fn transform_block_stmt(&mut self, block: &ts::BlockStmt) -> Result<Block> {
        self.push_scope();
        let mut stmts = Vec::new();
        for stmt in &block.stmts {
            stmts.extend(self.transform_stmt(stmt)?);
        }
        self.pop_scope();
        Ok(Block {
            stmts,
            span: Span::default(),
        })
    }

    fn transform_stmt(&mut self, stmt: &ts::Stmt) -> Result<Vec<Stmt>> {
        let span = Span::default();
        match stmt {
            ts::Stmt::Decl(ts::Decl::Var(var)) => self.transform_var_stmt(var),
            ts::Stmt::Decl(decl) => {
                let items = self.transform_decl(decl.clone())?;
                Ok(items.into_iter().map(|item| Stmt::Item(Box::new(item))).collect())
            }
            ts::Stmt::Expr(expr) => Ok(vec![Stmt::Expr(self.transform_expr(&expr.expr)?)]) ,
            ts::Stmt::Return(ret) => Ok(vec![Stmt::Return(
                ret.arg
                    .as_deref()
                    .map(|expr| self.transform_expr(expr))
                    .transpose()?,
                span,
            )]),
            ts::Stmt::If(if_stmt) => Ok(vec![Stmt::Expr(self.transform_if_stmt(if_stmt)?)]) ,
            ts::Stmt::While(while_stmt) => Ok(vec![Stmt::While {
                condition: self.transform_expr(&while_stmt.test)?,
                body: self.stmt_to_block(&while_stmt.body)?,
                span,
            }]),
            ts::Stmt::ForOf(for_of) => Ok(vec![Stmt::For {
                binding: self.for_head_to_pattern(&for_of.left),
                iter: self.transform_expr(&for_of.right)?,
                body: self.stmt_to_block(&for_of.body)?,
                span,
            }]),
            ts::Stmt::For(for_stmt) => self.transform_for_stmt(for_stmt),
            ts::Stmt::Break(_) => Ok(vec![Stmt::Break(None, span)]),
            ts::Stmt::Continue(_) => Ok(vec![Stmt::Continue(span)]),
            ts::Stmt::Block(block) => Ok(vec![Stmt::Expr(Expr::Block(
                self.transform_block_stmt(block)?,
                span,
            ))]),
            other => {
                self.note(format!("statement {:?} skipped", other));
                Ok(Vec::new())
            }
        }
    }

    fn transform_var_stmt(&mut self, decl: &ts::VarDecl) -> Result<Vec<Stmt>> {
        let span = Span::default();
        let mut out = Vec::new();

        for declarator in &decl.decls {
            let pattern = self.transform_pattern(&declarator.name);
            let ty = self
                .pat_type_annotation(&declarator.name)
                .map(|ann| self.type_mapper.map_type(ann, span))
                .transpose()?;
            let value = declarator
                .init
                .as_deref()
                .map(|expr| self.transform_expr(expr))
                .transpose()?;

            if let Pattern::Binding { name, .. } = &pattern {
                if let Some(ty) = &ty {
                    self.define(name, ty.clone());
                }
            }

            out.push(Stmt::Let {
                pattern,
                ty,
                value,
                span,
            });
        }

        Ok(out)
    }

    fn transform_for_stmt(&mut self, stmt: &ts::ForStmt) -> Result<Vec<Stmt>> {
        let span = Span::default();
        let mut init_stmts = Vec::new();
        if let Some(init) = &stmt.init {
            match init {
                ts::VarDeclOrExpr::VarDecl(var) => init_stmts.extend(self.transform_var_stmt(var)?),
                ts::VarDeclOrExpr::Expr(expr) => {
                    init_stmts.push(Stmt::Expr(self.transform_expr(expr)?));
                }
            }
        }

        let condition = stmt
            .test
            .as_deref()
            .map(|expr| self.transform_expr(expr))
            .transpose()?
            .unwrap_or(Expr::Bool(true, span));

        let mut body = self.stmt_to_block(&stmt.body)?;
        if let Some(update) = &stmt.update {
            body.stmts.push(Stmt::Expr(self.transform_expr(update)?));
        }

        init_stmts.push(Stmt::While {
            condition,
            body,
            span,
        });

        Ok(init_stmts)
    }

    fn transform_if_stmt(&mut self, stmt: &ts::IfStmt) -> Result<Expr> {
        Ok(Expr::If {
            condition: Box::new(self.transform_expr(&stmt.test)?),
            then_branch: self.stmt_to_block(&stmt.cons)?,
            else_branch: stmt
                .alt
                .as_deref()
                .map(|alt| self.transform_else_branch(alt))
                .transpose()?
                .map(Box::new),
            span: Span::default(),
        })
    }

    fn transform_else_branch(&mut self, stmt: &ts::Stmt) -> Result<ElseBranch> {
        match stmt {
            ts::Stmt::If(if_stmt) => Ok(ElseBranch::ElseIf(
                Box::new(self.transform_expr(&if_stmt.test)?),
                self.stmt_to_block(&if_stmt.cons)?,
                if_stmt
                    .alt
                    .as_deref()
                    .map(|alt| self.transform_else_branch(alt))
                    .transpose()?
                    .map(Box::new),
            )),
            _ => Ok(ElseBranch::Else(self.stmt_to_block(stmt)?)),
        }
    }

    fn stmt_to_block(&mut self, stmt: &ts::Stmt) -> Result<Block> {
        match stmt {
            ts::Stmt::Block(block) => self.transform_block_stmt(block),
            _ => Ok(Block {
                stmts: self.transform_stmt(stmt)?,
                span: Span::default(),
            }),
        }
    }

    fn transform_expr(&mut self, expr: &ts::Expr) -> Result<Expr> {
        let span = Span::default();
        match expr {
            ts::Expr::Lit(lit) => self.transform_lit(lit),
            ts::Expr::Ident(ident) => Ok(Expr::Ident(self.rename_value(&ident.sym), span)),
            ts::Expr::This(_) => Ok(Expr::Ident("_self".to_string(), span)),
            ts::Expr::Array(array) => Ok(Expr::Array(
                array
                    .elems
                    .iter()
                    .filter_map(|elem| elem.as_ref())
                    .map(|elem| self.transform_expr(&elem.expr))
                    .collect::<Result<Vec<_>>>()?,
                span,
            )),
            ts::Expr::Object(obj) => self.transform_object_lit(obj),
            ts::Expr::Bin(bin) => Ok(Expr::Binary {
                left: Box::new(self.transform_expr(&bin.left)?),
                op: map_binary_op(bin.op).ok_or_else(|| {
                    ImportError::UnsupportedFeature(format!("unsupported TypeScript binary op {:?}", bin.op))
                })?,
                right: Box::new(self.transform_expr(&bin.right)?),
                span,
            }),
            ts::Expr::Unary(unary) => {
                let operand = self.transform_expr(&unary.arg)?;
                if let Some(op) = map_unary_op(unary.op) {
                    Ok(Expr::Unary {
                        op,
                        operand: Box::new(operand),
                        span,
                    })
                } else {
                    self.note(format!("unary operator {:?} lowered to passthrough", unary.op));
                    Ok(operand)
                }
            }
            ts::Expr::Update(update) => self.transform_update_expr(update),
            ts::Expr::Assign(assign) => self.transform_assign_expr(assign),
            ts::Expr::TsAs(ts_as) => self.transform_expr(&ts_as.expr),
            ts::Expr::TsSatisfies(ts_sat) => self.transform_expr(&ts_sat.expr),
            ts::Expr::TsNonNull(non_null) => self.transform_expr(&non_null.expr),
            ts::Expr::TsTypeAssertion(assertion) => self.transform_expr(&assertion.expr),
            ts::Expr::TsInstantiation(inst) => self.transform_expr(&inst.expr),
            ts::Expr::Member(member) => self.transform_member_expr(member),
            ts::Expr::Call(call) => self.transform_call_expr(call),
            ts::Expr::New(new_expr) => self.transform_new_expr(new_expr),
            ts::Expr::Await(await_expr) => Ok(Expr::Await(
                Box::new(self.transform_expr(&await_expr.arg)?),
                span,
            )),
            ts::Expr::Cond(cond) => Ok(Expr::If {
                condition: Box::new(self.transform_expr(&cond.test)?),
                then_branch: block_with_expr(self.transform_expr(&cond.cons)?),
                else_branch: Some(Box::new(ElseBranch::Else(block_with_expr(
                    self.transform_expr(&cond.alt)?,
                )))),
                span,
            }),
            ts::Expr::Arrow(arrow) => self.transform_arrow_expr(arrow),
            ts::Expr::Paren(paren) => Ok(Expr::Paren(
                Box::new(self.transform_expr(&paren.expr)?),
                span,
            )),
            ts::Expr::OptChain(chain) => self.transform_opt_chain_expr(chain),
            ts::Expr::Tpl(tpl) => self.transform_template_literal(tpl),
            ts::Expr::Fn(func) => self.transform_function_expr(func),
            ts::Expr::JSXElement(element) => self.transform_jsx_element_expr(element),
            ts::Expr::JSXFragment(fragment) => self.transform_jsx_fragment_expr(fragment),
            ts::Expr::Seq(seq) => {
                let last = seq
                    .exprs
                    .last()
                    .ok_or_else(|| ImportError::TransformError("empty sequence expression".to_string()))?;
                self.transform_expr(last)
            }
            other => Err(ImportError::UnsupportedFeature(format!(
                "unsupported TypeScript expression {:?}",
                other
            ))),
        }
    }

    fn transform_lit(&mut self, lit: &ts::Lit) -> Result<Expr> {
        let span = Span::default();
        match lit {
            ts::Lit::Str(value) => Ok(Expr::String(value.value.to_string(), span)),
            ts::Lit::Num(value) => {
                let n = value.value;
                if (n.fract() - 0.0).abs() < f64::EPSILON {
                    Ok(Expr::Int(n as i64, span))
                } else {
                    Ok(Expr::Float(n, span))
                }
            }
            ts::Lit::Bool(value) => Ok(Expr::Bool(value.value, span)),
            ts::Lit::Null(_) => Ok(Expr::None(span)),
            ts::Lit::BigInt(value) => {
                let raw = value
                    .raw
                    .as_deref()
                    .map(|raw| raw.trim_end_matches('n'))
                    .unwrap_or_default();
                let digits = if raw.is_empty() {
                    value.value.to_string()
                } else {
                    raw.to_string()
                };

                match digits.parse::<i64>() {
                    Ok(parsed) => Ok(Expr::Int(parsed, span)),
                    Err(_) => {
                        self.note(format!(
                            "BigInt literal '{}' lowered to string placeholder",
                            digits
                        ));
                        Ok(Expr::String(digits, span))
                    }
                }
            }
            other => Err(ImportError::UnsupportedFeature(format!(
                "unsupported TypeScript literal {:?}",
                other
            ))),
        }
    }

    fn transform_object_lit(&mut self, obj: &ts::ObjectLit) -> Result<Expr> {
        let span = Span::default();
        let mut fields = Vec::new();

        for prop in &obj.props {
            match prop {
                ts::PropOrSpread::Prop(prop) => match &**prop {
                    ts::Prop::KeyValue(prop) => {
                        if let Some(name) = self.prop_name_to_string(&prop.key) {
                            fields.push((self.rename_field(&name), self.transform_expr(&prop.value)?));
                        } else if let ts::PropName::Computed(computed) = &prop.key {
                            if let Some(name) = self.expr_to_field_name(&computed.expr) {
                                fields.push((self.rename_field(&name), self.transform_expr(&prop.value)?));
                                self.note(format!("computed object key '{}' lowered to a plain field", name));
                            } else {
                                self.note("computed object literal key dropped during import".to_string());
                            }
                        } else {
                            self.note("unsupported object literal key dropped during import".to_string());
                        }
                    }
                    ts::Prop::Shorthand(ident) => {
                        let name = self.rename_field(&ident.sym);
                        fields.push((name.clone(), Expr::Ident(self.rename_value(&ident.sym), span)));
                    }
                    other => {
                        return Err(ImportError::UnsupportedFeature(format!(
                            "unsupported object literal property {:?}",
                            other
                        )));
                    }
                },
                ts::PropOrSpread::Spread(_) => {
                    match self.policy.object_spread {
                        SpreadFallbackMode::KeepExplicitFields => {
                            self.note("object spread lowered lossily by keeping explicit fields only".to_string());
                        }
                    }
                }
            }
        }

        Ok(Expr::AggregateInit {
            ty: Type::Infer(span),
            fields,
            zero_fill_rest: false,
            span,
        })
    }

    fn transform_assign_expr(&mut self, assign: &ts::AssignExpr) -> Result<Expr> {
        let span = Span::default();
        let target = self.transform_assign_target(&assign.left)?;
        let value = self.transform_expr(&assign.right)?;

        if assign.op == ts::AssignOp::Assign {
            return Ok(Expr::Assign {
                target: Box::new(target),
                value: Box::new(value),
                span,
            });
        }

        let Some(bin_op) = assign.op.to_update().and_then(map_binary_op) else {
            return Err(ImportError::UnsupportedFeature(format!(
                "unsupported compound assignment {:?}",
                assign.op
            )));
        };

        Ok(Expr::Assign {
            target: Box::new(target.clone()),
            value: Box::new(Expr::Binary {
                left: Box::new(target),
                op: bin_op,
                right: Box::new(value),
                span,
            }),
            span,
        })
    }

    fn transform_assign_target(&mut self, target: &ts::AssignTarget) -> Result<Expr> {
        match target {
            ts::AssignTarget::Simple(simple) => self.transform_simple_assign_target(simple),
            ts::AssignTarget::Pat(pat) => {
                let pat_as_pat: ts::Pat = pat.clone().into();
                let name = self
                    .pat_binding_name(&pat_as_pat)
                    .unwrap_or_else(|| self.fresh_temp("destructure"));
                self.note(format!(
                    "destructuring assignment target simplified to binding '{}'",
                    name
                ));
                Ok(Expr::Ident(self.rename_value(&name), Span::default()))
            }
        }
    }

    fn transform_simple_assign_target(&mut self, target: &ts::SimpleAssignTarget) -> Result<Expr> {
        match target {
            ts::SimpleAssignTarget::Ident(ident) => Ok(Expr::Ident(self.rename_value(&ident.id.sym), Span::default())),
            ts::SimpleAssignTarget::Member(member) => self.transform_member_expr(member),
            ts::SimpleAssignTarget::Paren(paren) => self.transform_expr(&paren.expr),
            ts::SimpleAssignTarget::SuperProp(_) => Err(ImportError::UnsupportedFeature(
                "super property assignment is not supported".to_string(),
            )),
            ts::SimpleAssignTarget::OptChain(_) => Err(ImportError::UnsupportedFeature(
                "optional chain assignment is not supported".to_string(),
            )),
            ts::SimpleAssignTarget::TsAs(ts_as) => self.transform_expr(&ts_as.expr),
            ts::SimpleAssignTarget::TsSatisfies(ts_sat) => self.transform_expr(&ts_sat.expr),
            ts::SimpleAssignTarget::TsNonNull(non_null) => self.transform_expr(&non_null.expr),
            ts::SimpleAssignTarget::TsTypeAssertion(assertion) => self.transform_expr(&assertion.expr),
            ts::SimpleAssignTarget::TsInstantiation(inst) => self.transform_expr(&inst.expr),
            ts::SimpleAssignTarget::Invalid(_) => Err(ImportError::TransformError(
                "invalid assignment target".to_string(),
            )),
        }
    }

    fn transform_member_expr(&mut self, member: &ts::MemberExpr) -> Result<Expr> {
        let span = Span::default();
        let object = self.transform_expr(&member.obj)?;
        match &member.prop {
            ts::MemberProp::Ident(ident) => Ok(Expr::Field {
                object: Box::new(object),
                field: self.rename_field(&ident.sym),
                span,
            }),
            ts::MemberProp::Computed(computed) => Ok(Expr::Index {
                object: Box::new(object),
                index: Box::new(self.transform_expr(&computed.expr)?),
                span,
            }),
            ts::MemberProp::PrivateName(name) => Ok(Expr::Field {
                object: Box::new(object),
                field: self.rename_field(&name.name),
                span,
            }),
        }
    }

    fn transform_call_expr(&mut self, call: &ts::CallExpr) -> Result<Expr> {
        let span = Span::default();
        let args = self.transform_call_args(&call.args)?;

        match &call.callee {
            ts::Callee::Expr(expr) => {
                if let ts::Expr::Member(member) = &**expr {
                    return Ok(self.member_call_to_kain(member, args));
                }
                Ok(Expr::Call {
                    callee: Box::new(self.transform_expr(expr)?),
                    args,
                    span,
                })
            }
            ts::Callee::Import(_) => {
                self.note("dynamic import lowered to import_dynamic(...) call".to_string());
                Ok(Expr::Call {
                    callee: Box::new(Expr::Ident("import_dynamic".to_string(), span)),
                    args,
                    span,
                })
            }
            other => Err(ImportError::UnsupportedFeature(format!(
                "unsupported callee {:?}",
                other
            ))),
        }
    }

    fn transform_new_expr(&mut self, new_expr: &ts::NewExpr) -> Result<Expr> {
        Ok(Expr::Call {
            callee: Box::new(self.transform_expr(&new_expr.callee)?),
            args: self.transform_call_args(new_expr.args.as_deref().unwrap_or(&[]))?,
            span: Span::default(),
        })
    }

    fn transform_update_expr(&mut self, update: &ts::UpdateExpr) -> Result<Expr> {
        let span = Span::default();
        let target = self.transform_expr(&update.arg)?;
        let delta = match update.op {
            ts::UpdateOp::PlusPlus => 1,
            ts::UpdateOp::MinusMinus => -1,
        };
        let value = Expr::Binary {
            left: Box::new(target.clone()),
            op: if delta > 0 { BinaryOp::Add } else { BinaryOp::Sub },
            right: Box::new(Expr::Int(1, span)),
            span,
        };

        Ok(Expr::Assign {
            target: Box::new(target),
            value: Box::new(value),
            span,
        })
    }

    fn transform_opt_chain_expr(&mut self, chain: &ts::OptChainExpr) -> Result<Expr> {
        self.note("optional chaining lowered to plain access/call".to_string());
        match &*chain.base {
            ts::OptChainBase::Member(member) => self.transform_member_expr(member),
            ts::OptChainBase::Call(call) => {
                let args = self.transform_call_args(&call.args)?;
                if let ts::Expr::Member(member) = &*call.callee {
                    Ok(self.member_call_to_kain(member, args))
                } else {
                    Ok(Expr::Call {
                        callee: Box::new(self.transform_expr(&call.callee)?),
                        args,
                        span: Span::default(),
                    })
                }
            }
        }
    }

    fn transform_arrow_expr(&mut self, arrow: &ts::ArrowExpr) -> Result<Expr> {
        let span = Span::default();
        let params = arrow
            .params
            .iter()
            .map(|pat| self.map_pat_to_param(pat))
            .collect::<Result<Vec<_>>>()?;
        let return_type = arrow
            .return_type
            .as_ref()
            .map(|ann| self.type_mapper.map_type(&ann.type_ann, span))
            .transpose()?;

        self.push_scope();
        for param in &params {
            self.define(&param.name, param.ty.clone());
        }
        let body = match &*arrow.body {
            ts::BlockStmtOrExpr::BlockStmt(block) => Expr::Block(self.transform_block_stmt(block)?, span),
            ts::BlockStmtOrExpr::Expr(expr) => self.transform_expr(expr)?,
        };
        self.pop_scope();

        Ok(Expr::Lambda {
            params,
            return_type,
            body: Box::new(body),
            span,
        })
    }

    fn transform_template_literal(&mut self, tpl: &ts::Tpl) -> Result<Expr> {
        let span = Span::default();
        let mut parts = Vec::new();

        for (idx, quasi) in tpl.quasis.iter().enumerate() {
            let raw = quasi.raw.to_string();
            if !raw.is_empty() {
                parts.push(Expr::String(raw, span));
            }
            if let Some(expr) = tpl.exprs.get(idx) {
                parts.push(self.transform_expr(expr)?);
            }
        }

        Ok(Expr::FString(parts, span))
    }

    fn transform_function_expr(&mut self, func: &ts::FnExpr) -> Result<Expr> {
        let name = func
            .ident
            .as_ref()
            .map(|ident| self.rename_value(&ident.sym))
            .unwrap_or_else(|| self.fresh_temp("lambda"));
        let function = self.transform_function_like(
            name.clone(),
            &func.function,
            Visibility::Private,
            false,
            func.function
                .type_params
                .as_deref()
                .map(Self::map_generics)
                .unwrap_or_default(),
        )?;

        Ok(Expr::Lambda {
            params: function.params,
            return_type: function.return_type,
            body: Box::new(Expr::Block(function.body, Span::default())),
            span: Span::default(),
        })
    }

    fn transform_jsx_element_expr(&mut self, element: &ts::JSXElement) -> Result<Expr> {
        Ok(Expr::JSX(
            self.transform_jsx_element_node(element)?,
            Span::default(),
        ))
    }

    fn transform_jsx_fragment_expr(&mut self, fragment: &ts::JSXFragment) -> Result<Expr> {
        Ok(Expr::JSX(
            self.transform_jsx_fragment_node(fragment)?,
            Span::default(),
        ))
    }

    fn transform_jsx_element_node(&mut self, element: &ts::JSXElement) -> Result<JSXNode> {
        let span = Span::default();
        let attributes = self.transform_jsx_attributes(&element.opening.attrs)?;
        let children = self.transform_jsx_children(&element.children)?;
        let name = self.jsx_element_name_to_string(&element.opening.name);
        let sanitized = sanitize_jsx_name(&name);

        if looks_like_component_name(&sanitized) {
            Ok(JSXNode::ComponentCall {
                name: sanitized,
                props: attributes,
                children,
                span,
            })
        } else {
            Ok(JSXNode::Element {
                tag: sanitized.to_ascii_lowercase(),
                attributes,
                children,
                span,
            })
        }
    }

    fn transform_jsx_fragment_node(&mut self, fragment: &ts::JSXFragment) -> Result<JSXNode> {
        Ok(JSXNode::Fragment(
            self.transform_jsx_children(&fragment.children)?,
            Span::default(),
        ))
    }

    fn transform_jsx_attributes(
        &mut self,
        attrs: &[ts::JSXAttrOrSpread],
    ) -> Result<Vec<JSXAttribute>> {
        let mut out = Vec::new();
        for attr in attrs {
            match attr {
                ts::JSXAttrOrSpread::JSXAttr(attr) => {
                    let Some(name) = self.jsx_attr_name_to_string(&attr.name) else {
                        self.note("unsupported JSX attribute name skipped".to_string());
                        continue;
                    };

                    let value = match &attr.value {
                        None => JSXAttrValue::Bool(true),
                        Some(ts::JSXAttrValue::Lit(lit)) => {
                            match lit {
                                ts::Lit::Str(value) => JSXAttrValue::String(value.value.to_string()),
                                ts::Lit::Bool(value) => JSXAttrValue::Bool(value.value),
                                _ => JSXAttrValue::Expr(self.transform_lit(lit)?),
                            }
                        }
                        Some(ts::JSXAttrValue::JSXExprContainer(container)) => {
                            match &container.expr {
                                ts::JSXExpr::Expr(expr) => {
                                    JSXAttrValue::Expr(self.transform_expr(expr)?)
                                }
                                ts::JSXExpr::JSXEmptyExpr(_) => JSXAttrValue::Bool(true),
                            }
                        }
                        Some(ts::JSXAttrValue::JSXElement(element)) => {
                            JSXAttrValue::Expr(Expr::JSX(self.transform_jsx_element_node(element)?, Span::default()))
                        }
                        Some(ts::JSXAttrValue::JSXFragment(fragment)) => {
                            JSXAttrValue::Expr(Expr::JSX(self.transform_jsx_fragment_node(fragment)?, Span::default()))
                        }
                    };

                    out.push(JSXAttribute {
                        name: sanitize_jsx_name(&name),
                        value,
                        span: Span::default(),
                    });
                }
                ts::JSXAttrOrSpread::SpreadElement(spread) => {
                    let lowered_expr = self
                        .transform_expr(&spread.expr)
                        .map(|expr| format!("{:?}", expr))
                        .unwrap_or_else(|_| "<expr>".to_string());
                    self.note(format!(
                        "JSX spread attribute '{}' lowered lossily and skipped",
                        lowered_expr
                    ));
                }
            }
        }
        Ok(out)
    }

    fn transform_jsx_children(&mut self, children: &[ts::JSXElementChild]) -> Result<Vec<JSXNode>> {
        let mut out = Vec::new();
        for child in children {
            match child {
                ts::JSXElementChild::JSXText(text) => {
                    if let Some(normalized) = normalize_jsx_text(&text.value) {
                        out.push(JSXNode::Text(normalized, Span::default()));
                    }
                }
                ts::JSXElementChild::JSXExprContainer(container) => {
                    if let Some(node) = self.transform_jsx_expr_container(container)? {
                        out.push(node);
                    }
                }
                ts::JSXElementChild::JSXElement(element) => {
                    out.push(self.transform_jsx_element_node(element)?);
                }
                ts::JSXElementChild::JSXFragment(fragment) => {
                    out.push(self.transform_jsx_fragment_node(fragment)?);
                }
                ts::JSXElementChild::JSXSpreadChild(spread) => {
                    out.push(JSXNode::Expression(Box::new(self.transform_expr(&spread.expr)?)));
                }
            }
        }
        Ok(out)
    }

    fn transform_jsx_expr_container(
        &mut self,
        container: &ts::JSXExprContainer,
    ) -> Result<Option<JSXNode>> {
        match &container.expr {
            ts::JSXExpr::JSXEmptyExpr(_) => Ok(None),
            ts::JSXExpr::Expr(expr) => self.try_transform_jsx_root(expr),
        }
    }

    fn try_transform_jsx_root(&mut self, expr: &ts::Expr) -> Result<Option<JSXNode>> {
        let span = Span::default();
        match expr {
            ts::Expr::Paren(paren) => self.try_transform_jsx_root(&paren.expr),
            ts::Expr::JSXElement(element) => Ok(Some(self.transform_jsx_element_node(element)?)),
            ts::Expr::JSXFragment(fragment) => Ok(Some(self.transform_jsx_fragment_node(fragment)?)),
            ts::Expr::Cond(cond) => {
                let Some(then_branch) = self.try_transform_jsx_root(&cond.cons)? else {
                    return Ok(None);
                };
                let else_branch = self.try_transform_jsx_root(&cond.alt)?.map(Box::new);
                Ok(Some(JSXNode::If {
                    condition: Box::new(self.transform_expr(&cond.test)?),
                    then_branch: Box::new(then_branch),
                    else_branch,
                    span,
                }))
            }
            ts::Expr::Bin(bin) if bin.op == ts::BinaryOp::LogicalAnd => {
                let Some(then_branch) = self.try_transform_jsx_root(&bin.right)? else {
                    return Ok(None);
                };
                Ok(Some(JSXNode::If {
                    condition: Box::new(self.transform_expr(&bin.left)?),
                    then_branch: Box::new(then_branch),
                    else_branch: None,
                    span,
                }))
            }
            ts::Expr::Call(call) => {
                if let Some(node) = self.try_transform_jsx_map_call(call)? {
                    Ok(Some(node))
                } else {
                    Ok(Some(JSXNode::Expression(Box::new(self.transform_expr(expr)?))))
                }
            }
            _ => Ok(Some(JSXNode::Expression(Box::new(self.transform_expr(expr)?)))),
        }
    }

    fn try_transform_jsx_map_call(&mut self, call: &ts::CallExpr) -> Result<Option<JSXNode>> {
        let ts::Callee::Expr(callee_expr) = &call.callee else {
            return Ok(None);
        };
        let ts::Expr::Member(member) = &**callee_expr else {
            return Ok(None);
        };
        let ts::MemberProp::Ident(prop) = &member.prop else {
            return Ok(None);
        };
        if prop.sym != *"map" || call.args.len() != 1 {
            return Ok(None);
        }

        let iter = self.transform_expr(&member.obj)?;
        let callback = &call.args[0].expr;
        let (binding, body_expr) = match &**callback {
            ts::Expr::Arrow(arrow) => {
                let Some(first) = arrow.params.first() else {
                    return Ok(None);
                };
                let binding = self
                    .pat_binding_name(first)
                    .unwrap_or_else(|| "item".to_string());
                let body = match &*arrow.body {
                    ts::BlockStmtOrExpr::Expr(expr) => expr.as_ref(),
                    ts::BlockStmtOrExpr::BlockStmt(block) => {
                        let Some(ts::Stmt::Return(ret)) = block.stmts.last() else {
                            return Ok(None);
                        };
                        let Some(expr) = ret.arg.as_deref() else {
                            return Ok(None);
                        };
                        expr
                    }
                };
                (binding, body)
            }
            _ => return Ok(None),
        };

        let Some(body) = self.try_transform_jsx_root(body_expr)? else {
            return Ok(None);
        };

        Ok(Some(JSXNode::For {
            binding: self.rename_value(&binding),
            iter: Box::new(iter),
            body: Box::new(body),
            span: Span::default(),
        }))
    }

    fn member_call_to_kain(&mut self, member: &ts::MemberExpr, args: Vec<CallArg>) -> Expr {
        let span = Span::default();
        let receiver = self
            .transform_expr(&member.obj)
            .unwrap_or(Expr::Ident("invalid_receiver".to_string(), span));
        match &member.prop {
            ts::MemberProp::Ident(ident) => Expr::MethodCall {
                receiver: Box::new(receiver),
                method: self.rename_value(&ident.sym),
                args,
                span,
            },
            ts::MemberProp::PrivateName(name) => Expr::MethodCall {
                receiver: Box::new(receiver),
                method: self.rename_value(&name.name),
                args,
                span,
            },
            ts::MemberProp::Computed(computed) => Expr::Call {
                callee: Box::new(Expr::Index {
                    object: Box::new(receiver),
                    index: Box::new(
                        self.transform_expr(&computed.expr)
                            .unwrap_or(Expr::Ident("invalid_index".to_string(), span)),
                    ),
                    span,
                }),
                args,
                span,
            },
        }
    }

    fn transform_call_args(&mut self, args: &[ts::ExprOrSpread]) -> Result<Vec<CallArg>> {
        args.iter()
            .map(|arg| {
                if arg.spread.is_some() {
                    match self.policy.call_spread {
                        SpreadFallbackMode::KeepExplicitFields => {
                            self.note("spread call argument dropped during import".to_string());
                            return Ok(CallArg {
                                name: None,
                                value: Expr::None(Span::default()),
                                span: Span::default(),
                            });
                        }
                    }
                }
                Ok(CallArg {
                    name: None,
                    value: self.transform_expr(&arg.expr)?,
                    span: Span::default(),
                })
            })
            .collect()
    }

    fn transform_pattern(&mut self, pat: &ts::Pat) -> Pattern {
        let span = Span::default();
        match pat {
            ts::Pat::Ident(ident) => Pattern::Binding {
                name: self.rename_value(&ident.id.sym),
                mutable: false,
                span,
            },
            ts::Pat::Array(array) => Pattern::Tuple(
                array
                    .elems
                    .iter()
                    .filter_map(|elem| elem.as_ref())
                    .map(|pat| self.transform_pattern(pat))
                    .collect(),
                span,
            ),
            ts::Pat::Object(object) => Pattern::Struct {
                name: "Object".to_string(),
                fields: object
                    .props
                    .iter()
                    .filter_map(|prop| match prop {
                        ts::ObjectPatProp::Assign(assign) => Some((
                            self.rename_field(&assign.key.id.sym),
                            Pattern::Binding {
                                name: self.rename_value(&assign.key.id.sym),
                                mutable: false,
                                span,
                            },
                        )),
                        ts::ObjectPatProp::KeyValue(key_value) => self
                            .prop_name_to_string(&key_value.key)
                            .map(|name| (self.rename_field(&name), self.transform_pattern(&key_value.value))),
                        ts::ObjectPatProp::Rest(_) => None,
                    })
                    .collect(),
                rest: false,
                span,
            },
            ts::Pat::Assign(assign) => self.transform_pattern(&assign.left),
            ts::Pat::Rest(rest) => self.transform_pattern(&rest.arg),
            ts::Pat::Expr(_) | ts::Pat::Invalid(_) => Pattern::Wildcard(span),
        }
    }

    fn for_head_to_pattern(&mut self, head: &ts::ForHead) -> Pattern {
        match head {
            ts::ForHead::Pat(pat) => self.transform_pattern(pat),
            ts::ForHead::VarDecl(var) => var
                .decls
                .first()
                .map(|decl| self.transform_pattern(&decl.name))
                .unwrap_or(Pattern::Wildcard(Span::default())),
            ts::ForHead::UsingDecl(using_decl) => using_decl
                .decls
                .first()
                .map(|decl| self.transform_pattern(&decl.name))
                .unwrap_or(Pattern::Wildcard(Span::default())),
        }
    }

    fn pat_binding_name(&mut self, pat: &ts::Pat) -> Option<String> {
        match pat {
            ts::Pat::Ident(ident) => Some(ident.id.sym.to_string()),
            ts::Pat::Assign(assign) => self.pat_binding_name(&assign.left),
            ts::Pat::Rest(rest) => self.pat_binding_name(&rest.arg),
            _ => None,
        }
    }

    fn pat_type_annotation<'a>(&self, pat: &'a ts::Pat) -> Option<&'a ts::TsType> {
        match pat {
            ts::Pat::Ident(ident) => ident.type_ann.as_deref().map(|ann| &*ann.type_ann),
            ts::Pat::Array(array) => array.type_ann.as_deref().map(|ann| &*ann.type_ann),
            ts::Pat::Object(object) => object.type_ann.as_deref().map(|ann| &*ann.type_ann),
            ts::Pat::Rest(rest) => rest.type_ann.as_deref().map(|ann| &*ann.type_ann),
            ts::Pat::Assign(assign) => self.pat_type_annotation(&assign.left),
            ts::Pat::Expr(_) | ts::Pat::Invalid(_) => None,
        }
    }

    fn prop_name_to_string(&self, name: &ts::PropName) -> Option<String> {
        match name {
            ts::PropName::Ident(ident) => Some(ident.sym.to_string()),
            ts::PropName::Str(str_) => Some(str_.value.to_string()),
            ts::PropName::Num(num) => Some(num.value.to_string()),
            ts::PropName::BigInt(bigint) => Some(bigint.value.to_string()),
            ts::PropName::Computed(_) => None,
        }
    }

    fn expr_to_field_name(&self, expr: &ts::Expr) -> Option<String> {
        match expr {
            ts::Expr::Ident(ident) => Some(ident.sym.to_string()),
            ts::Expr::Lit(ts::Lit::Str(value)) => Some(value.value.to_string()),
            ts::Expr::Lit(ts::Lit::Num(value)) => Some(value.value.to_string()),
            _ => None,
        }
    }

    fn rename_value(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Value, raw)
    }

    fn rename_type(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Type, raw)
    }

    fn rename_field(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Field, raw)
    }

    fn rename_variant(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Variant, raw)
    }

    fn note(&mut self, msg: String) {
        self.diagnostics.push(msg);
    }

    fn jsx_element_name_to_string(&self, name: &ts::JSXElementName) -> String {
        match name {
            ts::JSXElementName::Ident(ident) => ident.sym.to_string(),
            ts::JSXElementName::JSXMemberExpr(member) => {
                format!("{}.{}", self.jsx_object_to_string(&member.obj), member.prop.sym)
            }
            ts::JSXElementName::JSXNamespacedName(name) => format!("{}:{}", name.ns.sym, name.name.sym),
        }
    }

    fn jsx_object_to_string(&self, obj: &ts::JSXObject) -> String {
        match obj {
            ts::JSXObject::Ident(ident) => ident.sym.to_string(),
            ts::JSXObject::JSXMemberExpr(member) => {
                format!("{}.{}", self.jsx_object_to_string(&member.obj), member.prop.sym)
            }
        }
    }

    fn jsx_attr_name_to_string(&self, name: &ts::JSXAttrName) -> Option<String> {
        match name {
            ts::JSXAttrName::Ident(ident) => Some(ident.sym.to_string()),
            ts::JSXAttrName::JSXNamespacedName(name) => {
                Some(format!("{}_{}", name.ns.sym, name.name.sym))
            }
        }
    }

    fn call_callee_name(&self, call: &ts::CallExpr) -> Option<String> {
        match &call.callee {
            ts::Callee::Expr(expr) => match &**expr {
                ts::Expr::Ident(ident) => Some(ident.sym.to_string()),
                ts::Expr::Member(member) => match &member.prop {
                    ts::MemberProp::Ident(ident) => Some(ident.sym.to_string()),
                    _ => None,
                },
                _ => None,
            },
            _ => None,
        }
    }

    fn push_scope(&mut self) {
        self.local_types.push(HashMap::new());
    }

    fn pop_scope(&mut self) {
        if self.local_types.len() > 1 {
            self.local_types.pop();
        }
    }

    fn define(&mut self, name: &str, ty: Type) {
        if let Some(scope) = self.local_types.last_mut() {
            scope.insert(name.to_string(), ty);
        }
    }

    fn fresh_temp(&mut self, prefix: &str) -> String {
        let name = format!("{}_{}", prefix, self.temp_counter);
        self.temp_counter += 1;
        name
    }

    fn map_generics(type_params: &ts::TsTypeParamDecl) -> Vec<Generic> {
        type_params
            .params
            .iter()
            .map(|param| Generic {
                name: param.name.sym.to_string(),
                bounds: Vec::new(),
                span: Span::default(),
            })
            .collect()
    }
}

fn map_binary_op(op: ts::BinaryOp) -> Option<BinaryOp> {
    Some(match op {
        ts::BinaryOp::Add => BinaryOp::Add,
        ts::BinaryOp::Sub => BinaryOp::Sub,
        ts::BinaryOp::Mul => BinaryOp::Mul,
        ts::BinaryOp::Div => BinaryOp::Div,
        ts::BinaryOp::Mod => BinaryOp::Mod,
        ts::BinaryOp::Exp => BinaryOp::Pow,
        ts::BinaryOp::EqEq | ts::BinaryOp::EqEqEq => BinaryOp::Eq,
        ts::BinaryOp::NotEq | ts::BinaryOp::NotEqEq => BinaryOp::Ne,
        ts::BinaryOp::Lt => BinaryOp::Lt,
        ts::BinaryOp::LtEq => BinaryOp::Le,
        ts::BinaryOp::Gt => BinaryOp::Gt,
        ts::BinaryOp::GtEq => BinaryOp::Ge,
        ts::BinaryOp::LogicalAnd => BinaryOp::And,
        ts::BinaryOp::LogicalOr => BinaryOp::Or,
        ts::BinaryOp::BitAnd => BinaryOp::BitAnd,
        ts::BinaryOp::BitOr => BinaryOp::BitOr,
        ts::BinaryOp::BitXor => BinaryOp::BitXor,
        ts::BinaryOp::LShift => BinaryOp::Shl,
        ts::BinaryOp::RShift | ts::BinaryOp::ZeroFillRShift => BinaryOp::Shr,
        ts::BinaryOp::NullishCoalescing => BinaryOp::Or,
        ts::BinaryOp::In | ts::BinaryOp::InstanceOf => return None,
    })
}

fn map_unary_op(op: ts::UnaryOp) -> Option<UnaryOp> {
    Some(match op {
        ts::UnaryOp::Minus => UnaryOp::Neg,
        ts::UnaryOp::Bang => UnaryOp::Not,
        ts::UnaryOp::Tilde => UnaryOp::BitNot,
        ts::UnaryOp::Plus | ts::UnaryOp::TypeOf | ts::UnaryOp::Void | ts::UnaryOp::Delete => {
            return None
        }
    })
}

fn accessibility_to_visibility(accessibility: Option<ts::Accessibility>) -> Visibility {
    match accessibility {
        Some(ts::Accessibility::Public) => Visibility::Public,
        Some(ts::Accessibility::Protected) => Visibility::Super,
        Some(ts::Accessibility::Private) | None => Visibility::Private,
    }
}

fn block_with_expr(expr: Expr) -> Block {
    Block {
        stmts: vec![Stmt::Expr(expr)],
        span: Span::default(),
    }
}

fn looks_like_component_name(name: &str) -> bool {
    name.chars().next().is_some_and(|ch| ch.is_ascii_uppercase())
}

fn sanitize_jsx_name(name: &str) -> String {
    let mut out = String::new();
    for ch in name.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            out.push(ch);
        } else {
            out.push('_');
        }
    }
    while out.contains("__") {
        out = out.replace("__", "_");
    }
    let out = out.trim_matches('_').to_string();
    if out.is_empty() {
        "Node".to_string()
    } else if out.chars().next().is_some_and(|ch| ch.is_ascii_digit()) {
        format!("n_{}", out)
    } else {
        out
    }
}

fn normalize_jsx_text(raw: &str) -> Option<String> {
    let collapsed = raw.split_whitespace().collect::<Vec<_>>().join(" ");
    if collapsed.is_empty() {
        None
    } else {
        Some(collapsed)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::typescript::parser;
    use std::path::PathBuf;

    #[test]
    fn test_transform_function_body() {
        let source = r#"
            function add(a: number, b: number): number {
                const total = a  b;
                return total;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Function(func) => {
                assert_eq!(func.name, "add");
                assert_eq!(func.params.len(), 2);
                assert!(!func.body.stmts.is_empty());
            }
            other => panic!("expected function, got {:?}", other),
        }
    }

    #[test]
    fn test_transform_class_to_struct_and_impl() {
        let source = r#"
            class Point {
                x: number;
                y: number;

                translate(dx: number, dy: number): number {
                    return this.x + dx;
                }
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        assert_eq!(program.items.len(), 2);
        assert!(matches!(program.items[0], Item::Struct(_)));
        assert!(matches!(program.items[1], Item::Impl(_)));
    }

    #[test]
    fn test_transform_interface_methods_to_function_fields() {
        let source = r#"
            interface UserRepo {
                readonly name: string;
                find(id: number): string;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Struct(item) => {
                assert_eq!(item.fields.len(), 2);
                assert!(matches!(item.fields[1].ty, Type::Function { .. }));
            }
            other => panic!("expected struct, got {:?}", other),
        }
    }

    #[test]
    fn test_transform_optional_chain_expr() {
        let source = r#"
            function read(config: any) {
                return config?.nbody?.enabled;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Function(func) => assert!(!func.body.stmts.is_empty()),
            other => panic!("expected function, got {:?}", other),
        }
    }

    #[test]
    fn test_transform_jsx_expr_to_placeholder() {
        let source = r#"
            export default function Widget() {
                return <Panel title="Hello" />;
            }
        "#;
        let path = PathBuf::from("test.tsx");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Component(component) => match &component.body {
                JSXNode::ComponentCall { name, .. } => assert_eq!(name, "Panel"),
                other => panic!("expected component call, got {:?}", other),
            },
            other => panic!("expected component, got {:?}", other),
        }
    }

    #[test]
    fn test_transform_bigint_literal() {
        let source = r#"
            export default function answer() {
                return 0n;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Function(func) => assert!(!func.body.stmts.is_empty()),
            other => panic!("expected function, got {:?}", other),
        }
    }

    #[test]
    fn test_transform_function_with_jsx_return_stays_function_when_not_component() {
        let source = r#"
            export default function render_widget() {
                return <div>Hello</div>;
            }
        "#;
        let path = PathBuf::from("test.tsx");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();

        match &program.items[0] {
            Item::Function(func) => match &func.body.stmts[0] {
                Stmt::Return(Some(Expr::JSX(JSXNode::Element { tag, .. }, _)), _) => {
                    assert_eq!(tag, "div");
                }
                other => panic!("expected JSX return, got {:?}", other),
            },
            other => panic!("expected function, got {:?}", other),
        }
    }
}
