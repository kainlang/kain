use walrus::ValType;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WasmCRuntimeShimKind {
    Printf,
    Fprintf,
    Puts,
    Atoll,
    Sprintf,
    Strlen,
    Strcmp,
    Strcpy,
    Strncpy,
    Strcat,
    Strdup,
    Fopen,
    Fseek,
    Ftell,
    Fread,
    Fwrite,
    Fclose,
    Free,
    Exit,
}

#[derive(Clone, Copy, Debug)]
pub enum WasmImportSignature {
    I32ToI64,
    I32I32ToI64,
    I32I32ToI32,
    I32I32I64ToI32,
    I32I32ToI64RetPtr,
    I64I64I64ToI64RetPtr,
    I64ToI64,
    I64I64I64ToI64,
    I64I64I64I64ToI64,
    I32ToUnit,
}

#[derive(Clone, Copy, Debug)]
pub struct WasmCRuntimeShim {
    pub c_symbol: &'static str,
    pub kind: WasmCRuntimeShimKind,
    pub host_symbol: Option<&'static str>,
    pub signature: Option<WasmImportSignature>,
}

#[derive(Clone, Copy, Debug)]
pub struct WasmCRuntimeConstant {
    pub c_symbol: &'static str,
    pub value: i64,
}

pub const WASM_C_RUNTIME_SHIMS: &[WasmCRuntimeShim] = &[
    WasmCRuntimeShim {
        c_symbol: "printf",
        kind: WasmCRuntimeShimKind::Printf,
        host_symbol: None,
        signature: None,
    },
    WasmCRuntimeShim {
        c_symbol: "fprintf",
        kind: WasmCRuntimeShimKind::Fprintf,
        host_symbol: None,
        signature: None,
    },
    WasmCRuntimeShim {
        c_symbol: "puts",
        kind: WasmCRuntimeShimKind::Puts,
        host_symbol: None,
        signature: None,
    },
    WasmCRuntimeShim {
        c_symbol: "atoll",
        kind: WasmCRuntimeShimKind::Atoll,
        host_symbol: Some("c_atoll"),
        signature: Some(WasmImportSignature::I32ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "sprintf",
        kind: WasmCRuntimeShimKind::Sprintf,
        host_symbol: Some("c_sprintf"),
        signature: Some(WasmImportSignature::I32I32I64ToI32),
    },
    WasmCRuntimeShim {
        c_symbol: "strlen",
        kind: WasmCRuntimeShimKind::Strlen,
        host_symbol: Some("c_strlen"),
        signature: Some(WasmImportSignature::I32ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "strcmp",
        kind: WasmCRuntimeShimKind::Strcmp,
        host_symbol: Some("c_strcmp"),
        signature: Some(WasmImportSignature::I32I32ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "strcpy",
        kind: WasmCRuntimeShimKind::Strcpy,
        host_symbol: Some("c_strcpy"),
        signature: Some(WasmImportSignature::I32I32ToI32),
    },
    WasmCRuntimeShim {
        c_symbol: "strncpy",
        kind: WasmCRuntimeShimKind::Strncpy,
        host_symbol: Some("c_strncpy"),
        signature: Some(WasmImportSignature::I64I64I64ToI64RetPtr),
    },
    WasmCRuntimeShim {
        c_symbol: "strcat",
        kind: WasmCRuntimeShimKind::Strcat,
        host_symbol: Some("c_strcat"),
        signature: Some(WasmImportSignature::I32I32ToI32),
    },
    WasmCRuntimeShim {
        c_symbol: "strdup",
        kind: WasmCRuntimeShimKind::Strdup,
        host_symbol: Some("c_strdup"),
        signature: Some(WasmImportSignature::I32ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "fopen",
        kind: WasmCRuntimeShimKind::Fopen,
        host_symbol: Some("c_fopen"),
        signature: Some(WasmImportSignature::I32I32ToI64RetPtr),
    },
    WasmCRuntimeShim {
        c_symbol: "fseek",
        kind: WasmCRuntimeShimKind::Fseek,
        host_symbol: Some("c_fseek"),
        signature: Some(WasmImportSignature::I64I64I64ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "ftell",
        kind: WasmCRuntimeShimKind::Ftell,
        host_symbol: Some("c_ftell"),
        signature: Some(WasmImportSignature::I64ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "fread",
        kind: WasmCRuntimeShimKind::Fread,
        host_symbol: Some("c_fread"),
        signature: Some(WasmImportSignature::I64I64I64I64ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "fwrite",
        kind: WasmCRuntimeShimKind::Fwrite,
        host_symbol: Some("c_fwrite"),
        signature: Some(WasmImportSignature::I64I64I64I64ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "fclose",
        kind: WasmCRuntimeShimKind::Fclose,
        host_symbol: Some("c_fclose"),
        signature: Some(WasmImportSignature::I64ToI64),
    },
    WasmCRuntimeShim {
        c_symbol: "free",
        kind: WasmCRuntimeShimKind::Free,
        host_symbol: Some("c_free"),
        signature: Some(WasmImportSignature::I32ToUnit),
    },
    WasmCRuntimeShim {
        c_symbol: "exit",
        kind: WasmCRuntimeShimKind::Exit,
        host_symbol: Some("c_exit"),
        signature: Some(WasmImportSignature::I32ToUnit),
    },
];

pub const WASM_C_RUNTIME_CONSTANTS: &[WasmCRuntimeConstant] = &[
    WasmCRuntimeConstant {
        c_symbol: "SEEK_SET",
        value: 0,
    },
    WasmCRuntimeConstant {
        c_symbol: "SEEK_CUR",
        value: 1,
    },
    WasmCRuntimeConstant {
        c_symbol: "SEEK_END",
        value: 2,
    },
];

pub fn wasm_c_runtime_shim(symbol: &str) -> Option<&'static WasmCRuntimeShim> {
    WASM_C_RUNTIME_SHIMS
        .iter()
        .find(|shim| shim.c_symbol == symbol)
}

pub fn wasm_c_runtime_constant(symbol: &str) -> Option<i64> {
    WASM_C_RUNTIME_CONSTANTS
        .iter()
        .find(|constant| constant.c_symbol == symbol)
        .map(|constant| constant.value)
}

pub fn wasm_import_signature_types(
    signature: WasmImportSignature,
) -> (&'static [ValType], &'static [ValType]) {
    match signature {
        WasmImportSignature::I32ToI64 => (&[ValType::I32], &[ValType::I64]),
        WasmImportSignature::I32I32ToI64 => (&[ValType::I32, ValType::I32], &[ValType::I64]),
        WasmImportSignature::I32I32ToI32 => (&[ValType::I32, ValType::I32], &[ValType::I32]),
        WasmImportSignature::I32I32I64ToI32 => {
            (&[ValType::I32, ValType::I32, ValType::I64], &[ValType::I32])
        }
        WasmImportSignature::I32I32ToI64RetPtr => (&[ValType::I32, ValType::I32], &[ValType::I64]),
        WasmImportSignature::I64I64I64ToI64RetPtr => {
            (&[ValType::I64, ValType::I64, ValType::I64], &[ValType::I64])
        }
        WasmImportSignature::I64ToI64 => (&[ValType::I64], &[ValType::I64]),
        WasmImportSignature::I64I64I64ToI64 => {
            (&[ValType::I64, ValType::I64, ValType::I64], &[ValType::I64])
        }
        WasmImportSignature::I64I64I64I64ToI64 => (
            &[ValType::I64, ValType::I64, ValType::I64, ValType::I64],
            &[ValType::I64],
        ),
        WasmImportSignature::I32ToUnit => (&[ValType::I32], &[]),
    }
}
