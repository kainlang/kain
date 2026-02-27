//! Type mapping utilities for converting source language types to KAIN types

use kain_core::ast::Type;
use kain_core::span::Span;
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
        mappings.insert("int".into(), named_type("Int"));
        mappings.insert("long".into(), named_type("Int"));
        mappings.insert("short".into(), named_type("Int"));
        mappings.insert("char".into(), named_type("Char"));
        mappings.insert("signed".into(), named_type("Int"));
        mappings.insert("unsigned".into(), named_type("Int"));
        
        // Floating point types
        mappings.insert("float".into(), named_type("Float"));
        mappings.insert("double".into(), named_type("Float"));
        
        // Other types
        mappings.insert("void".into(), Type::Unit(Span::default()));
        mappings.insert("bool".into(), named_type("Bool"));
        mappings.insert("_Bool".into(), named_type("Bool"));
        
        // stdint.h types
        mappings.insert("int8_t".into(), named_type("Int"));
        mappings.insert("int16_t".into(), named_type("Int"));
        mappings.insert("int32_t".into(), named_type("Int"));
        mappings.insert("int64_t".into(), named_type("Int"));
        mappings.insert("uint8_t".into(), named_type("Int"));
        mappings.insert("uint16_t".into(), named_type("Int"));
        mappings.insert("uint32_t".into(), named_type("Int"));
        mappings.insert("uint64_t".into(), named_type("Int"));
        
        // size_t, ptrdiff_t
        mappings.insert("size_t".into(), named_type("Int"));
        mappings.insert("ptrdiff_t".into(), named_type("Int"));
        
        Self { mappings }
    }
    
    /// Create a new type mapper with default Rust type mappings
    pub fn new_rust() -> Self {
        let mut mappings = HashMap::new();
        
        // Integer types
        mappings.insert("i8".into(), named_type("Int"));
        mappings.insert("i16".into(), named_type("Int"));
        mappings.insert("i32".into(), named_type("Int"));
        mappings.insert("i64".into(), named_type("Int"));
        mappings.insert("i128".into(), named_type("Int"));
        mappings.insert("isize".into(), named_type("Int"));
        mappings.insert("u8".into(), named_type("Int"));
        mappings.insert("u16".into(), named_type("Int"));
        mappings.insert("u32".into(), named_type("Int"));
        mappings.insert("u64".into(), named_type("Int"));
        mappings.insert("u128".into(), named_type("Int"));
        mappings.insert("usize".into(), named_type("Int"));
        
        // Floating point types
        mappings.insert("f32".into(), named_type("Float"));
        mappings.insert("f64".into(), named_type("Float"));
        
        // Other types
        mappings.insert("bool".into(), named_type("Bool"));
        mappings.insert("char".into(), named_type("Char"));
        mappings.insert("str".into(), named_type("String"));
        
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
            span: Span::default(),
        }
    }
    
    /// Map an array type
    pub fn map_array(&self, element: Type, size: Option<usize>) -> Type {
        match size {
            Some(n) => Type::Array(Box::new(element), n, Span::default()),
            None => Type::Slice(Box::new(element), Span::default()),
        }
    }
}

impl Default for TypeMapper {
    fn default() -> Self {
        Self::new_c()
    }
}

fn named_type(name: &str) -> Type {
    Type::Named {
        name: name.to_string(),
        generics: Vec::new(),
        span: Span::default(),
    }
}
