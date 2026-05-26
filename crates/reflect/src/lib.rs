use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

pub type AttrMap = BTreeMap<String, String>;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PrimitiveType {
    Unit,
    Bool,
    Int,
    Float,
    String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypeRef {
    Primitive(PrimitiveType),
    Named(String),
    Array(Box<TypeRef>),
    Option(Box<TypeRef>),
}

impl TypeRef {
    pub fn render_kain(&self) -> String {
        match self {
            Self::Primitive(primitive) => primitive.render_kain().to_string(),
            Self::Named(name) => name.clone(),
            Self::Array(inner) => format!("Array<{}>", inner.render_kain()),
            Self::Option(inner) => format!("Option<{}>", inner.render_kain()),
        }
    }
}

impl PrimitiveType {
    pub fn render_kain(&self) -> &'static str {
        match self {
            Self::Unit => "Unit",
            Self::Bool => "Bool",
            Self::Int => "Int",
            Self::Float => "Float",
            Self::String => "String",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FieldSchema {
    pub name: String,
    pub ty: TypeRef,
    pub attrs: AttrMap,
}

impl FieldSchema {
    pub fn new(name: impl Into<String>, ty: TypeRef) -> Self {
        Self {
            name: name.into(),
            ty,
            attrs: AttrMap::new(),
        }
    }

    pub fn with_attr(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.attrs.insert(key.into(), value.into());
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum VariantShape {
    Unit,
    Tuple,
    Named,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct VariantSchema {
    pub name: String,
    pub shape: VariantShape,
    pub fields: Vec<FieldSchema>,
    pub attrs: AttrMap,
}

impl VariantSchema {
    pub fn new(name: impl Into<String>, shape: VariantShape, fields: Vec<FieldSchema>) -> Self {
        Self {
            name: name.into(),
            shape,
            fields,
            attrs: AttrMap::new(),
        }
    }

    pub fn with_attr(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.attrs.insert(key.into(), value.into());
        self
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypeKind {
    Struct { fields: Vec<FieldSchema> },
    Enum { variants: Vec<VariantSchema> },
    Transparent { inner: TypeRef },
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TypeSchema {
    pub name: String,
    pub rust_name: String,
    pub kind: TypeKind,
    pub attrs: AttrMap,
}

impl TypeSchema {
    pub fn new(name: impl Into<String>, rust_name: impl Into<String>, kind: TypeKind) -> Self {
        Self {
            name: name.into(),
            rust_name: rust_name.into(),
            kind,
            attrs: AttrMap::new(),
        }
    }

    pub fn with_attr(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.attrs.insert(key.into(), value.into());
        self
    }

    pub fn render_kain(&self) -> String {
        match &self.kind {
            TypeKind::Struct { fields } => {
                if fields.is_empty() {
                    format!("struct {}:\n", self.name)
                } else {
                    let mut out = format!("struct {}:\n", self.name);
                    for field in fields {
                        out.push_str(&format!("    {}: {}\n", field.name, field.ty.render_kain()));
                    }
                    out
                }
            }
            TypeKind::Enum { variants } => {
                let mut out = format!("enum {}:\n", self.name);
                for variant in variants {
                    match variant.shape {
                        VariantShape::Unit => {
                            out.push_str(&format!("    {}\n", variant.name));
                        }
                        VariantShape::Tuple | VariantShape::Named => {
                            let fields = variant
                                .fields
                                .iter()
                                .map(|field| field.ty.render_kain())
                                .collect::<Vec<_>>()
                                .join(", ");
                            out.push_str(&format!("    {}({})\n", variant.name, fields));
                        }
                    }
                }
                out
            }
            TypeKind::Transparent { inner } => {
                format!("type {} = {}\n", self.name, inner.render_kain())
            }
        }
    }
}

pub trait KainReflect {
    fn schema() -> TypeSchema;

    fn register(registry: &mut TypeRegistry) {
        registry.register_schema(Self::schema());
    }
}

pub trait StaticTypeRef {
    fn type_ref() -> TypeRef;
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct TypeRegistry {
    schemas: BTreeMap<String, TypeSchema>,
}

impl TypeRegistry {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn register<T>(&mut self)
    where
        T: KainReflect,
    {
        T::register(self);
    }

    pub fn register_schema(&mut self, schema: TypeSchema) -> Option<TypeSchema> {
        self.schemas.insert(schema.name.clone(), schema)
    }

    pub fn get(&self, name: &str) -> Option<&TypeSchema> {
        self.schemas.get(name)
    }

    pub fn iter(&self) -> impl Iterator<Item = (&String, &TypeSchema)> {
        self.schemas.iter()
    }

    pub fn schemas(&self) -> impl Iterator<Item = &TypeSchema> {
        self.schemas.values()
    }

    pub fn len(&self) -> usize {
        self.schemas.len()
    }

    pub fn is_empty(&self) -> bool {
        self.schemas.is_empty()
    }

    pub fn render_kain_prelude(&self) -> String {
        let mut out = String::new();
        for schema in self.schemas() {
            out.push_str(&schema.render_kain());
            if !out.ends_with("\n\n") {
                out.push('\n');
            }
        }
        out
    }
}

impl StaticTypeRef for () {
    fn type_ref() -> TypeRef {
        TypeRef::Primitive(PrimitiveType::Unit)
    }
}

impl StaticTypeRef for bool {
    fn type_ref() -> TypeRef {
        TypeRef::Primitive(PrimitiveType::Bool)
    }
}

macro_rules! impl_int_type_ref {
    ($($ty:ty),* $(,)?) => {
        $(
            impl StaticTypeRef for $ty {
                fn type_ref() -> TypeRef {
                    TypeRef::Primitive(PrimitiveType::Int)
                }
            }
        )*
    };
}

macro_rules! impl_float_type_ref {
    ($($ty:ty),* $(,)?) => {
        $(
            impl StaticTypeRef for $ty {
                fn type_ref() -> TypeRef {
                    TypeRef::Primitive(PrimitiveType::Float)
                }
            }
        )*
    };
}

impl_int_type_ref!(i8, i16, i32, i64, u8, u16, u32, u64, usize);
impl_float_type_ref!(f32, f64);

impl StaticTypeRef for String {
    fn type_ref() -> TypeRef {
        TypeRef::Primitive(PrimitiveType::String)
    }
}

impl StaticTypeRef for str {
    fn type_ref() -> TypeRef {
        TypeRef::Primitive(PrimitiveType::String)
    }
}

impl<T> StaticTypeRef for Vec<T>
where
    T: StaticTypeRef,
{
    fn type_ref() -> TypeRef {
        TypeRef::Array(Box::new(T::type_ref()))
    }
}

impl<T> StaticTypeRef for Option<T>
where
    T: StaticTypeRef,
{
    fn type_ref() -> TypeRef {
        TypeRef::Option(Box::new(T::type_ref()))
    }
}

impl<T> StaticTypeRef for T
where
    T: KainReflect,
{
    fn type_ref() -> TypeRef {
        TypeRef::Named(T::schema().name)
    }
}
