use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parse_macro_input, parse_quote, Attribute, Data, DataEnum, DataStruct, DeriveInput, Expr,
    ExprLit, Fields, Generics, Lit, Meta, Path,
};

#[proc_macro_derive(ToKainValue, attributes(kain))]
pub fn derive_to_kain_value(input: TokenStream) -> TokenStream {
    expand_to_kain_value(parse_macro_input!(input as DeriveInput))
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(FromKainValue, attributes(kain))]
pub fn derive_from_kain_value(input: TokenStream) -> TokenStream {
    expand_from_kain_value(parse_macro_input!(input as DeriveInput))
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(KainReflect, attributes(kain))]
pub fn derive_kain_reflect(input: TokenStream) -> TokenStream {
    expand_kain_reflect(parse_macro_input!(input as DeriveInput))
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[derive(Debug, Default, Clone)]
struct KainAttrs {
    rename: Option<String>,
    transparent: bool,
    version: Option<String>,
}

fn expand_to_kain_value(input: DeriveInput) -> syn::Result<proc_macro2::TokenStream> {
    let attrs = parse_kain_attrs(&input.attrs)?;
    let ident = input.ident;
    let type_name = attrs.rename.clone().unwrap_or_else(|| ident.to_string());
    let generics = add_trait_bound(input.generics, parse_quote!(::kain_host::ToKainValue));
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let body = match input.data {
        Data::Struct(data) => derive_struct_to_body(&ident, &type_name, &attrs, &data)?,
        Data::Enum(data) => {
            if attrs.transparent {
                return Err(syn::Error::new_spanned(
                    ident,
                    "#[kain(transparent)] is only supported on structs",
                ));
            }
            derive_enum_to_body(&type_name, &data)?
        }
        Data::Union(data) => {
            return Err(syn::Error::new_spanned(
                data.union_token,
                "ToKainValue cannot be derived for unions",
            ))
        }
    };

    Ok(quote! {
        impl #impl_generics ::kain_host::ToKainValue for #ident #ty_generics #where_clause {
            fn to_kain_value(self) -> ::kain_host::Value {
                #body
            }
        }
    })
}

fn expand_from_kain_value(input: DeriveInput) -> syn::Result<proc_macro2::TokenStream> {
    let attrs = parse_kain_attrs(&input.attrs)?;
    let ident = input.ident;
    let type_name = attrs.rename.clone().unwrap_or_else(|| ident.to_string());
    let generics = add_trait_bound(input.generics, parse_quote!(::kain_host::FromKainValue));
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let body = match input.data {
        Data::Struct(data) => derive_struct_from_body(&ident, &type_name, &attrs, &data)?,
        Data::Enum(data) => {
            if attrs.transparent {
                return Err(syn::Error::new_spanned(
                    ident,
                    "#[kain(transparent)] is only supported on structs",
                ));
            }
            derive_enum_from_body(&type_name, &data)?
        }
        Data::Union(data) => {
            return Err(syn::Error::new_spanned(
                data.union_token,
                "FromKainValue cannot be derived for unions",
            ))
        }
    };

    Ok(quote! {
        impl #impl_generics ::kain_host::FromKainValue for #ident #ty_generics #where_clause {
            fn from_kain_value(value: ::kain_host::Value) -> ::kain_host::HostResult<Self> {
                #body
            }
        }
    })
}

fn expand_kain_reflect(input: DeriveInput) -> syn::Result<proc_macro2::TokenStream> {
    let attrs = parse_kain_attrs(&input.attrs)?;
    let ident = input.ident;
    let type_name = attrs.rename.clone().unwrap_or_else(|| ident.to_string());
    let rust_name = ident.to_string();
    let generics = add_trait_bound(input.generics, parse_quote!(::kain_host::StaticTypeRef));
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let schema = match input.data {
        Data::Struct(data) => derive_struct_schema(&ident, &type_name, &rust_name, &attrs, &data)?,
        Data::Enum(data) => {
            if attrs.transparent {
                return Err(syn::Error::new_spanned(
                    ident,
                    "#[kain(transparent)] is only supported on structs",
                ));
            }
            derive_enum_schema(&ident, &type_name, &rust_name, &attrs, &data)?
        }
        Data::Union(data) => {
            return Err(syn::Error::new_spanned(
                data.union_token,
                "KainReflect cannot be derived for unions",
            ))
        }
    };

    Ok(quote! {
        impl #impl_generics ::kain_host::KainReflect for #ident #ty_generics #where_clause {
            fn schema() -> ::kain_host::reflect::TypeSchema {
                #schema
            }
        }
    })
}

fn derive_struct_to_body(
    ident: &syn::Ident,
    type_name: &str,
    attrs: &KainAttrs,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    if attrs.transparent {
        return derive_transparent_struct_to_body(ident, data);
    }

    match &data.fields {
        Fields::Named(fields) => {
            let field_entries = fields.named.iter().map(|field| {
                let field_ident = field.ident.as_ref().expect("named field");
                let field_attrs = parse_kain_attrs(&field.attrs).expect("field attrs");
                let field_name = field_attrs
                    .rename
                    .unwrap_or_else(|| field_ident.to_string());
                quote! {
                    (#field_name, ::kain_host::ToKainValue::to_kain_value(self.#field_ident))
                }
            });

            Ok(quote! {
                ::kain_host::bridge::struct_value(#type_name, [#(#field_entries),*])
            })
        }
        Fields::Unit => Ok(quote! {
            ::kain_host::bridge::struct_value(
                #type_name,
                ::std::iter::empty::<(&'static str, ::kain_host::Value)>(),
            )
        }),
        Fields::Unnamed(fields) => Err(syn::Error::new_spanned(
            &fields.unnamed,
            "ToKainValue derive does not support tuple structs unless #[kain(transparent)] is used",
        )),
    }
}

fn derive_struct_from_body(
    ident: &syn::Ident,
    type_name: &str,
    attrs: &KainAttrs,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    if attrs.transparent {
        return derive_transparent_struct_from_body(ident, data);
    }

    match &data.fields {
        Fields::Named(fields) => {
            let field_extracts = fields.named.iter().map(|field| {
                let field_ident = field.ident.as_ref().expect("named field");
                let field_attrs = parse_kain_attrs(&field.attrs).expect("field attrs");
                let field_name = field_attrs
                    .rename
                    .unwrap_or_else(|| field_ident.to_string());
                let field_ty = &field.ty;
                quote! {
                    #field_ident: ::kain_host::bridge::take_struct_field::<#field_ty>(&mut fields, #field_name)?
                }
            });

            Ok(quote! {
                let mut fields = ::kain_host::bridge::expect_struct(value, #type_name)?;
                Ok(Self {
                    #(#field_extracts),*
                })
            })
        }
        Fields::Unit => Ok(quote! {
            let _ = ::kain_host::bridge::expect_struct(value, #type_name)?;
            Ok(Self)
        }),
        Fields::Unnamed(fields) => Err(syn::Error::new_spanned(
            &fields.unnamed,
            "FromKainValue derive does not support tuple structs unless #[kain(transparent)] is used",
        )),
    }
}

fn derive_enum_to_body(type_name: &str, data: &DataEnum) -> syn::Result<proc_macro2::TokenStream> {
    let arms = data
        .variants
        .iter()
        .map(|variant| -> syn::Result<proc_macro2::TokenStream> {
            let variant_ident = &variant.ident;
            let variant_attrs = parse_kain_attrs(&variant.attrs)?;
            let variant_name = variant_attrs
                .rename
                .unwrap_or_else(|| variant_ident.to_string());

            Ok(match &variant.fields {
                Fields::Unit => quote! {
                    Self::#variant_ident => {
                        ::kain_host::bridge::enum_variant_value(
                            #type_name,
                            #variant_name,
                            ::std::vec::Vec::<::kain_host::Value>::new(),
                        )
                    }
                },
                Fields::Unnamed(fields) => {
                    let bindings = (0..fields.unnamed.len())
                        .map(|index| format_ident!("field_{index}"))
                        .collect::<Vec<_>>();
                    let values = bindings.iter().map(|binding| {
                        quote! { ::kain_host::ToKainValue::to_kain_value(#binding) }
                    });

                    quote! {
                        Self::#variant_ident(#(#bindings),*) => {
                            ::kain_host::bridge::enum_variant_value(
                                #type_name,
                                #variant_name,
                                vec![#(#values),*],
                            )
                        }
                    }
                }
                Fields::Named(fields) => {
                    let bindings = fields
                        .named
                        .iter()
                        .map(|field| field.ident.as_ref().expect("named field"))
                        .collect::<Vec<_>>();
                    let values = bindings.iter().map(|binding| {
                        quote! { ::kain_host::ToKainValue::to_kain_value(#binding) }
                    });

                    quote! {
                        Self::#variant_ident { #(#bindings),* } => {
                            ::kain_host::bridge::enum_variant_value(
                                #type_name,
                                #variant_name,
                                vec![#(#values),*],
                            )
                        }
                    }
                }
            })
        })
        .collect::<syn::Result<Vec<_>>>()?;

    Ok(quote! {
        match self {
            #(#arms),*
        }
    })
}

fn derive_enum_from_body(
    type_name: &str,
    data: &DataEnum,
) -> syn::Result<proc_macro2::TokenStream> {
    let arms = data
        .variants
        .iter()
        .map(|variant| -> syn::Result<proc_macro2::TokenStream> {
            let variant_ident = &variant.ident;
            let variant_attrs = parse_kain_attrs(&variant.attrs)?;
            let variant_name = variant_attrs
                .rename
                .unwrap_or_else(|| variant_ident.to_string());

            Ok(match &variant.fields {
                Fields::Unit => quote! {
                    #variant_name => {
                        let _ = ::kain_host::bridge::expect_variant_len(
                            fields,
                            0,
                            #type_name,
                            #variant_name,
                        )?;
                        Ok(Self::#variant_ident)
                    }
                },
                Fields::Unnamed(fields) => {
                    let field_types = fields.unnamed.iter().map(|field| &field.ty).collect::<Vec<_>>();
                    let field_count = field_types.len();
                    let values = field_types.iter().map(|field_ty| {
                        quote! {
                            <#field_ty as ::kain_host::FromKainValue>::from_kain_value(
                                values.next().expect("enum variant length checked above"),
                            )?
                        }
                    });

                    quote! {
                        #variant_name => {
                            let mut values = ::kain_host::bridge::expect_variant_len(
                                fields,
                                #field_count,
                                #type_name,
                                #variant_name,
                            )?
                            .into_iter();
                            Ok(Self::#variant_ident(#(#values),*))
                        }
                    }
                }
                Fields::Named(fields) => {
                    let field_names = fields
                        .named
                        .iter()
                        .map(|field| field.ident.as_ref().expect("named field"))
                        .collect::<Vec<_>>();
                    let field_types = fields.named.iter().map(|field| &field.ty).collect::<Vec<_>>();
                    let field_count = field_types.len();
                    let values = field_names.iter().zip(field_types.iter()).map(|(field_name, field_ty)| {
                        quote! {
                            #field_name: <#field_ty as ::kain_host::FromKainValue>::from_kain_value(
                                values.next().expect("enum variant length checked above"),
                            )?
                        }
                    });

                    quote! {
                        #variant_name => {
                            let mut values = ::kain_host::bridge::expect_variant_len(
                                fields,
                                #field_count,
                                #type_name,
                                #variant_name,
                            )?
                            .into_iter();
                            Ok(Self::#variant_ident { #(#values),* })
                        }
                    }
                }
            })
        })
        .collect::<syn::Result<Vec<_>>>()?;

    Ok(quote! {
        let (variant, fields) = ::kain_host::bridge::expect_enum(value, #type_name)?;
        match variant.as_str() {
            #(#arms),*,
            other => Err(::kain_host::KainError::runtime(format!(
                "Unknown variant {}::{}",
                #type_name,
                other,
            ))),
        }
    })
}

fn derive_struct_schema(
    _ident: &syn::Ident,
    type_name: &str,
    rust_name: &str,
    attrs: &KainAttrs,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    if attrs.transparent {
        let field_ty = transparent_field_ty(data)?;
        let inner = quote! { <#field_ty as ::kain_host::StaticTypeRef>::type_ref() };
        return Ok(apply_schema_attrs(
            attrs,
            quote! {
                ::kain_host::reflect::TypeSchema::new(
                    #type_name,
                    #rust_name,
                    ::kain_host::reflect::TypeKind::Transparent { inner: #inner },
                )
            },
        ));
    }

    match &data.fields {
        Fields::Named(fields) => {
            let field_schemas = fields
                .named
                .iter()
                .map(|field| derive_named_field_schema(field))
                .collect::<syn::Result<Vec<_>>>()?;
            Ok(apply_schema_attrs(
                attrs,
                quote! {
                    ::kain_host::reflect::TypeSchema::new(
                        #type_name,
                        #rust_name,
                        ::kain_host::reflect::TypeKind::Struct {
                            fields: vec![#(#field_schemas),*],
                        },
                    )
                },
            ))
        }
        Fields::Unit => Ok(apply_schema_attrs(
            attrs,
            quote! {
                ::kain_host::reflect::TypeSchema::new(
                    #type_name,
                    #rust_name,
                    ::kain_host::reflect::TypeKind::Struct { fields: Vec::new() },
                )
            },
        )),
        Fields::Unnamed(fields) => Err(syn::Error::new_spanned(
            &fields.unnamed,
            "KainReflect derive does not support tuple structs unless #[kain(transparent)] is used",
        )),
    }
}

fn derive_enum_schema(
    _ident: &syn::Ident,
    type_name: &str,
    rust_name: &str,
    attrs: &KainAttrs,
    data: &DataEnum,
) -> syn::Result<proc_macro2::TokenStream> {
    let variants = data
        .variants
        .iter()
        .map(|variant| derive_variant_schema(variant))
        .collect::<syn::Result<Vec<_>>>()?;

    Ok(apply_schema_attrs(
        attrs,
        quote! {
            ::kain_host::reflect::TypeSchema::new(
                #type_name,
                #rust_name,
                ::kain_host::reflect::TypeKind::Enum {
                    variants: vec![#(#variants),*],
                },
            )
        },
    ))
}

fn derive_variant_schema(variant: &syn::Variant) -> syn::Result<proc_macro2::TokenStream> {
    let attrs = parse_kain_attrs(&variant.attrs)?;
    let variant_ident = &variant.ident;
    let variant_name = attrs
        .rename
        .clone()
        .unwrap_or_else(|| variant_ident.to_string());

    let (shape, fields) = match &variant.fields {
        Fields::Unit => (
            quote! { ::kain_host::reflect::VariantShape::Unit },
            Vec::new(),
        ),
        Fields::Unnamed(fields) => {
            let fields = fields
                .unnamed
                .iter()
                .enumerate()
                .map(|(index, field)| derive_indexed_field_schema(index, field))
                .collect::<syn::Result<Vec<_>>>()?;
            (quote! { ::kain_host::reflect::VariantShape::Tuple }, fields)
        }
        Fields::Named(fields) => {
            let fields = fields
                .named
                .iter()
                .map(|field| derive_named_field_schema(field))
                .collect::<syn::Result<Vec<_>>>()?;
            (quote! { ::kain_host::reflect::VariantShape::Named }, fields)
        }
    };

    Ok(apply_variant_attrs(
        &attrs,
        quote! {
            ::kain_host::reflect::VariantSchema::new(
                #variant_name,
                #shape,
                vec![#(#fields),*],
            )
        },
    ))
}

fn derive_named_field_schema(field: &syn::Field) -> syn::Result<proc_macro2::TokenStream> {
    let field_ident = field.ident.as_ref().expect("named field");
    let attrs = parse_kain_attrs(&field.attrs)?;
    let field_name = attrs
        .rename
        .clone()
        .unwrap_or_else(|| field_ident.to_string());
    let field_ty = &field.ty;
    let rust_name = field_ident.to_string();

    Ok(apply_field_attrs(
        &attrs,
        rust_name != field_name,
        &rust_name,
        quote! {
            ::kain_host::reflect::FieldSchema::new(
                #field_name,
                <#field_ty as ::kain_host::StaticTypeRef>::type_ref(),
            )
        },
    ))
}

fn derive_indexed_field_schema(
    index: usize,
    field: &syn::Field,
) -> syn::Result<proc_macro2::TokenStream> {
    let attrs = parse_kain_attrs(&field.attrs)?;
    let default_name = format!("field_{index}");
    let field_name = attrs.rename.clone().unwrap_or(default_name.clone());
    let field_ty = &field.ty;

    Ok(apply_field_attrs(
        &attrs,
        default_name != field_name,
        &default_name,
        quote! {
            ::kain_host::reflect::FieldSchema::new(
                #field_name,
                <#field_ty as ::kain_host::StaticTypeRef>::type_ref(),
            )
        },
    ))
}

fn derive_transparent_struct_to_body(
    ident: &syn::Ident,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    match &data.fields {
        Fields::Named(fields) => {
            if fields.named.len() != 1 {
                return Err(syn::Error::new_spanned(
                    ident,
                    "#[kain(transparent)] requires exactly one field",
                ));
            }
            let field_ident = fields
                .named
                .first()
                .and_then(|field| field.ident.as_ref())
                .unwrap();
            Ok(quote! {
                ::kain_host::ToKainValue::to_kain_value(self.#field_ident)
            })
        }
        Fields::Unnamed(fields) => {
            if fields.unnamed.len() != 1 {
                return Err(syn::Error::new_spanned(
                    ident,
                    "#[kain(transparent)] requires exactly one field",
                ));
            }
            Ok(quote! {
                let Self(inner) = self;
                ::kain_host::ToKainValue::to_kain_value(inner)
            })
        }
        Fields::Unit => Err(syn::Error::new_spanned(
            ident,
            "#[kain(transparent)] cannot be used on unit structs",
        )),
    }
}

fn derive_transparent_struct_from_body(
    ident: &syn::Ident,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    let field_ty = transparent_field_ty(data)?;
    match &data.fields {
        Fields::Named(fields) => {
            let field_ident = fields
                .named
                .first()
                .and_then(|field| field.ident.as_ref())
                .unwrap();
            Ok(quote! {
                Ok(Self {
                    #field_ident: <#field_ty as ::kain_host::FromKainValue>::from_kain_value(value)?,
                })
            })
        }
        Fields::Unnamed(_) => Ok(quote! {
            Ok(Self(<#field_ty as ::kain_host::FromKainValue>::from_kain_value(value)?))
        }),
        Fields::Unit => Err(syn::Error::new_spanned(
            ident,
            "#[kain(transparent)] cannot be used on unit structs",
        )),
    }
}

fn transparent_field_ty(data: &DataStruct) -> syn::Result<&syn::Type> {
    match &data.fields {
        Fields::Named(fields) => {
            if fields.named.len() != 1 {
                return Err(syn::Error::new_spanned(
                    &fields.named,
                    "#[kain(transparent)] requires exactly one field",
                ));
            }
            Ok(&fields.named.first().unwrap().ty)
        }
        Fields::Unnamed(fields) => {
            if fields.unnamed.len() != 1 {
                return Err(syn::Error::new_spanned(
                    &fields.unnamed,
                    "#[kain(transparent)] requires exactly one field",
                ));
            }
            Ok(&fields.unnamed.first().unwrap().ty)
        }
        Fields::Unit => Err(syn::Error::new_spanned(
            &data.fields,
            "#[kain(transparent)] cannot be used on unit structs",
        )),
    }
}

fn parse_kain_attrs(attrs: &[Attribute]) -> syn::Result<KainAttrs> {
    let mut out = KainAttrs::default();
    for attr in attrs {
        if !attr.path().is_ident("kain") {
            continue;
        }

        match &attr.meta {
            Meta::List(_) => {
                attr.parse_nested_meta(|meta| {
                    if meta.path.is_ident("rename") {
                        let value = meta.value()?.parse::<Expr>()?;
                        out.rename = Some(parse_string_expr(&value)?);
                        Ok(())
                    } else if meta.path.is_ident("version") {
                        let value = meta.value()?.parse::<Expr>()?;
                        out.version = Some(parse_string_expr(&value)?);
                        Ok(())
                    } else if meta.path.is_ident("transparent") {
                        out.transparent = true;
                        Ok(())
                    } else {
                        Err(meta.error("unsupported #[kain(...)] attribute"))
                    }
                })?;
            }
            _ => {
                return Err(syn::Error::new_spanned(
                    attr,
                    "#[kain(...)] expects a list of options",
                ))
            }
        }
    }
    Ok(out)
}

fn parse_string_expr(expr: &Expr) -> syn::Result<String> {
    match expr {
        Expr::Lit(ExprLit {
            lit: Lit::Str(value),
            ..
        }) => Ok(value.value()),
        _ => Err(syn::Error::new_spanned(expr, "expected a string literal")),
    }
}

fn add_trait_bound(mut generics: Generics, bound: Path) -> Generics {
    let type_params = generics
        .type_params()
        .map(|param| param.ident.clone())
        .collect::<Vec<_>>();
    let where_clause = generics.make_where_clause();
    for ident in type_params {
        where_clause.predicates.push(parse_quote!(#ident: #bound));
    }
    generics
}

fn apply_schema_attrs(
    attrs: &KainAttrs,
    base: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let mut out = base;
    if let Some(version) = &attrs.version {
        out = quote! { (#out).with_attr("version", #version) };
    }
    out
}

fn apply_variant_attrs(
    attrs: &KainAttrs,
    base: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let mut out = base;
    if let Some(version) = &attrs.version {
        out = quote! { (#out).with_attr("version", #version) };
    }
    out
}

fn apply_field_attrs(
    attrs: &KainAttrs,
    renamed: bool,
    rust_name: &str,
    base: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let mut out = base;
    if renamed {
        out = quote! { (#out).with_attr("rust_name", #rust_name) };
    }
    if let Some(version) = &attrs.version {
        out = quote! { (#out).with_attr("version", #version) };
    }
    out
}
