//! Type mapping utilities for converting source language types to KAIN types

use kain_core::ast::Type;
use std::collections::HashMap;

/// Type mapper for converting source language types to KAIN types
pub struct TypeMapper {
    mappings: HashMap<String, Type>,
}

impl TypeMapper {
    /// Create a new type mapper with default C type mappings
    pub fn new_c() -> Self {
        let mut mappings = HashMap::new();
        
        // Integer types
        mappings.insert("int".into(), Type::Int);
        mappings.insert("long".into(), Type::Int);
        mappings.insert("short".into(), Type::Int);
        mappings.insert("char".into(), Type::Char);
        mappings.insert("signed".into(), Type::Int);
        mappings.insert("unsigned".into(), Type::Int);
        
        // Floating point types
        mappings.insert("float".into(), Type::Float);
        mappings.insert("double".into(), Type::Float);
        
        // Other types
        mappings.insert("void".into(), Type::Unit);
        mappings.insert("bool".into(), Type::Bool);
        mappings.insert("_Bool".into(), Type::Bool);
        
        // stdint.h types
        mappings.insert("int8_t".into(), Type::Int);
        mappings.insert("int16_t".into(), Type::Int);
        mappings.insert("int32_t".into(), Type::Int);
        mappings.insert("int64_t".into(), Type::Int);
        mappings.insert("uint8_t".into(), Type::Int);
        mappings.insert("uint16_t".into(), Type::Int);
        mappings.insert("uint32_t".into(), Type::Int);
        mappings.insert("uint64_t".into(), Type::Int);
        
        // size_t, ptrdiff_t
        mappings.insert("size_t".into(), Type::Int);
        mappings.insert("ptrdiff_t".into(), Type::Int);
        
        Self { mappings }
    }
    
    /// Create a new type mapper with default Rust type mappings
    pub fn new_rust() -> Self {
        let mut mappings = HashMap::new();
        
        // Integer types
        mappings.insert("i8".into(), Type::Int);
        mappings.insert("i16".into(), Type::Int);
        mappings.insert("i32".into(), Type::Int);
        mappings.insert("i64".into(), Type::Int);
        mappings.insert("i128".into(), Type::Int);
        mappings.insert("isize".into(), Type::Int);
        mappings.insert("u8".into(), Type::Int);
        mappings.insert("u16".into(), Type::Int);
        mappings.insert("u32".into(), Type::Int);
        mappings.insert("u64".into(), Type::Int);
        mappings.insert("u128".into(), Type::Int);
        mappings.insert("usize".into(), Type::Int);
        
        // Floating point types
        mappings.insert("f32".into(), Type::Float);
        mappings.insert("f64".into(), Type::Float);
        
        // Other types
        mappings.insert("bool".into(), Type::Bool);
        mappings.insert("char".into(), Type::Char);
        mappings.insert("str".into(), Type::String);
        
        Self { mappings }
    }
    
    /// Get the KAIN type for a source language type name
    pub fn get(&self, type_name: &str) -> Option<&Type> {
        self.mappings.get(type_name)
    }
    
    /// Add a custom type mapping
    pub fn add_mapping(&mut self, source_type: String, kain_type: Type) {
        self.mappings.insert(source_type, kain_type);
    }
    
    /// Map a pointer type
    pub fn map_pointer(&self, inner: Type, mutable: bool) -> Type {
        Type::Ref {
            mutable,
            inner: Box::new(inner),
            lifetime: None,
        }
    }
    
    /// Map an array type
    pub fn map_array(&self, element: Type, size: Option<usize>) -> Type {
        match size {
            Some(n) => Type::Array(Box::new(element), n),
            None => Type::Slice(Box::new(element)),
        }
    }
}

impl Default for TypeMapper {
    fn default() -> Self {
        Self::new_c()
    }
}
