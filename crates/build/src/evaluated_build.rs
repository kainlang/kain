use crate::workspace::KainBuildGraphPlatformPackage;
use blade::{KainBuildTaskSection, KainManifest};
use kain_core::ast::{
    BinaryOp, Block, CallArg, Const, Expr, Function, Item, Pattern, Program, Stmt,
};
use kain_core::diagnostics::SpanMapper;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub(crate) struct EvaluatedBuildScript {
    pub manifest: KainManifest,
    pub platform_packages: Vec<KainBuildGraphPlatformPackage>,
    pub explicit_tasks: Vec<KainBuildTaskSection>,
}

#[derive(Debug, thiserror::Error)]
pub(crate) enum EvaluatedBuildError {
    #[error("Kain parser error: {0}")]
    Kain(#[from] kain_core::KainError),
    #[error("build.kn evaluator error: {0}")]
    Message(String),
}

type EvalResult<T> = Result<T, EvaluatedBuildError>;

pub(crate) fn evaluate_build_script(
    source: &str,
    workspace_root: &Path,
    graph_source: &str,
) -> EvalResult<EvaluatedBuildScript> {
    let program = parse_program(source, graph_source)?;
    BuildEvaluator::new(workspace_root, graph_source, &program).evaluate()
}

fn parse_program(source: &str, graph_source: &str) -> EvalResult<Program> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, graph_source);
    Ok(parser.parse()?)
}

#[derive(Debug, Clone)]
enum BuildValue {
    Unit,
    Bool(bool),
    Int(i64),
    String(String),
    Array(Vec<BuildValue>),
    Function(String),
    Lambda(LambdaValue),
    Project(ProjectSpec),
    Package(PackageSpec),
    Blade(BladeSpec),
    BuildDefaults(BuildDefaultsSpec),
    RunDefaults(RunDefaultsSpec),
    Workspace(WorkspaceSpec),
    SourceSet(SourceSetSpec),
    PlatformPackage(PlatformPackageSpec),
    Task(TaskValue),
    Graph(GraphSpec),
}

#[derive(Debug, Clone)]
struct LambdaValue {
    params: Vec<String>,
    body: Expr,
}

#[derive(Debug, Clone, Default)]
struct ProjectSpec {
    name: String,
    kind: Option<String>,
    version: Option<String>,
    description: Option<String>,
    entry: Option<PathBuf>,
    source_roots: Vec<PathBuf>,
    module_roots: Vec<PathBuf>,
    generated_root: Option<PathBuf>,
    targets: Vec<String>,
    artifact_root: Option<PathBuf>,
    cache_root: Option<PathBuf>,
    profile: Option<String>,
    run_args: Vec<String>,
    watch: Vec<PathBuf>,
}

#[derive(Debug, Clone, Default)]
struct PackageSpec {
    name: String,
    version: Option<String>,
    description: Option<String>,
}

#[derive(Debug, Clone, Default)]
struct BladeSpec {
    name: String,
    kind: Option<String>,
    version: Option<String>,
    entry: Option<PathBuf>,
    source_roots: Vec<PathBuf>,
    module_roots: Vec<PathBuf>,
    build_targets: Vec<String>,
}

#[derive(Debug, Clone, Default)]
struct BuildDefaultsSpec {
    entry: Option<PathBuf>,
    entry_module: Option<String>,
    source_root: Option<PathBuf>,
    source_order: Vec<PathBuf>,
    module_roots: Vec<PathBuf>,
    module_search_paths: Vec<PathBuf>,
    targets: Vec<String>,
    artifact_root: Option<PathBuf>,
    cache_root: Option<PathBuf>,
    profile: Option<String>,
}

#[derive(Debug, Clone, Default)]
struct RunDefaultsSpec {
    entry: Option<PathBuf>,
    blade: Option<String>,
    target: Option<String>,
    args: Vec<String>,
    env: BTreeMap<String, String>,
    cwd: Option<PathBuf>,
    watch: Vec<PathBuf>,
}

#[derive(Debug, Clone, Default)]
struct WorkspaceSpec {
    blades: Vec<PathBuf>,
    blade_roots: Vec<PathBuf>,
    members: Vec<PathBuf>,
    search_roots: Vec<PathBuf>,
    stdlib_root: Option<PathBuf>,
    manifest_root: Option<PathBuf>,
    generated_root: Option<PathBuf>,
}

#[derive(Debug, Clone, Default)]
struct SourceSetSpec {
    name: String,
    roots: Vec<PathBuf>,
    files: Vec<PathBuf>,
    dirs: Vec<PathBuf>,
    globs: Vec<String>,
    excludes: Vec<String>,
}

#[derive(Debug, Clone)]
struct PlatformPackageSpec {
    package: String,
    provider: String,
    source: String,
}

#[derive(Debug, Clone)]
enum TaskValue {
    Sections(Vec<KainBuildTaskSection>),
    GpuSuite(GpuSuiteSpec),
    CudaArtifacts(CudaArtifactsSpec),
    KainRunner(KainRunnerSpec),
    AlbumMode(AlbumModeSpec),
    CapsuleSet(CapsuleSetSpec),
}

#[derive(Debug, Clone)]
struct GpuSuiteSpec {
    id: String,
    common: KainBuildTaskSection,
    fragment: Option<PathBuf>,
    compute: Option<PathBuf>,
    targets: Vec<String>,
    artifact_root: Option<PathBuf>,
}

#[derive(Debug, Clone)]
struct CudaArtifactsSpec {
    common: KainBuildTaskSection,
    stem: Option<String>,
    output_dir: Option<PathBuf>,
    output_roles: Vec<String>,
}

#[derive(Debug, Clone)]
struct KainRunnerSpec {
    common: KainBuildTaskSection,
}

#[derive(Debug, Clone)]
struct AlbumModeSpec {
    common: KainBuildTaskSection,
    mode: Option<String>,
    runner: Option<PathBuf>,
    executable: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    extra_args: Vec<String>,
}

#[derive(Debug, Clone)]
struct CapsuleSetSpec {
    name: String,
    common: KainBuildTaskSection,
    source_output: Option<PathBuf>,
    artifacts_output: Option<PathBuf>,
    evidence_output: Option<PathBuf>,
}

#[derive(Debug, Clone, Default)]
struct GraphSpec {
    manifest: KainManifest,
    platform_packages: Vec<KainBuildGraphPlatformPackage>,
    tasks: Vec<KainBuildTaskSection>,
}

struct BuildEvaluator<'a> {
    workspace_root: &'a Path,
    graph_source: &'a str,
    functions: BTreeMap<String, Function>,
    consts: Vec<Const>,
    globals: BTreeMap<String, BuildValue>,
    scopes: Vec<BTreeMap<String, BuildValue>>,
}

impl<'a> BuildEvaluator<'a> {
    fn new(workspace_root: &'a Path, graph_source: &'a str, program: &Program) -> Self {
        let functions = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function) => Some((function.name.clone(), function.clone())),
                _ => None,
            })
            .collect();
        let consts = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Const(constant) => Some(constant.clone()),
                _ => None,
            })
            .collect();
        Self {
            workspace_root,
            graph_source,
            functions,
            consts,
            globals: BTreeMap::new(),
            scopes: Vec::new(),
        }
    }

    fn evaluate(mut self) -> EvalResult<EvaluatedBuildScript> {
        self.evaluate_consts()?;
        let Some(build_function) = self.functions.get("build").cloned() else {
            return Err(EvaluatedBuildError::Message(
                "missing fn build(ctx: BuildContext) -> BuildGraph".to_string(),
            ));
        };
        let graph = self.call_function(&build_function, vec![BuildValue::Unit])?;
        let graph = match graph {
            BuildValue::Graph(graph) => graph,
            other => {
                return Err(EvaluatedBuildError::Message(format!(
                    "build() returned {}, expected BuildGraph",
                    other.type_name()
                )));
            }
        };
        Ok(EvaluatedBuildScript {
            manifest: graph.manifest,
            platform_packages: sort_platform_packages(graph.platform_packages),
            explicit_tasks: sort_tasks_stably(graph.tasks),
        })
    }

    fn evaluate_consts(&mut self) -> EvalResult<()> {
        for constant in self.consts.clone() {
            let value = self.eval_expr(&constant.value)?;
            self.globals.insert(constant.name, value);
        }
        Ok(())
    }

    fn call_function(&mut self, function: &Function, args: Vec<BuildValue>) -> EvalResult<BuildValue> {
        if args.len() > function.params.len() {
            return Err(EvaluatedBuildError::Message(format!(
                "function '{}' expected at most {} argument(s), got {}",
                function.name,
                function.params.len(),
                args.len()
            )));
        }
        let mut scope = BTreeMap::new();
        for (index, param) in function.params.iter().enumerate() {
            let value = args.get(index).cloned().unwrap_or(BuildValue::Unit);
            scope.insert(param.name.clone(), value);
        }
        self.scopes.push(scope);
        let result = self.eval_block(&function.body);
        self.scopes.pop();
        result
    }

    fn call_lambda(&mut self, lambda: &LambdaValue, args: Vec<BuildValue>) -> EvalResult<BuildValue> {
        if args.len() > lambda.params.len() {
            return Err(EvaluatedBuildError::Message(format!(
                "lambda expected at most {} argument(s), got {}",
                lambda.params.len(),
                args.len()
            )));
        }
        let mut scope = BTreeMap::new();
        for (index, name) in lambda.params.iter().enumerate() {
            let value = args.get(index).cloned().unwrap_or(BuildValue::Unit);
            scope.insert(name.clone(), value);
        }
        self.scopes.push(scope);
        let result = match &lambda.body {
            Expr::Block(block, _) => self.eval_block(block),
            expr => self.eval_expr(expr),
        };
        self.scopes.pop();
        result
    }

    fn eval_block(&mut self, block: &Block) -> EvalResult<BuildValue> {
        let mut last = BuildValue::Unit;
        for stmt in &block.stmts {
            match stmt {
                Stmt::Let { pattern, value, .. } => {
                    let value = match value {
                        Some(expr) => self.eval_expr(expr)?,
                        None => BuildValue::Unit,
                    };
                    self.bind_pattern(pattern, value)?;
                }
                Stmt::Expr(expr) => {
                    last = self.eval_expr(expr)?;
                }
                Stmt::Return(value, _) => {
                    return value
                        .as_ref()
                        .map(|expr| self.eval_expr(expr))
                        .unwrap_or(Ok(BuildValue::Unit));
                }
                Stmt::For {
                    binding,
                    iter,
                    body,
                    ..
                } => {
                    let values = self.eval_expr(iter)?;
                    let BuildValue::Array(values) = values else {
                        return Err(EvaluatedBuildError::Message(
                            "for loops in build.kn evaluator require array iterators".to_string(),
                        ));
                    };
                    for value in values {
                        self.scopes.push(BTreeMap::new());
                        self.bind_pattern(binding, value)?;
                        last = self.eval_block(body)?;
                        self.scopes.pop();
                    }
                }
                Stmt::Item(_) | Stmt::Defer { .. } | Stmt::Dispatch { .. } | Stmt::Break(_, _)
                | Stmt::Continue(_) | Stmt::Fanout { .. } | Stmt::While { .. } | Stmt::Loop { .. } => {
                    return Err(EvaluatedBuildError::Message(format!(
                        "unsupported statement in deterministic build.kn evaluator: {:?}",
                        stmt
                    )));
                }
            }
        }
        Ok(last)
    }

    fn bind_pattern(&mut self, pattern: &Pattern, value: BuildValue) -> EvalResult<()> {
        match pattern {
            Pattern::Binding { name, .. } => {
                if let Some(scope) = self.scopes.last_mut() {
                    scope.insert(name.clone(), value);
                } else {
                    self.globals.insert(name.clone(), value);
                }
                Ok(())
            }
            Pattern::Wildcard(_) => Ok(()),
            _ => Err(EvaluatedBuildError::Message(
                "build.kn evaluator only supports simple let bindings".to_string(),
            )),
        }
    }

    fn eval_expr(&mut self, expr: &Expr) -> EvalResult<BuildValue> {
        match expr {
            Expr::Int(value, _) => Ok(BuildValue::Int(*value)),
            Expr::Float(value, _) => Ok(BuildValue::String(trim_float(*value))),
            Expr::String(value, _) => Ok(BuildValue::String(value.clone())),
            Expr::Bool(value, _) => Ok(BuildValue::Bool(*value)),
            Expr::None(_) => Ok(BuildValue::Unit),
            Expr::Ident(name, _) => self.resolve_ident(name),
            Expr::Paren(inner, _) => self.eval_expr(inner),
            Expr::Array(items, _) => items
                .iter()
                .map(|item| self.eval_expr(item))
                .collect::<EvalResult<Vec<_>>>()
                .map(BuildValue::Array),
            Expr::Tuple(items, _) => items
                .iter()
                .map(|item| self.eval_expr(item))
                .collect::<EvalResult<Vec<_>>>()
                .map(BuildValue::Array),
            Expr::Binary {
                left, op, right, ..
            } => self.eval_binary(left, *op, right),
            Expr::Call { callee, args, .. } => self.eval_call(callee, args),
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let receiver = self.eval_expr(receiver)?;
                self.apply_method(receiver, method, args)
            }
            Expr::Lambda { params, body, .. } => Ok(BuildValue::Lambda(LambdaValue {
                params: params.iter().map(|param| param.name.clone()).collect(),
                body: (**body).clone(),
            })),
            Expr::Block(block, _) => {
                self.scopes.push(BTreeMap::new());
                let result = self.eval_block(block);
                self.scopes.pop();
                result
            }
            Expr::Return(value, _) => value
                .as_ref()
                .map(|expr| self.eval_expr(expr))
                .unwrap_or(Ok(BuildValue::Unit)),
            _ => Err(EvaluatedBuildError::Message(format!(
                "unsupported expression in deterministic build.kn evaluator: {:?}",
                expr
            ))),
        }
    }

    fn resolve_ident(&self, name: &str) -> EvalResult<BuildValue> {
        for scope in self.scopes.iter().rev() {
            if let Some(value) = scope.get(name) {
                return Ok(value.clone());
            }
        }
        if let Some(value) = self.globals.get(name) {
            return Ok(value.clone());
        }
        if self.functions.contains_key(name) {
            return Ok(BuildValue::Function(name.to_string()));
        }
        Err(EvaluatedBuildError::Message(format!(
            "unknown build.kn identifier '{name}'"
        )))
    }

    fn eval_binary(&mut self, left: &Expr, op: BinaryOp, right: &Expr) -> EvalResult<BuildValue> {
        let left = self.eval_expr(left)?;
        let right = self.eval_expr(right)?;
        match op {
            BinaryOp::Add => match (left, right) {
                (BuildValue::String(left), right) => Ok(BuildValue::String(left + &right.as_string()?)),
                (left, BuildValue::String(right)) => Ok(BuildValue::String(left.as_string()? + &right)),
                (BuildValue::Int(left), BuildValue::Int(right)) => Ok(BuildValue::Int(left + right)),
                (left, right) => Err(EvaluatedBuildError::Message(format!(
                    "unsupported + operands in build.kn evaluator: {} + {}",
                    left.type_name(),
                    right.type_name()
                ))),
            },
            _ => Err(EvaluatedBuildError::Message(format!(
                "unsupported binary operator in build.kn evaluator: {:?}",
                op
            ))),
        }
    }

    fn eval_call(&mut self, callee: &Expr, args: &[CallArg]) -> EvalResult<BuildValue> {
        let values = self.eval_call_args(args)?;
        match callee {
            Expr::Ident(name, _) => self.call_named(name, values),
            _ => {
                let callable = self.eval_expr(callee)?;
                self.call_callable(callable, values)
            }
        }
    }

    fn eval_call_args(&mut self, args: &[CallArg]) -> EvalResult<Vec<BuildValue>> {
        args.iter()
            .map(|arg| self.eval_expr(&arg.value))
            .collect::<EvalResult<Vec<_>>>()
    }

    fn call_named(&mut self, name: &str, args: Vec<BuildValue>) -> EvalResult<BuildValue> {
        match name {
            "map" => self.call_map(args),
            "build_graph" => Ok(BuildValue::Graph(self.build_graph_from_args(&args)?)),
            "project" => Ok(BuildValue::Project(ProjectSpec {
                name: first_string_arg(name, &args)?,
                ..ProjectSpec::default()
            })),
            "package" => Ok(BuildValue::Package(PackageSpec {
                name: first_string_arg(name, &args)?,
                ..PackageSpec::default()
            })),
            "blade" => Ok(BuildValue::Blade(BladeSpec {
                name: first_string_arg(name, &args)?,
                ..BladeSpec::default()
            })),
            "build_defaults" => Ok(BuildValue::BuildDefaults(BuildDefaultsSpec::default())),
            "run_defaults" => Ok(BuildValue::RunDefaults(RunDefaultsSpec::default())),
            "workspace_defaults" => Ok(BuildValue::Workspace(WorkspaceSpec::default())),
            "source_set" => Ok(BuildValue::SourceSet(SourceSetSpec {
                name: first_string_arg(name, &args)?,
                ..SourceSetSpec::default()
            })),
            "platform_package" | "build_platform_package" | "platform_requirement"
            | "requires_platform_package" => Ok(BuildValue::PlatformPackage(PlatformPackageSpec {
                package: first_string_arg(name, &args)?,
                provider: "system".to_string(),
                source: self.graph_source.to_string(),
            })),
            "gpu_suite" => {
                let id = first_string_arg(name, &args)?;
                Ok(BuildValue::Task(TaskValue::GpuSuite(GpuSuiteSpec {
                    id: id.clone(),
                    common: KainBuildTaskSection {
                        id,
                        kind: "gpu".to_string(),
                        ..KainBuildTaskSection::default()
                    },
                    fragment: None,
                    compute: None,
                    targets: Vec::new(),
                    artifact_root: None,
                })))
            }
            "cuda_artifacts" => Ok(BuildValue::Task(TaskValue::CudaArtifacts(CudaArtifactsSpec {
                common: KainBuildTaskSection {
                    id: first_string_arg(name, &args)?,
                    kind: "exec".to_string(),
                    command: Some("kain".to_string()),
                    ..KainBuildTaskSection::default()
                },
                stem: None,
                output_dir: None,
                output_roles: Vec::new(),
            }))),
            "kain_runner" => Ok(BuildValue::Task(TaskValue::KainRunner(KainRunnerSpec {
                common: KainBuildTaskSection {
                    id: first_string_arg(name, &args)?,
                    kind: "exec".to_string(),
                    command: Some("kain".to_string()),
                    ..KainBuildTaskSection::default()
                },
            }))),
            "album_mode" => Ok(BuildValue::Task(TaskValue::AlbumMode(AlbumModeSpec {
                common: KainBuildTaskSection {
                    id: first_string_arg(name, &args)?,
                    kind: "exec".to_string(),
                    command: Some("powershell".to_string()),
                    cwd: Some(PathBuf::from("..")),
                    ..KainBuildTaskSection::default()
                },
                mode: None,
                runner: None,
                executable: None,
                output_dir: None,
                extra_args: Vec::new(),
            }))),
            "capsule_set" => {
                let name = first_string_arg(name, &args)?;
                Ok(BuildValue::Task(TaskValue::CapsuleSet(CapsuleSetSpec {
                    name: name.clone(),
                    common: KainBuildTaskSection {
                        id: name,
                        kind: "amalgamate".to_string(),
                        ..KainBuildTaskSection::default()
                    },
                    source_output: None,
                    artifacts_output: None,
                    evidence_output: None,
                })))
            }
            "certify" => {
                let subject = first_string_arg(name, &args)?;
                Ok(BuildValue::Task(TaskValue::Sections(vec![
                    KainBuildTaskSection {
                        id: format!("certify-{}", sanitize_id(&subject)),
                        kind: "certify".to_string(),
                        certifies: vec![subject],
                        ..KainBuildTaskSection::default()
                    },
                ])))
            }
            _ if simple_task_kind(name).is_some() => {
                let kind = simple_task_kind(name).unwrap_or_default().to_string();
                Ok(BuildValue::Task(TaskValue::Sections(vec![
                    KainBuildTaskSection {
                        id: first_string_arg(name, &args)?,
                        kind,
                        ..KainBuildTaskSection::default()
                    },
                ])))
            }
            _ => {
                let Some(function) = self.functions.get(name).cloned() else {
                    return Err(EvaluatedBuildError::Message(format!(
                        "unknown build.kn function '{name}'"
                    )));
                };
                self.call_function(&function, args)
            }
        }
    }

    fn call_callable(
        &mut self,
        callable: BuildValue,
        args: Vec<BuildValue>,
    ) -> EvalResult<BuildValue> {
        match callable {
            BuildValue::Function(name) => {
                let Some(function) = self.functions.get(&name).cloned() else {
                    return Err(EvaluatedBuildError::Message(format!(
                        "unknown build.kn function '{name}'"
                    )));
                };
                self.call_function(&function, args)
            }
            BuildValue::Lambda(lambda) => self.call_lambda(&lambda, args),
            other => Err(EvaluatedBuildError::Message(format!(
                "expected callable, got {}",
                other.type_name()
            ))),
        }
    }

    fn call_map(&mut self, args: Vec<BuildValue>) -> EvalResult<BuildValue> {
        if args.len() != 2 {
            return Err(EvaluatedBuildError::Message(
                "map(values, callback) expects exactly two arguments".to_string(),
            ));
        }
        let BuildValue::Array(values) = args[0].clone() else {
            return Err(EvaluatedBuildError::Message(
                "map(values, callback) requires an array as the first argument".to_string(),
            ));
        };
        let callback = args[1].clone();
        let mut mapped = Vec::new();
        for value in values {
            mapped.push(self.call_callable(callback.clone(), vec![value])?);
        }
        Ok(BuildValue::Array(mapped))
    }

    fn apply_method(
        &mut self,
        receiver: BuildValue,
        method: &str,
        args: &[CallArg],
    ) -> EvalResult<BuildValue> {
        let values = self.eval_call_args(args)?;
        match receiver {
            BuildValue::Project(mut project) => {
                apply_project_method(&mut project, method, &values)?;
                Ok(BuildValue::Project(project))
            }
            BuildValue::Package(mut package) => {
                apply_package_method(&mut package, method, &values)?;
                Ok(BuildValue::Package(package))
            }
            BuildValue::Blade(mut blade) => {
                apply_blade_method(&mut blade, method, &values)?;
                Ok(BuildValue::Blade(blade))
            }
            BuildValue::BuildDefaults(mut defaults) => {
                apply_build_defaults_method(&mut defaults, method, &values)?;
                Ok(BuildValue::BuildDefaults(defaults))
            }
            BuildValue::RunDefaults(mut defaults) => {
                apply_run_defaults_method(&mut defaults, method, &values)?;
                Ok(BuildValue::RunDefaults(defaults))
            }
            BuildValue::Workspace(mut workspace) => {
                apply_workspace_method(&mut workspace, method, &values)?;
                Ok(BuildValue::Workspace(workspace))
            }
            BuildValue::SourceSet(mut source_set) => {
                self.apply_source_set_method(&mut source_set, method, &values)?;
                Ok(BuildValue::SourceSet(source_set))
            }
            BuildValue::PlatformPackage(mut package) => {
                apply_platform_package_method(&mut package, method, &values)?;
                Ok(BuildValue::PlatformPackage(package))
            }
            BuildValue::Task(mut task) => {
                self.apply_task_method(&mut task, method, &values)?;
                Ok(BuildValue::Task(task))
            }
            BuildValue::Graph(mut graph) => {
                self.apply_graph_method(&mut graph, method, &values)?;
                Ok(BuildValue::Graph(graph))
            }
            other => Err(EvaluatedBuildError::Message(format!(
                "method '.{method}(...)' is not supported on {}",
                other.type_name()
            ))),
        }
    }

    fn build_graph_from_args(&self, args: &[BuildValue]) -> EvalResult<GraphSpec> {
        let mut graph = GraphSpec::default();
        for value in args {
            apply_graph_value(&mut graph, value, self.workspace_root)?;
        }
        Ok(graph)
    }

    fn apply_graph_method(
        &self,
        graph: &mut GraphSpec,
        method: &str,
        values: &[BuildValue],
    ) -> EvalResult<()> {
        match method {
            "project" | "package" | "blade" | "defaults" | "run" | "workspace"
            | "platform_package" => {
                for value in values {
                    apply_graph_value(graph, value, self.workspace_root)?;
                }
            }
            "task" | "tasks" => {
                for task in flatten_task_sections(values, self.workspace_root)? {
                    push_unique_task(&mut graph.tasks, task);
                }
            }
            "sources" | "source" => {
                for value in values {
                    if let BuildValue::SourceSet(source_set) = value {
                        let _ = source_set.paths(self.workspace_root)?;
                    }
                }
            }
            _ => {
                return Err(EvaluatedBuildError::Message(format!(
                    "unsupported build_graph().{method}(...) method"
                )));
            }
        }
        Ok(())
    }

    fn apply_source_set_method(
        &self,
        source_set: &mut SourceSetSpec,
        method: &str,
        values: &[BuildValue],
    ) -> EvalResult<()> {
        match method {
            "root" | "roots" => push_unique_paths(&mut source_set.roots, &values_to_paths(values, self.workspace_root)?),
            "file" | "files" => push_unique_paths(&mut source_set.files, &values_to_paths(values, self.workspace_root)?),
            "dir" | "dirs" => push_unique_paths(&mut source_set.dirs, &values_to_paths(values, self.workspace_root)?),
            "glob" | "globs" => push_unique_strings(&mut source_set.globs, &values_to_strings(values)?),
            "exclude" | "excludes" => {
                push_unique_strings(&mut source_set.excludes, &values_to_strings(values)?)
            }
            _ => {
                return Err(EvaluatedBuildError::Message(format!(
                    "unsupported source_set().{method}(...) method"
                )));
            }
        }
        Ok(())
    }

    fn apply_task_method(
        &self,
        task: &mut TaskValue,
        method: &str,
        values: &[BuildValue],
    ) -> EvalResult<()> {
        match task {
            TaskValue::Sections(sections) => apply_section_task_method(
                sections,
                method,
                values,
                self.workspace_root,
                None,
            ),
            TaskValue::GpuSuite(spec) => apply_gpu_suite_method(spec, method, values, self.workspace_root),
            TaskValue::CudaArtifacts(spec) => {
                apply_cuda_artifacts_method(spec, method, values, self.workspace_root)
            }
            TaskValue::KainRunner(spec) => apply_kain_runner_method(spec, method, values, self.workspace_root),
            TaskValue::AlbumMode(spec) => apply_album_mode_method(spec, method, values, self.workspace_root),
            TaskValue::CapsuleSet(spec) => apply_capsule_set_method(spec, method, values, self.workspace_root),
        }
    }
}

fn apply_project_method(
    project: &mut ProjectSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "kind" => project.kind = first_optional_string(values)?,
        "version" => project.version = first_optional_string(values)?,
        "description" => project.description = first_optional_string(values)?,
        "entry" => project.entry = first_optional_path(values)?,
        "source_root" | "source_roots" => {
            push_unique_paths(&mut project.source_roots, &values_to_paths(values, Path::new(""))?)
        }
        "module_root" | "module_roots" => {
            push_unique_paths(&mut project.module_roots, &values_to_paths(values, Path::new(""))?)
        }
        "generated_root" => project.generated_root = first_optional_path(values)?,
        "target" | "targets" | "build_target" | "build_targets" => {
            push_unique_strings(&mut project.targets, &values_to_strings(values)?)
        }
        "artifact_root" => project.artifact_root = first_optional_path(values)?,
        "cache_root" => project.cache_root = first_optional_path(values)?,
        "profile" => project.profile = first_optional_string(values)?,
        "run_arg" | "run_args" | "arg" | "args" => {
            push_unique_strings(&mut project.run_args, &values_to_strings(values)?)
        }
        "watch" => push_unique_paths(&mut project.watch, &values_to_paths(values, Path::new(""))?),
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported project().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_package_method(
    package: &mut PackageSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "version" => package.version = first_optional_string(values)?,
        "description" => package.description = first_optional_string(values)?,
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported package().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_blade_method(
    blade: &mut BladeSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "kind" => blade.kind = first_optional_string(values)?,
        "version" => blade.version = first_optional_string(values)?,
        "entry" => blade.entry = first_optional_path(values)?,
        "source_root" | "source_roots" => {
            push_unique_paths(&mut blade.source_roots, &values_to_paths(values, Path::new(""))?)
        }
        "module_root" | "module_roots" => {
            push_unique_paths(&mut blade.module_roots, &values_to_paths(values, Path::new(""))?)
        }
        "build_target" | "build_targets" | "target" | "targets" => {
            push_unique_strings(&mut blade.build_targets, &values_to_strings(values)?)
        }
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported blade().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_build_defaults_method(
    defaults: &mut BuildDefaultsSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "entry" => defaults.entry = first_optional_path(values)?,
        "entry_module" => defaults.entry_module = first_optional_string(values)?,
        "source_root" => defaults.source_root = first_optional_path(values)?,
        "source_order" => push_unique_paths(&mut defaults.source_order, &values_to_paths(values, Path::new(""))?),
        "module_root" | "module_roots" => {
            push_unique_paths(&mut defaults.module_roots, &values_to_paths(values, Path::new(""))?)
        }
        "module_search_path" | "module_search_paths" => push_unique_paths(
            &mut defaults.module_search_paths,
            &values_to_paths(values, Path::new(""))?,
        ),
        "target" | "targets" => push_unique_strings(&mut defaults.targets, &values_to_strings(values)?),
        "artifact_root" => defaults.artifact_root = first_optional_path(values)?,
        "cache_root" => defaults.cache_root = first_optional_path(values)?,
        "profile" => defaults.profile = first_optional_string(values)?,
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported build_defaults().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_run_defaults_method(
    defaults: &mut RunDefaultsSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "entry" => defaults.entry = first_optional_path(values)?,
        "blade" => defaults.blade = first_optional_string(values)?,
        "target" => defaults.target = first_optional_string(values)?,
        "arg" | "args" => push_unique_strings(&mut defaults.args, &values_to_strings(values)?),
        "env" => insert_pair(values, &mut defaults.env)?,
        "cwd" => defaults.cwd = first_optional_path(values)?,
        "watch" => push_unique_paths(&mut defaults.watch, &values_to_paths(values, Path::new(""))?),
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported run_defaults().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_workspace_method(
    workspace: &mut WorkspaceSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "blade_pattern" | "blades" => {
            push_unique_paths(&mut workspace.blades, &values_to_paths(values, Path::new(""))?)
        }
        "blade_root" | "blade_roots" => {
            push_unique_paths(&mut workspace.blade_roots, &values_to_paths(values, Path::new(""))?)
        }
        "member" | "members" => {
            push_unique_paths(&mut workspace.members, &values_to_paths(values, Path::new(""))?)
        }
        "search_root" | "search_roots" => {
            push_unique_paths(&mut workspace.search_roots, &values_to_paths(values, Path::new(""))?)
        }
        "stdlib_root" => workspace.stdlib_root = first_optional_path(values)?,
        "manifest_root" => workspace.manifest_root = first_optional_path(values)?,
        "generated_root" => workspace.generated_root = first_optional_path(values)?,
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported workspace_defaults().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_platform_package_method(
    package: &mut PlatformPackageSpec,
    method: &str,
    values: &[BuildValue],
) -> EvalResult<()> {
    match method {
        "provider" => {
            package.provider = first_string_arg("provider", values)?;
        }
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported platform_package().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_section_task_method(
    sections: &mut [KainBuildTaskSection],
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
    project: Option<&ProjectSpec>,
) -> EvalResult<()> {
    for section in sections {
        apply_single_section_task_method(section, method, values, workspace_root, project)?;
    }
    Ok(())
}

fn apply_single_section_task_method(
    section: &mut KainBuildTaskSection,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
    project: Option<&ProjectSpec>,
) -> EvalResult<()> {
    match method {
        "kind" => section.kind = first_string_arg("kind", values)?,
        "project" => {
            if let Some(BuildValue::Project(project)) = values.first() {
                apply_project_to_task(section, project);
            } else if let Some(project) = project {
                apply_project_to_task(section, project);
            }
        }
        "blade" => section.blade = first_optional_string(values)?,
        "entry" | "source" | "path" => section.entry = first_optional_path(values)?,
        "manifest" => section.manifest = first_optional_path(values)?,
        "command" => section.command = first_optional_string(values)?,
        "arg" | "args" => section.args.extend(values_to_strings(values)?),
        "cwd" => section.cwd = first_optional_path(values)?,
        "target" => section.target = first_optional_string(values)?,
        "profile" => section.profile = first_optional_string(values)?,
        "input" | "inputs" => push_unique_paths(
            &mut section.inputs,
            &values_to_paths(values, workspace_root)?,
        ),
        "output" | "outputs" | "root_output" | "blade_output" | "artifact" | "produces" => {
            push_unique_paths(&mut section.outputs, &values_to_paths(values, workspace_root)?)
        }
        "depends_on" | "depends" | "dependency" | "requires" | "requires_task" | "after" => {
            push_unique_strings(&mut section.depends_on, &values_to_task_ids(values, workspace_root)?)
        }
        "requires_capability" | "when_capability" | "capability" => {
            push_unique_strings(&mut section.required_capabilities, &values_to_strings(values)?)
        }
        "axis" | "matrix_axis" | "matrix_value" | "matrix" => {
            push_unique_strings(&mut section.matrix_axes, &canonical_matrix_axis_values(values_to_strings(values)?))
        }
        "telemetry" | "telemetry_channel" => {
            push_unique_strings(&mut section.telemetry, &values_to_strings(values)?)
        }
        "certifies" | "certificate" => {
            push_unique_strings(&mut section.certifies, &values_to_strings(values)?)
        }
        "env" => insert_pair(values, &mut section.env)?,
        "meta" => insert_pair(values, &mut section.meta)?,
        "option" => insert_pair(values, &mut section.options)?,
        "tag" => push_unique_strings(&mut section.tags, &values_to_strings(values)?),
        "note" => push_unique_strings(&mut section.notes, &values_to_strings(values)?),
        "author" => push_unique_strings(&mut section.authors, &values_to_strings(values)?),
        "name" | "version" | "storage" | "contents" | "capsule_set" | "header"
        | "compression" | "preview_symbols" | "api_index" | "module_index" | "timeout_ms"
        | "stdout" | "stderr" => {
            if let Some(value) = values_to_strings(values)?.first() {
                section.options.insert(method.to_string(), value.clone());
            }
        }
        "archive" => {
            let enabled = values
                .first()
                .map(value_to_bool)
                .transpose()?
                .unwrap_or(true);
            section.options.insert(
                "storage".to_string(),
                if enabled { "archive" } else { "editable" }.to_string(),
            );
        }
        "editable" => {
            section
                .options
                .insert("storage".to_string(), "editable".to_string());
        }
        "always_run" => {
            section
                .options
                .insert("always_run".to_string(), "true".to_string());
        }
        "proof_mode" | "mode" => section.args.extend(values_to_strings(values)?),
        "artifact_root" | "output_dir" | "stem" => {
            if let Some(value) = values_to_strings(values)?.first() {
                section.options.insert(method.to_string(), value.clone());
            }
        }
        _ => {
            return Err(EvaluatedBuildError::Message(format!(
                "unsupported task().{method}(...) method"
            )));
        }
    }
    Ok(())
}

fn apply_gpu_suite_method(
    spec: &mut GpuSuiteSpec,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<()> {
    match method {
        "fragment" => {
            spec.fragment = first_optional_path(values)?;
            if let Some(path) = &spec.fragment {
                push_unique_path(&mut spec.common.inputs, path.clone());
            }
        }
        "compute" => {
            spec.compute = first_optional_path(values)?;
            if let Some(path) = &spec.compute {
                push_unique_path(&mut spec.common.inputs, path.clone());
            }
        }
        "target" | "targets" => push_unique_strings(&mut spec.targets, &values_to_strings(values)?),
        "artifact_root" | "output_dir" => spec.artifact_root = first_optional_path(values)?,
        _ => apply_single_section_task_method(&mut spec.common, method, values, workspace_root, None)?,
    }
    Ok(())
}

fn apply_cuda_artifacts_method(
    spec: &mut CudaArtifactsSpec,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<()> {
    match method {
        "entry" | "source" | "path" => {
            spec.common.entry = first_optional_path(values)?;
            if let Some(path) = &spec.common.entry {
                push_unique_path(&mut spec.common.inputs, path.clone());
            }
        }
        "stem" => spec.stem = first_optional_string(values)?,
        "output_dir" | "artifact_root" => spec.output_dir = first_optional_path(values)?,
        "outputs" => push_unique_strings(&mut spec.output_roles, &values_to_strings(values)?),
        _ => apply_single_section_task_method(&mut spec.common, method, values, workspace_root, None)?,
    }
    Ok(())
}

fn apply_kain_runner_method(
    spec: &mut KainRunnerSpec,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<()> {
    match method {
        "entry" | "source" | "path" => {
            spec.common.entry = first_optional_path(values)?;
            if let Some(path) = &spec.common.entry {
                spec.common.args = vec!["check".to_string(), path.display().to_string()];
                push_unique_path(&mut spec.common.inputs, path.clone());
            }
        }
        "target" => {
            spec.common.target = first_optional_string(values)?;
            if let Some(target) = &spec.common.target {
                spec.common.args.extend(["--target".to_string(), target.clone()]);
            }
        }
        _ => apply_single_section_task_method(&mut spec.common, method, values, workspace_root, None)?,
    }
    Ok(())
}

fn apply_album_mode_method(
    spec: &mut AlbumModeSpec,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<()> {
    match method {
        "mode" => spec.mode = first_optional_string(values)?,
        "runner" => {
            spec.runner = first_optional_path(values)?;
            if let Some(path) = &spec.runner {
                push_unique_path(&mut spec.common.inputs, path.clone());
            }
        }
        "executable" => spec.executable = first_optional_path(values)?,
        "output_dir" => spec.output_dir = first_optional_path(values)?,
        "arg" | "args" => spec.extra_args.extend(values_to_strings(values)?),
        _ => apply_single_section_task_method(&mut spec.common, method, values, workspace_root, None)?,
    }
    Ok(())
}

fn apply_capsule_set_method(
    spec: &mut CapsuleSetSpec,
    method: &str,
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<()> {
    match method {
        "source" => spec.source_output = first_optional_path(values)?,
        "artifacts" => spec.artifacts_output = first_optional_path(values)?,
        "evidence" => spec.evidence_output = first_optional_path(values)?,
        "after" => push_unique_strings(&mut spec.common.depends_on, &values_to_task_ids(values, workspace_root)?),
        _ => apply_single_section_task_method(&mut spec.common, method, values, workspace_root, None)?,
    }
    Ok(())
}

fn apply_project_to_task(section: &mut KainBuildTaskSection, project: &ProjectSpec) {
    if section.entry.is_none() {
        section.entry = project.entry.clone();
    }
    if section.target.is_none() && project.targets.len() == 1 {
        section.target = project.targets.first().cloned();
    }
    for root in &project.source_roots {
        push_unique_path(&mut section.inputs, root.clone());
    }
}

fn apply_graph_value(graph: &mut GraphSpec, value: &BuildValue, workspace_root: &Path) -> EvalResult<()> {
    match value {
        BuildValue::Project(project) => merge_project_into_manifest(&mut graph.manifest, project),
        BuildValue::Package(package) => merge_package_into_manifest(&mut graph.manifest, package),
        BuildValue::Blade(blade) => merge_blade_into_manifest(&mut graph.manifest, blade),
        BuildValue::BuildDefaults(defaults) => merge_build_defaults_into_manifest(&mut graph.manifest, defaults),
        BuildValue::RunDefaults(defaults) => merge_run_defaults_into_manifest(&mut graph.manifest, defaults),
        BuildValue::Workspace(workspace) => merge_workspace_into_manifest(&mut graph.manifest, workspace),
        BuildValue::PlatformPackage(package) => graph.platform_packages.push(KainBuildGraphPlatformPackage {
            package: package.package.clone(),
            provider: package.provider.clone(),
            source: package.source.clone(),
        }),
        BuildValue::Task(_) | BuildValue::Array(_) => {
            for task in flatten_task_sections(std::slice::from_ref(value), workspace_root)? {
                push_unique_task(&mut graph.tasks, task);
            }
        }
        BuildValue::SourceSet(_) | BuildValue::Unit => {}
        other => {
            return Err(EvaluatedBuildError::Message(format!(
                "cannot attach {} to build_graph",
                other.type_name()
            )));
        }
    }
    Ok(())
}

fn merge_project_into_manifest(manifest: &mut KainManifest, project: &ProjectSpec) {
    manifest.package.name = Some(project.name.clone());
    manifest.package.version = project.version.clone();
    manifest.package.description = project.description.clone();
    manifest.blade.name = Some(project.name.clone());
    manifest.blade.version = project.version.clone();
    manifest.blade.kind = project.kind.clone();
    manifest.blade.entry = project.entry.clone();
    replace_if_not_empty(&mut manifest.blade.source_roots, project.source_roots.clone());
    replace_if_not_empty(&mut manifest.blade.module_roots, project.module_roots.clone());
    replace_if_not_empty(&mut manifest.blade.build_targets, project.targets.clone());
    manifest.build.entry = project.entry.clone();
    if let Some(root) = project.source_roots.first() {
        manifest.build.source_root = Some(root.clone());
    }
    replace_if_not_empty(&mut manifest.build.module_roots, project.module_roots.clone());
    replace_if_not_empty(&mut manifest.build.targets, project.targets.clone());
    manifest.build.artifact_root = project.artifact_root.clone();
    manifest.build.cache_root = project.cache_root.clone();
    manifest.build.profile = project.profile.clone();
    manifest.run.entry = project.entry.clone();
    if project.targets.len() == 1 {
        manifest.run.target = project.targets.first().cloned();
    }
    replace_if_not_empty(&mut manifest.run.args, project.run_args.clone());
    replace_if_not_empty(&mut manifest.run.watch, project.watch.clone());
    manifest.workspace.generated_root = project.generated_root.clone();
}

fn merge_package_into_manifest(manifest: &mut KainManifest, package: &PackageSpec) {
    manifest.package.name = Some(package.name.clone());
    manifest.package.version = package.version.clone();
    manifest.package.description = package.description.clone();
}

fn merge_blade_into_manifest(manifest: &mut KainManifest, blade: &BladeSpec) {
    manifest.blade.name = Some(blade.name.clone());
    manifest.blade.kind = blade.kind.clone();
    manifest.blade.version = blade.version.clone();
    manifest.blade.entry = blade.entry.clone();
    replace_if_not_empty(&mut manifest.blade.source_roots, blade.source_roots.clone());
    replace_if_not_empty(&mut manifest.blade.module_roots, blade.module_roots.clone());
    replace_if_not_empty(&mut manifest.blade.build_targets, blade.build_targets.clone());
}

fn merge_build_defaults_into_manifest(manifest: &mut KainManifest, defaults: &BuildDefaultsSpec) {
    manifest.build.entry = defaults.entry.clone();
    manifest.build.entry_module = defaults.entry_module.clone();
    manifest.build.source_root = defaults.source_root.clone();
    replace_if_not_empty(&mut manifest.build.source_order, defaults.source_order.clone());
    replace_if_not_empty(&mut manifest.build.module_roots, defaults.module_roots.clone());
    replace_if_not_empty(
        &mut manifest.build.module_search_paths,
        defaults.module_search_paths.clone(),
    );
    replace_if_not_empty(&mut manifest.build.targets, defaults.targets.clone());
    manifest.build.artifact_root = defaults.artifact_root.clone();
    manifest.build.cache_root = defaults.cache_root.clone();
    manifest.build.profile = defaults.profile.clone();
}

fn merge_run_defaults_into_manifest(manifest: &mut KainManifest, defaults: &RunDefaultsSpec) {
    manifest.run.entry = defaults.entry.clone();
    manifest.run.blade = defaults.blade.clone();
    manifest.run.target = defaults.target.clone();
    replace_if_not_empty(&mut manifest.run.args, defaults.args.clone());
    if !defaults.env.is_empty() {
        manifest.run.env = defaults.env.clone();
    }
    manifest.run.cwd = defaults.cwd.clone();
    replace_if_not_empty(&mut manifest.run.watch, defaults.watch.clone());
}

fn merge_workspace_into_manifest(manifest: &mut KainManifest, workspace: &WorkspaceSpec) {
    replace_if_not_empty(&mut manifest.workspace.blades, workspace.blades.clone());
    replace_if_not_empty(&mut manifest.workspace.blade_roots, workspace.blade_roots.clone());
    replace_if_not_empty(&mut manifest.workspace.members, workspace.members.clone());
    replace_if_not_empty(&mut manifest.workspace.search_roots, workspace.search_roots.clone());
    manifest.workspace.stdlib_root = workspace.stdlib_root.clone();
    manifest.workspace.manifest_root = workspace.manifest_root.clone();
    manifest.workspace.generated_root = workspace.generated_root.clone();
}

fn replace_if_not_empty<T>(slot: &mut Vec<T>, values: Vec<T>) {
    if !values.is_empty() {
        *slot = values;
    }
}

fn flatten_task_sections(
    values: &[BuildValue],
    workspace_root: &Path,
) -> EvalResult<Vec<KainBuildTaskSection>> {
    let mut output = Vec::new();
    for value in values {
        match value {
            BuildValue::Task(task) => output.extend(task.to_sections(workspace_root)?),
            BuildValue::Array(values) => output.extend(flatten_task_sections(values, workspace_root)?),
            BuildValue::String(value) => output.push(KainBuildTaskSection {
                id: value.clone(),
                ..KainBuildTaskSection::default()
            }),
            BuildValue::Unit => {}
            other => {
                return Err(EvaluatedBuildError::Message(format!(
                    "expected task value, got {}",
                    other.type_name()
                )));
            }
        }
    }
    Ok(output)
}

impl TaskValue {
    fn to_sections(&self, workspace_root: &Path) -> EvalResult<Vec<KainBuildTaskSection>> {
        match self {
            TaskValue::Sections(sections) => Ok(sections.clone()),
            TaskValue::GpuSuite(spec) => Ok(spec.to_sections()),
            TaskValue::CudaArtifacts(spec) => Ok(vec![spec.to_section()]),
            TaskValue::KainRunner(spec) => Ok(vec![spec.to_section()]),
            TaskValue::AlbumMode(spec) => Ok(vec![spec.to_section()]),
            TaskValue::CapsuleSet(spec) => spec.to_sections(workspace_root),
        }
    }

    fn ids(&self, workspace_root: &Path) -> EvalResult<Vec<String>> {
        Ok(self
            .to_sections(workspace_root)?
            .into_iter()
            .map(|section| section.id)
            .collect())
    }
}

impl GpuSuiteSpec {
    fn to_sections(&self) -> Vec<KainBuildTaskSection> {
        let targets = if self.targets.is_empty() {
            vec!["all".to_string()]
        } else {
            self.targets.clone()
        };
        let mut sections = Vec::new();
        for target in targets {
            let mut section = self.common.clone();
            section.id = format!("{}-{}", self.id, sanitize_id(&target));
            section.kind = "gpu".to_string();
            section.target = Some(target.clone());
            section
                .options
                .insert("target".to_string(), target.clone());
            section.entry = if target == "cuda" || target == "ptx" {
                self.compute.clone().or_else(|| self.fragment.clone())
            } else {
                self.fragment.clone().or_else(|| self.compute.clone())
            };
            if let Some(root) = &self.artifact_root {
                section
                    .outputs
                    .push(root.join(sanitize_id(&target)).join(&section.id));
            }
            if let Some(entry) = &section.entry {
                push_unique_path(&mut section.inputs, entry.clone());
            }
            sections.push(section);
        }
        sections
    }
}

impl CudaArtifactsSpec {
    fn to_section(&self) -> KainBuildTaskSection {
        let mut section = self.common.clone();
        section.kind = "exec".to_string();
        section.command = Some("kain".to_string());
        let entry = section
            .entry
            .clone()
            .unwrap_or_else(|| PathBuf::from("src/main.kn"));
        let stem = self
            .stem
            .clone()
            .unwrap_or_else(|| path_stem_or_name(&entry));
        let output_dir = self
            .output_dir
            .clone()
            .unwrap_or_else(|| PathBuf::from(".kain/oracle/gpu").join(&stem));
        let output_base = output_dir.join(&stem);
        section.args = vec![
            "gpu-artifacts".to_string(),
            entry.display().to_string(),
            "--output".to_string(),
            output_base.display().to_string(),
            "--target".to_string(),
            "cuda".to_string(),
        ];
        let roles = if self.output_roles.is_empty() {
            vec![
                "ptx".to_string(),
                "gpu_rs".to_string(),
                "reflection".to_string(),
                "shader_bundle".to_string(),
                "residency".to_string(),
            ]
        } else {
            self.output_roles.clone()
        };
        for output in cuda_artifact_outputs(&output_dir, &output_base, &roles) {
            push_unique_path(&mut section.outputs, output);
        }
        section
    }
}

impl KainRunnerSpec {
    fn to_section(&self) -> KainBuildTaskSection {
        let mut section = self.common.clone();
        section.kind = "exec".to_string();
        section.command.get_or_insert_with(|| "kain".to_string());
        if section.args.is_empty() {
            if let Some(entry) = &section.entry {
                section.args.push("check".to_string());
                section.args.push(entry.display().to_string());
                if let Some(target) = &section.target {
                    section.args.push("--target".to_string());
                    section.args.push(target.clone());
                }
            }
        }
        section
    }
}

impl AlbumModeSpec {
    fn to_section(&self) -> KainBuildTaskSection {
        let mut section = self.common.clone();
        let mode = self
            .mode
            .clone()
            .unwrap_or_else(|| section.id.trim_start_matches("album-").to_string());
        let runner = self
            .runner
            .clone()
            .unwrap_or_else(|| PathBuf::from("telemetry/run_smoketest_mode.kn"));
        let executable = self
            .executable
            .clone()
            .unwrap_or_else(|| PathBuf::from("$root/smoketest.exe"));
        let output_dir = self
            .output_dir
            .clone()
            .unwrap_or_else(|| PathBuf::from("$root/telemetry").join(&mode));
        section.args = vec![
            "-NoProfile".to_string(),
            "-ExecutionPolicy".to_string(),
            "Bypass".to_string(),
            "-File".to_string(),
            "$root/telemetry/invoke_kain.ps1".to_string(),
            "run".to_string(),
            runner.display().to_string(),
            "--target".to_string(),
            "interpret".to_string(),
            "--".to_string(),
            "--mode".to_string(),
            mode,
            "--executable".to_string(),
            executable.display().to_string(),
            "--output-dir".to_string(),
            output_dir.display().to_string(),
        ];
        section.args.extend(self.extra_args.clone());
        push_unique_path(&mut section.inputs, runner);
        push_unique_path(&mut section.outputs, output_dir);
        section
    }
}

impl CapsuleSetSpec {
    fn to_sections(&self, _workspace_root: &Path) -> EvalResult<Vec<KainBuildTaskSection>> {
        let mut sections = Vec::new();
        let source = self
            .source_output
            .clone()
            .unwrap_or_else(|| PathBuf::from(format!("$root/{}.kn", self.name)));
        let artifacts = self
            .artifacts_output
            .clone()
            .unwrap_or_else(|| PathBuf::from(format!("$root/{}.artifacts.kn", self.name)));
        let evidence = self
            .evidence_output
            .clone()
            .unwrap_or_else(|| PathBuf::from(format!("$root/{}.evidence.kn", self.name)));
        for (suffix, output, contents, header, preview) in [
            ("capsule", source, "source", "rich", "96"),
            ("capsule-artifacts", artifacts, "artifacts", "minimal", "0"),
            ("capsule-evidence", evidence, "evidence", "minimal", "0"),
        ] {
            let mut section = self.common.clone();
            section.id = format!("{}-{}", self.name, suffix);
            section.kind = "amalgamate".to_string();
            section.entry = Some(PathBuf::from("."));
            section.outputs = vec![output];
            section
                .options
                .insert("name".to_string(), format!("{}-{}", self.name, suffix));
            section
                .options
                .insert("contents".to_string(), contents.to_string());
            section
                .options
                .insert("capsule_set".to_string(), self.name.clone());
            section
                .options
                .insert("header".to_string(), header.to_string());
            section
                .options
                .insert("preview_symbols".to_string(), preview.to_string());
            section
                .options
                .entry("storage".to_string())
                .or_insert_with(|| "editable".to_string());
            sections.push(section);
        }
        Ok(sections)
    }
}

impl SourceSetSpec {
    fn paths(&self, workspace_root: &Path) -> EvalResult<Vec<PathBuf>> {
        let mut output = Vec::new();
        push_unique_paths(&mut output, &self.files);
        push_unique_paths(&mut output, &self.dirs);
        for pattern in &self.globs {
            for candidate in expand_source_glob(workspace_root, &self.roots, pattern) {
                if !self.excludes.iter().any(|exclude| glob_match(exclude, &candidate)) {
                    push_unique_path(&mut output, PathBuf::from(candidate));
                }
            }
        }
        Ok(output)
    }
}

impl BuildValue {
    fn as_string(&self) -> EvalResult<String> {
        match self {
            BuildValue::String(value) => Ok(value.clone()),
            BuildValue::Int(value) => Ok(value.to_string()),
            BuildValue::Bool(value) => Ok(value.to_string()),
            BuildValue::Function(value) => Ok(value.clone()),
            BuildValue::Project(project) => Ok(project.name.clone()),
            BuildValue::Package(package) => Ok(package.name.clone()),
            BuildValue::Blade(blade) => Ok(blade.name.clone()),
            BuildValue::SourceSet(source_set) => Ok(source_set.name.clone()),
            BuildValue::Task(task) => {
                let ids = task.ids(Path::new(""))?;
                Ok(ids.first().cloned().unwrap_or_default())
            }
            other => Err(EvaluatedBuildError::Message(format!(
                "expected string-like value, got {}",
                other.type_name()
            ))),
        }
    }

    fn type_name(&self) -> &'static str {
        match self {
            BuildValue::Unit => "Unit",
            BuildValue::Bool(_) => "Bool",
            BuildValue::Int(_) => "Int",
            BuildValue::String(_) => "String",
            BuildValue::Array(_) => "Array",
            BuildValue::Function(_) => "Function",
            BuildValue::Lambda(_) => "Lambda",
            BuildValue::Project(_) => "Project",
            BuildValue::Package(_) => "Package",
            BuildValue::Blade(_) => "Blade",
            BuildValue::BuildDefaults(_) => "BuildDefaults",
            BuildValue::RunDefaults(_) => "RunDefaults",
            BuildValue::Workspace(_) => "Workspace",
            BuildValue::SourceSet(_) => "SourceSet",
            BuildValue::PlatformPackage(_) => "PlatformPackage",
            BuildValue::Task(_) => "BuildTask",
            BuildValue::Graph(_) => "BuildGraph",
        }
    }
}

fn simple_task_kind(name: &str) -> Option<&'static str> {
    match name {
        "build_task" => Some(""),
        "build_check" | "check_task" => Some("check"),
        "exec_task" | "command_task" => Some("exec"),
        "amalgamate_capsule" | "capsule_task" => Some("amalgamate"),
        "native_executable" | "root_executable" | "build_native_executable" => {
            Some("native-executable")
        }
        "test_task" | "test_suite" | "source_tests" => Some("test"),
        "proof_task" | "proof_obligation" | "z3_proof" => Some("proof"),
        "bench_task" | "bench_case" | "benchmark_task" => Some("benchmark"),
        "attrition_task" | "attrition_case" => Some("attrition"),
        "certify_task" | "certify_gate" | "release_gate" => Some("certify"),
        _ => None,
    }
}

fn first_string_arg(function: &str, values: &[BuildValue]) -> EvalResult<String> {
    values
        .first()
        .ok_or_else(|| {
            EvaluatedBuildError::Message(format!("{function}(...) requires a first argument"))
        })?
        .as_string()
}

fn first_optional_string(values: &[BuildValue]) -> EvalResult<Option<String>> {
    values.first().map(BuildValue::as_string).transpose()
}

fn first_optional_path(values: &[BuildValue]) -> EvalResult<Option<PathBuf>> {
    Ok(first_optional_string(values)?.map(PathBuf::from))
}

fn values_to_strings(values: &[BuildValue]) -> EvalResult<Vec<String>> {
    let mut output = Vec::new();
    for value in values {
        match value {
            BuildValue::Array(items) => output.extend(values_to_strings(items)?),
            BuildValue::Unit => {}
            other => output.push(other.as_string()?),
        }
    }
    Ok(output)
}

fn values_to_paths(values: &[BuildValue], workspace_root: &Path) -> EvalResult<Vec<PathBuf>> {
    let mut output = Vec::new();
    for value in values {
        match value {
            BuildValue::Array(items) => output.extend(values_to_paths(items, workspace_root)?),
            BuildValue::SourceSet(source_set) => output.extend(source_set.paths(workspace_root)?),
            BuildValue::Unit => {}
            other => output.push(PathBuf::from(other.as_string()?)),
        }
    }
    Ok(output)
}

fn values_to_task_ids(values: &[BuildValue], workspace_root: &Path) -> EvalResult<Vec<String>> {
    let mut output = Vec::new();
    for value in values {
        match value {
            BuildValue::Array(items) => output.extend(values_to_task_ids(items, workspace_root)?),
            BuildValue::Task(task) => output.extend(task.ids(workspace_root)?),
            BuildValue::String(value) => output.push(value.clone()),
            BuildValue::Unit => {}
            other => output.push(other.as_string()?),
        }
    }
    Ok(output)
}

fn value_to_bool(value: &BuildValue) -> EvalResult<bool> {
    match value {
        BuildValue::Bool(value) => Ok(*value),
        BuildValue::Int(value) => Ok(*value != 0),
        BuildValue::String(value) => Ok(matches!(
            value.trim().to_ascii_lowercase().as_str(),
            "1" | "true" | "yes" | "on"
        )),
        other => Err(EvaluatedBuildError::Message(format!(
            "expected bool-like value, got {}",
            other.type_name()
        ))),
    }
}

fn insert_pair(values: &[BuildValue], slot: &mut BTreeMap<String, String>) -> EvalResult<()> {
    let values = values_to_strings(values)?;
    if values.len() >= 2 {
        slot.insert(values[0].clone(), values[1].clone());
    }
    Ok(())
}

fn canonical_matrix_axis_values(values: Vec<String>) -> Vec<String> {
    if values.len() == 2 {
        vec![format!("{}={}", values[0], values[1])]
    } else {
        values
    }
}

fn push_unique_strings(slot: &mut Vec<String>, values: &[String]) {
    for value in values {
        if !slot.iter().any(|existing| existing == value) {
            slot.push(value.clone());
        }
    }
}

fn push_unique_paths(slot: &mut Vec<PathBuf>, values: &[PathBuf]) {
    for value in values {
        push_unique_path(slot, value.clone());
    }
}

fn push_unique_path(slot: &mut Vec<PathBuf>, value: PathBuf) {
    if !slot.iter().any(|existing| existing == &value) {
        slot.push(value);
    }
}

fn push_unique_task(slot: &mut Vec<KainBuildTaskSection>, task: KainBuildTaskSection) {
    if let Some(existing) = slot.iter_mut().find(|existing| existing.id == task.id) {
        *existing = task;
    } else {
        slot.push(task);
    }
}

fn sort_tasks_stably(mut tasks: Vec<KainBuildTaskSection>) -> Vec<KainBuildTaskSection> {
    let mut seen = BTreeSet::new();
    tasks.retain(|task| seen.insert(task.id.clone()));
    tasks
}

fn sort_platform_packages(
    mut packages: Vec<KainBuildGraphPlatformPackage>,
) -> Vec<KainBuildGraphPlatformPackage> {
    packages.sort_by(|left, right| {
        left.package
            .cmp(&right.package)
            .then(left.provider.cmp(&right.provider))
            .then(left.source.cmp(&right.source))
    });
    packages.dedup_by(|left, right| {
        left.package == right.package
            && left.provider == right.provider
            && left.source == right.source
    });
    packages
}

fn cuda_artifact_outputs(
    output_dir: &Path,
    output_base: &Path,
    roles: &[String],
) -> Vec<PathBuf> {
    let mut outputs = Vec::new();
    for role in roles {
        match role.trim().replace('-', "_").as_str() {
            "ptx" | "derived_ptx" => outputs.push(with_suffix(output_base, ".derived", "ptx")),
            "gpu_rs" | "gpu" | "rs" => outputs.push(with_suffix(output_base, ".gpu", "rs")),
            "reflection" | "reflect" | "reflect_json" => {
                outputs.push(with_suffix(output_base, ".reflect", "json"))
            }
            "shader_bundle" | "bundle" => {
                outputs.push(with_suffix(output_base, ".shader_bundle", "json"))
            }
            "residency" | "compute_residency" => {
                outputs.push(output_dir.join("kain_compute_residency.json"))
            }
            other => outputs.push(output_base.with_extension(other)),
        }
    }
    outputs
}

fn with_suffix(path: &Path, suffix: &str, extension: &str) -> PathBuf {
    let stem = path_stem_or_name(path);
    path.with_file_name(format!("{stem}{suffix}.{extension}"))
}

fn path_stem_or_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(|value| value.to_str())
        .filter(|value| !value.is_empty())
        .unwrap_or("artifact")
        .to_string()
}

fn trim_float(value: f64) -> String {
    let text = value.to_string();
    text.strip_suffix(".0").unwrap_or(&text).to_string()
}

fn sanitize_id(value: &str) -> String {
    let mut output = String::new();
    let mut last_dash = false;
    for ch in value.chars() {
        if ch.is_ascii_alphanumeric() {
            output.push(ch.to_ascii_lowercase());
            last_dash = false;
        } else if !last_dash {
            output.push('-');
            last_dash = true;
        }
    }
    let output = output.trim_matches('-').to_string();
    if output.is_empty() {
        "item".to_string()
    } else {
        output
    }
}

fn expand_source_glob(workspace_root: &Path, roots: &[PathBuf], pattern: &str) -> Vec<String> {
    let patterns = expand_brace_pattern(&normalize_path_string(pattern));
    let roots = if roots.is_empty() {
        vec![PathBuf::from(".")]
    } else {
        roots.to_vec()
    };
    let mut output = BTreeSet::new();
    for pattern in patterns {
        let search_root = glob_search_root(workspace_root, &roots, &pattern);
        let mut stack = vec![search_root];
        while let Some(dir) = stack.pop() {
            let Ok(entries) = std::fs::read_dir(&dir) else {
                continue;
            };
            let mut entries = entries.filter_map(Result::ok).collect::<Vec<_>>();
            entries.sort_by_key(|entry| entry.path());
            for entry in entries {
                let path = entry.path();
                let Ok(metadata) = entry.metadata() else {
                    continue;
                };
                if metadata.is_dir() {
                    stack.push(path);
                } else if metadata.is_file() {
                    let relative = path
                        .strip_prefix(workspace_root)
                        .ok()
                        .map(normalize_path)
                        .unwrap_or_else(|| normalize_path(&path));
                    if glob_match(&pattern, &relative) {
                        output.insert(relative);
                    }
                }
            }
        }
    }
    output.into_iter().collect()
}

fn glob_search_root(workspace_root: &Path, roots: &[PathBuf], pattern: &str) -> PathBuf {
    let wildcard = pattern
        .find(['*', '?', '{'])
        .unwrap_or_else(|| pattern.len());
    let prefix = &pattern[..wildcard];
    let prefix_dir = prefix
        .rsplit_once('/')
        .map(|(dir, _)| dir)
        .unwrap_or("")
        .trim_end_matches('/');
    if prefix_dir.is_empty() {
        workspace_root.to_path_buf()
    } else {
        let candidate = workspace_root.join(prefix_dir);
        if candidate.exists() {
            candidate
        } else {
            roots
                .iter()
                .map(|root| workspace_root.join(root).join(prefix_dir))
                .find(|path| path.exists())
                .unwrap_or(candidate)
        }
    }
}

fn expand_brace_pattern(pattern: &str) -> Vec<String> {
    let Some(open) = pattern.find('{') else {
        return vec![pattern.to_string()];
    };
    let Some(close_relative) = pattern[open + 1..].find('}') else {
        return vec![pattern.to_string()];
    };
    let close = open + 1 + close_relative;
    let before = &pattern[..open];
    let after = &pattern[close + 1..];
    pattern[open + 1..close]
        .split(',')
        .flat_map(|part| expand_brace_pattern(&format!("{before}{part}{after}")))
        .collect()
}

fn glob_match(pattern: &str, path: &str) -> bool {
    let pattern = normalize_path_string(pattern);
    let path = normalize_path_string(path);
    let pattern_segments = pattern.split('/').collect::<Vec<_>>();
    let path_segments = path.split('/').collect::<Vec<_>>();
    glob_segments_match(&pattern_segments, &path_segments)
}

fn glob_segments_match(pattern: &[&str], path: &[&str]) -> bool {
    if pattern.is_empty() {
        return path.is_empty();
    }
    if pattern[0] == "**" {
        return glob_segments_match(&pattern[1..], path)
            || (!path.is_empty() && glob_segments_match(pattern, &path[1..]));
    }
    !path.is_empty()
        && glob_segment_match(pattern[0], path[0])
        && glob_segments_match(&pattern[1..], &path[1..])
}

fn glob_segment_match(pattern: &str, text: &str) -> bool {
    glob_segment_match_bytes(pattern.as_bytes(), text.as_bytes())
}

fn glob_segment_match_bytes(pattern: &[u8], text: &[u8]) -> bool {
    if pattern.is_empty() {
        return text.is_empty();
    }
    match pattern[0] {
        b'*' => {
            glob_segment_match_bytes(&pattern[1..], text)
                || (!text.is_empty() && glob_segment_match_bytes(pattern, &text[1..]))
        }
        b'?' => !text.is_empty() && glob_segment_match_bytes(&pattern[1..], &text[1..]),
        byte => !text.is_empty() && text[0] == byte && glob_segment_match_bytes(&pattern[1..], &text[1..]),
    }
}

fn normalize_path(path: &Path) -> String {
    normalize_path_string(&path.display().to_string())
}

fn normalize_path_string(value: &str) -> String {
    value.replace('\\', "/").trim_start_matches("./").to_string()
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_fs as kfs;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn evaluator_lowers_helpers_maps_source_sets_and_specialized_tasks() {
        let root = unique_test_dir("evaluated-build");
        kfs::create_dir_all(root.join("src/gpu")).expect("src gpu");
        kfs::create_dir_all(root.join("telemetry")).expect("telemetry");
        kfs::write_text(root.join("src/main.kn"), "fn main() -> Int:\n    return 0\n")
            .expect("main");
        kfs::write_text(root.join("src/gpu/fragment.kn"), "shader fragment F() -> Vec4:\n    return vec4(0.0, 0.0, 0.0, 1.0)\n")
            .expect("fragment");
        kfs::write_text(root.join("src/gpu/compute.kn"), "shader compute C(id: UVec3) -> Vec4:\n    return vec4(0.0, 0.0, 0.0, 1.0)\n")
            .expect("compute");
        kfs::write_text(root.join("telemetry/run.kn"), "fn main() -> Int:\n    return 0\n")
            .expect("runner");

        let source = r#"
use std::build

const MODES = ["full", "attrition"]

fn mode_task(name: String) -> BuildTask:
    return album_mode("album-" + name)
        .mode(name)
        .runner("telemetry/run.kn")
        .executable("$root/demo.exe")
        .output_dir("$root/telemetry/" + name)
        .requires("root-executable")

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("demo")
        .kind("kain_executable")
        .version("0.1.0")
        .entry("src/main.kn")
        .source_root("src")
        .targets("llvm", "cuda")
        .artifact_root(".kain/out")
    let sources = source_set("sources")
        .root("src")
        .glob("src/**/*.kn")
        .file("build.kn")
    let check = check_task("check-llvm")
        .project(app)
        .target("llvm")
        .inputs(sources)
    let gpu = gpu_suite("gpu-artifacts")
        .fragment("src/gpu/fragment.kn")
        .compute("src/gpu/compute.kn")
        .targets("spirv", "cuda")
        .requires(check)
    let exe = native_executable("root-executable")
        .project(app)
        .requires(check, gpu)
    let modes = map(MODES, mode_task)
    return build_graph(app).sources(sources).tasks(check, gpu, exe, modes)
"#;

        let evaluated = evaluate_build_script(source, &root, "build.kn:evaluated")
            .expect("evaluated build script");
        assert_eq!(evaluated.manifest.package.name.as_deref(), Some("demo"));
        assert_eq!(evaluated.manifest.build.entry, Some(PathBuf::from("src/main.kn")));
        assert_eq!(
            evaluated.manifest.build.targets,
            vec!["llvm".to_string(), "cuda".to_string()]
        );
        let by_id = evaluated
            .explicit_tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();
        assert!(by_id.contains_key("check-llvm"));
        assert!(by_id.contains_key("gpu-artifacts-spirv"));
        assert!(by_id.contains_key("gpu-artifacts-cuda"));
        assert!(by_id.contains_key("root-executable"));
        assert!(by_id.contains_key("album-full"));
        assert!(by_id.contains_key("album-attrition"));
        assert!(by_id["check-llvm"].inputs.contains(&PathBuf::from("src/main.kn")));
        assert_eq!(
            by_id["root-executable"].depends_on,
            vec![
                "check-llvm".to_string(),
                "gpu-artifacts-spirv".to_string(),
                "gpu-artifacts-cuda".to_string()
            ]
        );
    }

    #[test]
    fn evaluator_lowers_cuda_artifacts_to_existing_exec_shape() {
        let root = unique_test_dir("evaluated-cuda");
        kfs::create_dir_all(root.join("src")).expect("src");
        kfs::write_text(root.join("src/search_kernel.kn"), "").expect("kernel");
        let source = r#"
use std::build

fn kernel(name: String) -> BuildTask:
    return cuda_artifacts("emit-" + name)
        .entry("src/" + name + ".kn")
        .stem(name)
        .output_dir(".kain/oracle/gpu/" + name)
        .outputs("ptx", "gpu_rs", "reflection", "shader_bundle", "residency")

fn build(ctx: BuildContext) -> BuildGraph:
    let tasks = map(["search_kernel"], kernel)
    return build_graph(project("oracle").entry("src/main.kn").targets("llvm")).tasks(tasks)
"#;
        let evaluated = evaluate_build_script(source, &root, "build.kn:evaluated")
            .expect("evaluated build script");
        let task = evaluated
            .explicit_tasks
            .iter()
            .find(|task| task.id == "emit-search_kernel")
            .expect("cuda artifacts task");
        assert_eq!(task.kind, "exec");
        assert_eq!(task.command.as_deref(), Some("kain"));
        assert!(task.args.contains(&"gpu-artifacts".to_string()));
        assert!(task
            .outputs
            .contains(&PathBuf::from(".kain/oracle/gpu/search_kernel/kain_compute_residency.json")));
    }

    fn unique_test_dir(label: &str) -> PathBuf {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time")
            .as_nanos();
        std::env::temp_dir().join(format!("{label}-{unique}"))
    }
}
