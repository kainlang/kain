//! C type system utilities

use lang_c::ast as c_ast;
use kain_core::ast::Type;
use crate::common::c_registry::{map_c_builtin_type_specifier, named_type};
use crate::common::type_mapper::TypeMapper;
use crate::{ImportError, Result};

/// C type transformer
pub struct CTypeTransformer {
    type_mapper: TypeMapper,
}

impl CTypeTransformer {
    pub fn new() -> Self {
        Self {
            type_mapper: TypeMapper::new_c(),
        }
    }
    
    /// Transform a C type specifier to KAIN type
    pub fn transform_type_specifier(&self, spec: &c_ast::TypeSpecifier) -> Result<Type> {
        use c_ast::TypeSpecifier::*;

        if let Some(mapped) = map_c_builtin_type_specifier(spec) {
            return Ok(mapped);
        }

        match spec {
            Struct(struct_type) => {
                // Handle struct types
                self.transform_struct_type(&struct_type.node)
            }
            
            Enum(enum_type) => {
                // Handle enum types
                self.transform_enum_type(&enum_type.node)
            }
            
            TypedefName(name) => {
                // Look up typedef
                self.type_mapper.get(&name.node.name)
                    .cloned()
                    .ok_or_else(|| ImportError::TypeError(format!("Unknown typedef: {}", name.node.name)))
            }
            
            _ => Err(ImportError::UnsupportedFeature(format!("Type specifier: {:?}", spec))),
        }
    }
    
    fn transform_struct_type(&self, _struct_type: &c_ast::StructType) -> Result<Type> {
        if let Some(ident) = &_struct_type.identifier {
            Ok(named_type(&ident.node.name))
        } else {
            Ok(named_type("AnonymousStruct"))
        }
    }
    
    fn transform_enum_type(&self, _enum_type: &c_ast::EnumType) -> Result<Type> {
        if let Some(ident) = &_enum_type.identifier {
            Ok(named_type(&ident.node.name))
        } else {
            Ok(named_type("AnonymousEnum"))
        }
    }
    
    /// Transform a pointer declarator
    pub fn transform_pointer(&self, inner: Type, _qualifiers: &[c_ast::TypeQualifier]) -> Type {
        // Check if const (immutable reference)
        // For now, assume mutable
        self.type_mapper.map_pointer(inner, true)
    }
    
    /// Transform an array declarator
    pub fn transform_array(&self, element: Type, size: Option<usize>) -> Type {
        self.type_mapper.map_array(element, size)
    }
}

impl Default for CTypeTransformer {
    fn default() -> Self {
        Self::new()
    }
}
