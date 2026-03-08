//! TypeScript AST → KAIN AST transformation
//!
//! This module walks the SWC TypeScript AST and transforms it into KAIN AST.

use kain_core::ast::*;
use kain_core::span::Span;
use swc_ecma_ast::{Module, ModuleItem, ModuleDecl, Decl, Stmt};
use crate::{ImportError, Result};
use super::types::TypeMapper;

pub struct TypeScriptTransformer {
    // TODO: Add context/state as needed (e.g., symbol table, current scope)
}

impl TypeScriptTransformer {
    pub fn new() -> Self {
        Self {}
    }

    /// Transform a SWC Module into a KAIN Program.
    pub fn transform(&mut self, module: Module) -> Result<Program> {
        let mut items = Vec::new();
        let span = Span::default();

        for module_item in module.body {
            match module_item {
                ModuleItem::ModuleDecl(decl) => {
                    // Handle export declarations, imports, etc.
                    if let Some(item) = self.transform_module_decl(decl)? {
                        items.push(item);
                    }
                }
                ModuleItem::Stmt(stmt) => {
                    // Handle top-level statements (declarations)
                    if let Some(item) = self.transform_stmt(stmt)? {
                        items.push(item);
                    }
                }
            }
        }

        Ok(Program { items, span })
    }

    fn transform_module_decl(&mut self, decl: ModuleDecl) -> Result<Option<Item>> {
        match decl {
            ModuleDecl::ExportDecl(export) => {
                // Transform the exported declaration
                self.transform_decl(export.decl)
            }
            ModuleDecl::ExportDefaultDecl(_) => {
                // TODO: Handle default exports
                Ok(None)
            }
            ModuleDecl::Import(_) => {
                // TODO: Handle imports (might need to track for module resolution)
                Ok(None)
            }
            _ => Ok(None),
        }
    }

    fn transform_stmt(&mut self, stmt: Stmt) -> Result<Option<Item>> {
        match stmt {
            Stmt::Decl(decl) => self.transform_decl(decl),
            _ => Ok(None), // Other statements are not top-level items
        }
    }

    fn transform_decl(&mut self, decl: Decl) -> Result<Option<Item>> {
        let span = Span::default();
        
        match decl {
            // Function declarations
            Decl::Fn(func) => {
                let name = func.ident.sym.to_string();
                let params = self.transform_params(&func.function.params)?;
                let return_type = func.function.return_type
                    .as_ref()
                    .map(|rt| TypeMapper::map_type(&rt.type_ann, span))
                    .transpose()?;
                
                // Create empty body for now (TODO: transform function body)
                let body = Block {
                    stmts: vec![],
                    span,
                };

                Ok(Some(Item::Function(Function {
                    name,
                    params,
                    return_type,
                    body,
                    visibility: Visibility::Public,
                    generics: vec![],
                    effects: vec![],
                    attributes: vec![],
                    span,
                })))
            }

            // TypeScript interface declarations
            Decl::TsInterface(interface) => {
                let name = interface.id.sym.to_string();
                let mut fields = Vec::new();

                for member in &interface.body.body {
                    if let swc_ecma_ast::TsTypeElement::TsPropertySignature(prop) = member {
                        if let swc_ecma_ast::Expr::Ident(ident) = &*prop.key {
                            let field_name = ident.sym.to_string();
                            let field_type = prop.type_ann
                                .as_ref()
                                .map(|ta| TypeMapper::map_type(&ta.type_ann, span))
                                .transpose()?
                                .unwrap_or(Type::Infer(span));

                            fields.push(Field {
                                name: field_name,
                                ty: field_type,
                                visibility: Visibility::Public,
                                attributes: vec![],
                                default: None,
                                weak: false,
                                span,
                            });
                        }
                    }
                }

                Ok(Some(Item::Struct(Struct {
                    name,
                    fields,
                    visibility: Visibility::Public,
                    generics: vec![],
                    methods: vec![],
                    attributes: vec![],
                    span,
                })))
            }

            // TypeScript enum declarations
            Decl::TsEnum(ts_enum) => {
                let name = ts_enum.id.sym.to_string();
                let mut variants = Vec::new();

                for member in &ts_enum.members {
                    if let swc_ecma_ast::TsEnumMemberId::Ident(ident) = &member.id {
                        let variant_name = ident.sym.to_string();
                        variants.push(Variant {
                            name: variant_name,
                            fields: VariantFields::Unit,
                            span,
                        });
                    }
                }

                Ok(Some(Item::Enum(Enum {
                    name,
                    variants,
                    visibility: Visibility::Public,
                    generics: vec![],
                    span,
                })))
            }

            // TypeScript type alias declarations
            Decl::TsTypeAlias(type_alias) => {
                let name = type_alias.id.sym.to_string();
                let aliased_type = TypeMapper::map_type(&type_alias.type_ann, span)?;

                Ok(Some(Item::TypeAlias(TypeAlias {
                    name,
                    target: aliased_type,
                    visibility: Visibility::Public,
                    generics: vec![],
                    span,
                })))
            }

            // Class declarations
            Decl::Class(class) => {
                let name = class.ident.sym.to_string();
                let mut fields = Vec::new();

                // Extract class properties
                for member in &class.class.body {
                    if let swc_ecma_ast::ClassMember::ClassProp(prop) = member {
                        if let swc_ecma_ast::PropName::Ident(ident_name) = &prop.key {
                            let field_name = ident_name.sym.to_string();
                            let field_type = prop.type_ann
                                .as_ref()
                                .map(|ta| TypeMapper::map_type(&ta.type_ann, span))
                                .transpose()?
                                .unwrap_or(Type::Infer(span));

                            fields.push(Field {
                                name: field_name,
                                ty: field_type,
                                visibility: Visibility::Public,
                                attributes: vec![],
                                default: None,
                                weak: false,
                                span,
                            });
                        }
                    }
                }

                // TODO: Transform class methods into impl block

                Ok(Some(Item::Struct(Struct {
                    name,
                    fields,
                    visibility: Visibility::Public,
                    generics: vec![],
                    methods: vec![],
                    attributes: vec![],
                    span,
                })))
            }

            // Variable declarations (const, let, var)
            Decl::Var(_) => {
                // TODO: Transform variable declarations
                Ok(None)
            }

            _ => Ok(None),
        }
    }

    fn transform_params(&mut self, params: &[swc_ecma_ast::Param]) -> Result<Vec<Param>> {
        let span = Span::default();
        let mut kain_params = Vec::new();

        for param in params {
            if let swc_ecma_ast::Pat::Ident(ident) = &param.pat {
                let name = ident.id.sym.to_string();
                let ty = ident.type_ann
                    .as_ref()
                    .map(|ta| TypeMapper::map_type(&ta.type_ann, span))
                    .transpose()?
                    .unwrap_or(Type::Infer(span));

                kain_params.push(Param {
                    name,
                    ty,
                    mutable: false,
                    default: None,
                    span,
                });
            }
        }

        Ok(kain_params)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::typescript::parser;
    use std::path::PathBuf;

    #[test]
    fn test_transform_function() {
        let source = r#"
            function add(a: number, b: number): number {
                return a + b;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();
        
        assert_eq!(program.items.len(), 1);
        match &program.items[0] {
            Item::Function(func) => {
                assert_eq!(func.name, "add");
                assert_eq!(func.params.len(), 2);
            }
            _ => panic!("Expected function item"),
        }
    }

    #[test]
    fn test_transform_interface() {
        let source = r#"
            interface User {
                name: string;
                age: number;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();
        
        assert_eq!(program.items.len(), 1);
        match &program.items[0] {
            Item::Struct(s) => {
                assert_eq!(s.name, "User");
                assert_eq!(s.fields.len(), 2);
            }
            _ => panic!("Expected struct item"),
        }
    }

    #[test]
    fn test_transform_enum() {
        let source = r#"
            enum Color {
                Red,
                Green,
                Blue
            }
        "#;
        let path = PathBuf::from("test.ts");
        let module = parser::parse_typescript(source, &path).unwrap();
        let mut transformer = TypeScriptTransformer::new();
        let program = transformer.transform(module).unwrap();
        
        assert_eq!(program.items.len(), 1);
        match &program.items[0] {
            Item::Enum(e) => {
                assert_eq!(e.name, "Color");
                assert_eq!(e.variants.len(), 3);
            }
            _ => panic!("Expected enum item"),
        }
    }
}
