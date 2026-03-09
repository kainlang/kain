//! Rust type → KAIN type mapping.
//!
//! Rust and KAIN share the same type-system philosophy (generics, algebraic
//! types, references, slices) so this mapping is much cleaner than C→KAIN.
//!
//! Key design decisions:
//!
//! - `Box<T>`, `Arc<T>`, `Rc<T>`, `Cell<T>`, `RefCell<T>` → transparent (inner T)
//!   These are ownership wrappers; KAIN handles ownership at the language level.
//! - `Vec<T>` → `Array<T>` (KAIN's growable collection)
//! - `HashMap<K,V>` / `BTreeMap<K,V>` → `Map<K,V>`
//! - `HashSet<T>` / `BTreeSet<T>` → `Set<T>`
//! - `&T` / `&mut T` → `Type::Ref` (lifetime erased)
//! - `*const T` / `*mut T` → `Type::Ptr` (low-level memory layer)
//! - `impl Trait` → `Type::Impl`
//! - Lifetimes → erased (KAIN uses effect system for safety)

use kain_core::ast::{PointerProvenance, Type};
use kain_core::span::Span;
use std::collections::HashMap;

pub struct RustTypeMapper {
    visible_paths: HashMap<String, Vec<String>>,
    preserve_wrapper_types: bool,
}

impl RustTypeMapper {
    pub fn new() -> Self {
        Self {
            visible_paths: HashMap::new(),
            preserve_wrapper_types: false,
        }
    }

    pub fn new_selfhost() -> Self {
        Self {
            visible_paths: HashMap::new(),
            preserve_wrapper_types: true,
        }
    }

    pub fn register_visible_path(&mut self, visible_name: impl Into<String>, full_path: Vec<String>) {
        self.visible_paths.insert(visible_name.into(), full_path);
    }

    pub fn resolve_path_segments(&self, path: &syn::Path) -> Vec<String> {
        let mut segments = path
            .segments
            .iter()
            .map(|seg| seg.ident.to_string())
            .collect::<Vec<_>>();
        if let Some(first) = segments.first().cloned() {
            if let Some(expanded) = self.visible_paths.get(&first) {
                let mut resolved = expanded.clone();
                resolved.extend(segments.drain(1..));
                return resolved;
            }
        }
        segments
    }

    pub fn map_type(&self, ty: &syn::Type) -> Type {
        match ty {
            syn::Type::Path(tp)         => self.map_path(tp),
            syn::Type::Reference(r)     => self.map_ref(r),
            syn::Type::Ptr(p)           => self.map_ptr(p),
            syn::Type::Array(a)         => self.map_array(a),
            syn::Type::Slice(s)         => self.map_slice(s),
            syn::Type::Tuple(t)         => self.map_tuple(t),
            syn::Type::BareFn(f)        => self.map_bare_fn(f),
            syn::Type::ImplTrait(i)     => self.map_impl_trait(i),
            syn::Type::TraitObject(t)   => self.map_trait_object(t),
            syn::Type::Paren(p)         => self.map_type(&p.elem),
            syn::Type::Group(g)         => self.map_type(&g.elem),
            syn::Type::Never(_)         => Type::Never(S),
            syn::Type::Infer(_)         => Type::Infer(S),
            _                           => named("Unknown"),
        }
    }

    // ── Path types (most common) ──────────────────────────────────────────

    fn map_path(&self, tp: &syn::TypePath) -> Type {
        let resolved_segments = self.resolve_path_segments(&tp.path);
        let seg = match tp.path.segments.last() {
            Some(s) => s,
            None    => return Type::Unit(S),
        };

        let name = resolved_segments
            .last()
            .cloned()
            .unwrap_or_else(|| seg.ident.to_string());
        let generics = self.generic_args(&seg.arguments);

        // Primitives — map to KAIN canonical names
        match name.as_str() {
            "bool"          => return named("Bool"),
            "str" | "String" => return named("String"),
            "char"          => return named("Char"),
            "f32"           => return named("f32"),
            "f64"           => return named("f64"),
            "u8"            => return named("u8"),
            "u16"           => return named("u16"),
            "u32"           => return named("u32"),
            "u64"           => return named("u64"),
            "u128"          => return named("u128"),
            "usize"         => return named("usize"),
            "i8"            => return named("i8"),
            "i16"           => return named("i16"),
            "i32"           => return named("i32"),
            "i64"           => return named("i64"),
            "i128"          => return named("i128"),
            "isize"         => return named("isize"),
            _               => {}
        }

        // Ownership wrappers — unwrap to inner T for ergonomic import, but preserve
        // them in strict self-host mode so recursive compiler/runtime types keep
        // their original indirection shape.
        if Self::is_wrapper_type(name.as_str()) && !self.preserve_wrapper_types {
            if let Some(inner) = generics.first().cloned() {
                return inner;
            }
            return named("Unknown");
        }

        // Well-known standard-library generics
        match name.as_str() {
            "Vec" | "VecDeque" | "LinkedList" => {
                if let Some(inner) = generics.first().cloned() {
                    return Type::Named { name: "Array".to_string(), generics: vec![inner], span: S };
                }
            }
            "Option" => {
                if let Some(inner) = generics.first().cloned() {
                    return Type::Option(Box::new(inner), S);
                }
            }
            "Result" => {
                let mut g = generics.iter().cloned();
                let ok  = g.next().unwrap_or(Type::Unit(S));
                let err = g.next().unwrap_or(named("Error"));
                return Type::Result(Box::new(ok), Box::new(err), S);
            }
            "HashMap" | "BTreeMap" | "IndexMap" => {
                return Type::Named { name: "Map".to_string(), generics, span: S };
            }
            "HashSet" | "BTreeSet" | "IndexSet" => {
                return Type::Named { name: "Set".to_string(), generics, span: S };
            }
            "KainResult" => {
                // KAIN's own Result alias
                let mut g = generics.iter().cloned();
                let ok  = g.next().unwrap_or(named("String"));
                let err = g.next().unwrap_or(named("Error"));
                return Type::Result(Box::new(ok), Box::new(err), S);
            }
            _ => {}
        }

        let qualified_name = if resolved_segments.len() > 1 {
            resolved_segments.join("::")
        } else {
            name
        };
        Type::Named { name: qualified_name, generics, span: S }
    }

    // ── References ────────────────────────────────────────────────────────

    fn map_ref(&self, r: &syn::TypeReference) -> Type {
        Type::Ref {
            mutable:  r.mutability.is_some(),
            inner:    Box::new(self.map_type(&r.elem)),
            lifetime: r.lifetime.as_ref().map(|lt| lt.ident.to_string()),
            span:     S,
        }
    }

    // ── Raw pointers → low-level Ptr ─────────────────────────────────────

    fn map_ptr(&self, p: &syn::TypePtr) -> Type {
        Type::Ptr {
            mutable:    p.mutability.is_some(),
            inner:      Box::new(self.map_type(&p.elem)),
            provenance: PointerProvenance::LoweredRef,
            span:       S,
        }
    }

    // ── Arrays / slices ───────────────────────────────────────────────────

    fn map_array(&self, a: &syn::TypeArray) -> Type {
        let inner = self.map_type(&a.elem);
        let len   = extract_const_usize(&a.len).unwrap_or(0);
        Type::Array(Box::new(inner), len, S)
    }

    fn map_slice(&self, s: &syn::TypeSlice) -> Type {
        Type::Slice(Box::new(self.map_type(&s.elem)), S)
    }

    // ── Tuples ────────────────────────────────────────────────────────────

    fn map_tuple(&self, t: &syn::TypeTuple) -> Type {
        if t.elems.is_empty() {
            return Type::Unit(S);
        }
        Type::Tuple(t.elems.iter().map(|e| self.map_type(e)).collect(), S)
    }

    // ── Function pointers ─────────────────────────────────────────────────

    fn map_bare_fn(&self, f: &syn::TypeBareFn) -> Type {
        let params      = f.inputs.iter().map(|a| self.map_type(&a.ty)).collect();
        let return_type = match &f.output {
            syn::ReturnType::Default  => Type::Unit(S),
            syn::ReturnType::Type(_, ty) => self.map_type(ty),
        };
        Type::Function {
            params,
            return_type: Box::new(return_type),
            effects: vec![],
            span: S,
        }
    }

    // ── impl Trait ────────────────────────────────────────────────────────

    fn map_impl_trait(&self, i: &syn::TypeImplTrait) -> Type {
        for bound in &i.bounds {
            if let syn::TypeParamBound::Trait(tb) = bound {
                if let Some(seg) = tb.path.segments.last() {
                    let trait_name = seg.ident.to_string();
                    let generics   = self.generic_args(&seg.arguments);
                    return Type::Impl { trait_name, generics, span: S };
                }
            }
        }
        Type::Infer(S)
    }

    // ── dyn Trait ─────────────────────────────────────────────────────────

    fn map_trait_object(&self, t: &syn::TypeTraitObject) -> Type {
        // dyn Trait → impl Trait (same structural semantics in KAIN)
        for bound in &t.bounds {
            if let syn::TypeParamBound::Trait(tb) = bound {
                if let Some(seg) = tb.path.segments.last() {
                    let trait_name = seg.ident.to_string();
                    let generics   = self.generic_args(&seg.arguments);
                    return Type::Impl { trait_name, generics, span: S };
                }
            }
        }
        Type::Infer(S)
    }

    // ── Generic argument extraction ───────────────────────────────────────

    pub fn generic_args(&self, args: &syn::PathArguments) -> Vec<Type> {
        match args {
            syn::PathArguments::AngleBracketed(ab) => ab
                .args
                .iter()
                .filter_map(|arg| match arg {
                    syn::GenericArgument::Type(ty) => Some(self.map_type(ty)),
                    _                              => None,
                })
                .collect(),
            _ => vec![],
        }
    }

    /// Map generic parameters (the `<T: Foo>` part of a fn/struct/enum)
    /// into KAIN generic name strings.
    pub fn map_generic_params(
        &self,
        params: &syn::punctuated::Punctuated<syn::GenericParam, syn::token::Comma>,
    ) -> Vec<kain_core::ast::Generic> {
        params
            .iter()
            .filter_map(|p| match p {
                syn::GenericParam::Type(tp) => Some(kain_core::ast::Generic {
                    name: tp.ident.to_string(),
                    bounds: Vec::new(),
                    span: S,
                }),
                syn::GenericParam::Const(cp) => Some(kain_core::ast::Generic {
                    name: cp.ident.to_string(),
                    bounds: Vec::new(),
                    span: S,
                }),
                syn::GenericParam::Lifetime(_) => None, // lifetimes erased
            })
            .collect()
    }
}

impl RustTypeMapper {
    fn is_wrapper_type(name: &str) -> bool {
        matches!(
            name,
            "Box"
                | "Arc"
                | "Rc"
                | "Cell"
                | "RefCell"
                | "Mutex"
                | "RwLock"
                | "ManuallyDrop"
                | "MaybeUninit"
                | "Pin"
                | "Cow"
        )
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

/// Shorthand zero span.
const S: Span = Span { start: 0, end: 0 };

fn named(n: &str) -> Type {
    Type::Named { name: n.to_string(), generics: vec![], span: S }
}

fn extract_const_usize(expr: &syn::Expr) -> Option<usize> {
    match expr {
        syn::Expr::Lit(lit) => {
            if let syn::Lit::Int(i) = &lit.lit {
                i.base10_parse::<usize>().ok()
            } else {
                None
            }
        }
        _ => None,
    }
}
