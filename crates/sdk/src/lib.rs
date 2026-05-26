pub use kain_build;
pub use kain_build::{EngineModuleBuild, EngineModuleBuildConfig, EngineModuleBuildResult};
pub use kain_host;
pub use kain_host::reflect;
pub use kain_host::{
    bridge, CompileTarget, EngineModuleExport, EngineModuleExportConfig, Env, FromKainValue,
    HostPreludeConfig, HostResult, HostSession, HostType, KainError, KainReflect, NativeFn,
    NativeFunction, NativeParam, StaticTypeRef, ToKainValue, TypeRegistry, TypeSchema,
    TypedProgram, Value,
};

pub struct KainEngine {
    host: HostSession,
}

impl KainEngine {
    pub fn new() -> Self {
        Self {
            host: HostSession::new(),
        }
    }

    pub fn host(&self) -> &HostSession {
        &self.host
    }

    pub fn host_mut(&mut self) -> &mut HostSession {
        &mut self.host
    }

    pub fn register_type<T>(&mut self) -> &mut Self
    where
        T: KainReflect,
    {
        self.host.register_type::<T>();
        self
    }

    pub fn register_native_fn(
        &mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
        func: NativeFn,
    ) -> &mut Self {
        self.host
            .register_native_fn(name, params, return_type, func);
        self
    }

    pub fn declare_native_fn(
        &mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
    ) -> &mut Self {
        self.host.declare_native_fn(name, params, return_type);
        self
    }

    pub fn load_source(&mut self, source: &str) -> HostResult<&mut Self> {
        self.host.load_source(source)?;
        Ok(self)
    }

    pub fn call<R>(&mut self, function_name: &str, args: Vec<Value>) -> HostResult<R>
    where
        R: FromKainValue,
    {
        self.host.call(function_name, args)
    }

    pub fn run_main<R>(&mut self) -> HostResult<R>
    where
        R: FromKainValue,
    {
        self.host.run_main()
    }

    pub fn emit_type_prelude(&self) -> String {
        self.host.emit_type_prelude()
    }

    pub fn emit_engine_prelude(&self) -> String {
        self.host.emit_engine_prelude()
    }

    pub fn emit_engine_module_source(&self) -> String {
        self.host.emit_engine_module_source()
    }

    pub fn emit_engine_import_source(&self) -> String {
        self.host.emit_engine_import_source()
    }

    pub fn export_engine_module(
        &self,
        config: &EngineModuleExportConfig,
    ) -> HostResult<EngineModuleExport> {
        self.host.export_engine_module(config)
    }

    pub fn set_prelude_module_name(&mut self, module_name: impl Into<String>) -> &mut Self {
        self.host.set_prelude_module_name(module_name);
        self
    }

    pub fn set_auto_use_prelude(&mut self, enabled: bool) -> &mut Self {
        self.host.set_auto_use_prelude(enabled);
        self
    }
}

impl Default for KainEngine {
    fn default() -> Self {
        Self::new()
    }
}

pub struct KainEngineBuilder {
    host: HostSession,
}

impl KainEngineBuilder {
    pub fn new() -> Self {
        Self {
            host: HostSession::new(),
        }
    }

    pub fn register_type<T>(mut self) -> Self
    where
        T: KainReflect,
    {
        self.host.register_type::<T>();
        self
    }

    pub fn register_native_fn(
        mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
        func: NativeFn,
    ) -> Self {
        self.host
            .register_native_fn(name, params, return_type, func);
        self
    }

    pub fn declare_native_fn(
        mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
    ) -> Self {
        self.host.declare_native_fn(name, params, return_type);
        self
    }

    pub fn prelude_module_name(mut self, module_name: impl Into<String>) -> Self {
        self.host.set_prelude_module_name(module_name);
        self
    }

    pub fn auto_use_prelude(mut self, enabled: bool) -> Self {
        self.host.set_auto_use_prelude(enabled);
        self
    }

    pub fn build(self) -> KainEngine {
        KainEngine { host: self.host }
    }
}

impl Default for KainEngineBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug, Clone, PartialEq, ToKainValue, FromKainValue, KainReflect)]
    #[kain(rename = "Vec3")]
    struct Vec3 {
        x: f32,
        y: f32,
        z: f32,
    }

    #[test]
    fn sdk_builder_exposes_reflected_types_to_kain() {
        let mut engine = KainEngineBuilder::new().register_type::<Vec3>().build();
        engine
            .load_source(
                r#"
fn pass_through(value: Vec3) -> Vec3:
    return value
"#,
            )
            .expect("load source");

        let value = Vec3 {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        };
        let echoed = engine
            .call::<Vec3>("pass_through", vec![value.clone().to_kain_value()])
            .expect("call pass_through");

        assert_eq!(echoed, value);
    }

    #[test]
    fn sdk_exposes_engine_module_prelude() {
        let engine = KainEngineBuilder::new()
            .register_type::<Vec3>()
            .prelude_module_name("engine")
            .build();

        let prelude = engine.emit_engine_prelude();
        assert!(prelude.contains("mod engine:"));
        assert!(prelude.contains("use engine::*"));
        assert!(prelude.contains("struct Vec3:"));
    }

    #[test]
    fn sdk_exposes_engine_module_source_without_inline_wrapper() {
        let engine = KainEngineBuilder::new()
            .register_type::<Vec3>()
            .prelude_module_name("engine")
            .build();

        let module_source = engine.emit_engine_module_source();
        assert!(module_source.contains("struct Vec3:"));
        assert!(!module_source.contains("mod engine:"));
        assert!(!module_source.contains("use engine::*"));
    }
}
