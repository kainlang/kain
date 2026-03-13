use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parse_macro_input, parse_quote, Data, DataEnum, DataStruct, DeriveInput, Fields, Generics, Path,
};

#[proc_macro_derive(ToKainValue)]
pub fn derive_to_kain_value(input: TokenStream) -> TokenStream {
    expand_to_kain_value(parse_macro_input!(input as DeriveInput))
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[proc_macro_derive(FromKainValue)]
pub fn derive_from_kain_value(input: TokenStream) -> TokenStream {
    expand_from_kain_value(parse_macro_input!(input as DeriveInput))
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

fn expand_to_kain_value(input: DeriveInput) -> syn::Result<proc_macro2::TokenStream> {
    let ident = input.ident;
    let generics = add_trait_bound(input.generics, parse_quote!(::kain_host::ToKainValue));
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let body = match input.data {
        Data::Struct(data) => derive_struct_to_body(&ident, &data)?,
        Data::Enum(data) => derive_enum_to_body(&ident, &data),
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
    let ident = input.ident;
    let generics = add_trait_bound(input.generics, parse_quote!(::kain_host::FromKainValue));
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let body = match input.data {
        Data::Struct(data) => derive_struct_from_body(&ident, &data)?,
        Data::Enum(data) => derive_enum_from_body(&ident, &data),
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

fn derive_struct_to_body(
    ident: &syn::Ident,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    match &data.fields {
        Fields::Named(fields) => {
            let field_entries = fields.named.iter().map(|field| {
                let field_ident = field.ident.as_ref().expect("named field");
                let field_name = field_ident.to_string();
                quote! {
                    (#field_name, ::kain_host::ToKainValue::to_kain_value(self.#field_ident))
                }
            });

            Ok(quote! {
                ::kain_host::bridge::struct_value(stringify!(#ident), [#(#field_entries),*])
            })
        }
        Fields::Unit => Ok(quote! {
            ::kain_host::bridge::struct_value(
                stringify!(#ident),
                ::std::iter::empty::<(&'static str, ::kain_host::Value)>(),
            )
        }),
        Fields::Unnamed(fields) => Err(syn::Error::new_spanned(
            &fields.unnamed,
            "ToKainValue derive does not support tuple structs yet; use named fields for Kain interop",
        )),
    }
}

fn derive_struct_from_body(
    ident: &syn::Ident,
    data: &DataStruct,
) -> syn::Result<proc_macro2::TokenStream> {
    match &data.fields {
        Fields::Named(fields) => {
            let field_extracts = fields.named.iter().map(|field| {
                let field_ident = field.ident.as_ref().expect("named field");
                let field_name = field_ident.to_string();
                let field_ty = &field.ty;
                quote! {
                    #field_ident: ::kain_host::bridge::take_struct_field::<#field_ty>(&mut fields, #field_name)?
                }
            });

            Ok(quote! {
                let mut fields = ::kain_host::bridge::expect_struct(value, stringify!(#ident))?;
                Ok(Self {
                    #(#field_extracts),*
                })
            })
        }
        Fields::Unit => Ok(quote! {
            let _ = ::kain_host::bridge::expect_struct(value, stringify!(#ident))?;
            Ok(Self)
        }),
        Fields::Unnamed(fields) => Err(syn::Error::new_spanned(
            &fields.unnamed,
            "FromKainValue derive does not support tuple structs yet; use named fields for Kain interop",
        )),
    }
}

fn derive_enum_to_body(ident: &syn::Ident, data: &DataEnum) -> proc_macro2::TokenStream {
    let arms = data.variants.iter().map(|variant| {
        let variant_ident = &variant.ident;
        let variant_name = variant_ident.to_string();

        match &variant.fields {
            Fields::Unit => quote! {
                Self::#variant_ident => {
                    ::kain_host::bridge::enum_variant_value(
                        stringify!(#ident),
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
                            stringify!(#ident),
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
                            stringify!(#ident),
                            #variant_name,
                            vec![#(#values),*],
                        )
                    }
                }
            }
        }
    });

    quote! {
        match self {
            #(#arms),*
        }
    }
}

fn derive_enum_from_body(ident: &syn::Ident, data: &DataEnum) -> proc_macro2::TokenStream {
    let arms = data.variants.iter().map(|variant| {
        let variant_ident = &variant.ident;
        let variant_name = variant_ident.to_string();

        match &variant.fields {
            Fields::Unit => quote! {
                #variant_name => {
                    let _ = ::kain_host::bridge::expect_variant_len(
                        fields,
                        0,
                        stringify!(#ident),
                        #variant_name,
                    )?;
                    Ok(Self::#variant_ident)
                }
            },
            Fields::Unnamed(fields) => {
                let field_types = fields
                    .unnamed
                    .iter()
                    .map(|field| &field.ty)
                    .collect::<Vec<_>>();
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
                            stringify!(#ident),
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
                let field_types = fields
                    .named
                    .iter()
                    .map(|field| &field.ty)
                    .collect::<Vec<_>>();
                let field_count = field_types.len();
                let values = field_names.iter().zip(field_types.iter()).map(
                    |(field_name, field_ty)| {
                        quote! {
                            #field_name: <#field_ty as ::kain_host::FromKainValue>::from_kain_value(
                                values.next().expect("enum variant length checked above"),
                            )?
                        }
                    },
                );

                quote! {
                    #variant_name => {
                        let mut values = ::kain_host::bridge::expect_variant_len(
                            fields,
                            #field_count,
                            stringify!(#ident),
                            #variant_name,
                        )?
                        .into_iter();
                        Ok(Self::#variant_ident { #(#values),* })
                    }
                }
            }
        }
    });

    quote! {
        let (variant, fields) = ::kain_host::bridge::expect_enum(value, stringify!(#ident))?;
        match variant.as_str() {
            #(#arms),*,
            other => Err(::kain_host::KainError::runtime(format!(
                "Unknown variant {}::{}",
                stringify!(#ident),
                other,
            ))),
        }
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
