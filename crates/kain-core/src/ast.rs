//! KAIN Abstract Syntax Tree
//!
//! The AST represents the structure of a KAIN program after parsing.
//! It captures all language constructs as first-class citizens:
//! - Functions with effect annotations
//! - Components (React-like UI)
//! - Shaders (GPU programs)
//! - Actors (Erlang-style concurrency)
//! - Comptime blocks (Zig-style compile-time execution)

use crate::effects::Effect;
use crate::span::Span;

/// A complete KAIN program/module
#[derive(Debug, Clone, PartialEq)]
pub struct Program {
    pub items: Vec<Item>,
    pub span: Span,
}

/// Top-level items in a module
#[derive(Debug, Clone, PartialEq)]
pub enum Item {
    /// `fn name(args) -> Type with Effects: body`
    Function(Function),

    /// `component Name(props) -> UI with Reactive: jsx`
    Component(Component),

    /// `shader Name(inputs) -> Fragment with GPU: body`
    Shader(Shader),

    /// `actor Name: handlers`
    Actor(Actor),

    /// `struct Name { fields }`
    Struct(Struct),

    /// `enum Name { variants }`
    Enum(Enum),

    /// `trait Name { methods }`
    Trait(Trait),

    /// `impl Trait for Type { methods }`
    Impl(Impl),

    /// `type Alias = Type`
    TypeAlias(TypeAlias),

    /// `use path::to::item`
    Use(Use),

    /// `mod name`
    Mod(Mod),

    /// `const NAME: Type = value`
    Const(Const),

    /// `comptime { code }`
    Comptime(ComptimeBlock),

    /// `macro name!(params) { expansion }`
    Macro(MacroDef),

    /// `test "name": body`
    Test(TestDef),

    /// `@material_graph Name: inputs, body, outputs`
    MaterialGraph(MaterialGraphDef),

    /// `@material_function Name: inputs, body, output`
    MaterialFunction(MaterialFunctionDef),

    /// `@graph_editor Name: node_types, schema`
    GraphEditor(GraphEditorDef),

    /// `@graph_runtime Name: graph_data, node_data, instance, pins`
    GraphRuntime(GraphRuntimeDef),

    /// `@state_machine Name: states, transitions`
    StateMachine(StateMachineDef),

    /// `@async_task Name: input, output, callback, do_work`
    AsyncTask(AsyncTaskDef),

    /// `@editor_module Name: menu_entries, toolbar_buttons, toolbar_widgets`
    EditorModule(EditorModuleDef),

    /// `@gameplay_tags namespace Name: tag_hierarchy`
    GameplayTags(GameplayTagsNamespace),

    /// `@ability struct Name: policies, tags, lifecycle_hooks`
    GameplayAbility(GameplayAbilityDef),

    /// `@gameplay_effect struct Name: duration, modifiers, tags`
    /// `@gameplay_effect struct Name: duration, modifiers, tags`
    GameplayEffect(GameplayEffectDef),

    /// `@gameplay_cue struct Name: tag, type, lifecycle_hooks`
    GameplayCue(GameplayCueDef),

    /// `@ability_task struct Name: delegates, state, lifecycle_hooks`
    AbilityTask(AbilityTaskDef),

    /// `@target_actor struct Name: trace_type, filters, reticle`
    TargetActor(TargetActorDef),
}

#[derive(Debug, Clone, PartialEq)]
pub struct TestDef {
    pub name: String,
    pub body: Block,
    pub span: Span,
}

// === FUNCTIONS ===

/// Function attribute/decorator (e.g., @wasm, @js, @inline)
#[derive(Debug, Clone, PartialEq)]
pub struct Attribute {
    pub name: String,
    pub args: Vec<Expr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Function {
    pub name: String,
    pub generics: Vec<Generic>,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub effects: Vec<Effect>,
    pub body: Block,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Param {
    pub name: String,
    pub ty: Type,
    pub mutable: bool,
    pub default: Option<Expr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Generic {
    pub name: String,
    pub bounds: Vec<TypeBound>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct TypeBound {
    pub trait_name: String,
    pub span: Span,
}

// === COMPONENTS (React-like UI) ===

#[derive(Debug, Clone, PartialEq)]
pub struct Component {
    pub name: String,
    pub props: Vec<Param>,
    pub state: Vec<StateDecl>,
    pub methods: Vec<Function>,
    pub effects: Vec<Effect>,
    pub body: JSXNode,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct StateDecl {
    pub name: String,
    pub ty: Type,
    pub initial: Expr,
    pub weak: bool,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum JSXNode {
    /// `<tag attr="value">children</tag>`
    Element {
        tag: String,
        attributes: Vec<JSXAttribute>,
        children: Vec<JSXNode>,
        span: Span,
    },
    /// `{expression}`
    Expression(Box<Expr>),
    /// Plain text
    Text(String, Span),
    /// `<Component prop={value} />`
    ComponentCall {
        name: String,
        props: Vec<JSXAttribute>,
        children: Vec<JSXNode>,
        span: Span,
    },
    /// `for item in list: jsx`
    For {
        binding: String,
        iter: Box<Expr>,
        body: Box<JSXNode>,
        span: Span,
    },
    /// `if cond: jsx [else: jsx]`
    If {
        condition: Box<Expr>,
        then_branch: Box<JSXNode>,
        else_branch: Option<Box<JSXNode>>,
        span: Span,
    },
    /// Fragment wrapper
    Fragment(Vec<JSXNode>, Span),
}

#[derive(Debug, Clone, PartialEq)]
pub struct JSXAttribute {
    pub name: String,
    pub value: JSXAttrValue,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum JSXAttrValue {
    String(String),
    Expr(Expr),
    Bool(bool),
}

// === SHADERS (GPU Programs) ===

#[derive(Debug, Clone, PartialEq)]
pub struct Shader {
    pub name: String,
    pub stage: ShaderStage,
    pub inputs: Vec<Param>,
    pub outputs: Type,
    pub uniforms: Vec<Uniform>,
    pub body: Block,
    pub span: Span,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Surface,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Uniform {
    pub name: String,
    pub ty: Type,
    pub binding: u32,
    pub span: Span,
}

// === ACTORS (Erlang-style Concurrency) ===

#[derive(Debug, Clone, PartialEq)]
pub struct Actor {
    pub name: String,
    pub state: Vec<StateDecl>,
    pub handlers: Vec<MessageHandler>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct MessageHandler {
    pub message_type: String,
    pub params: Vec<Param>,
    pub body: Block,
    pub span: Span,
}

// === DATA STRUCTURES ===

#[derive(Debug, Clone, PartialEq)]
pub struct Struct {
    pub name: String,
    pub generics: Vec<Generic>,
    pub fields: Vec<Field>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Field {
    pub name: String,
    pub ty: Type,
    pub attributes: Vec<Attribute>,
    pub visibility: Visibility,
    pub default: Option<Expr>,
    pub weak: bool,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Enum {
    pub name: String,
    pub generics: Vec<Generic>,
    pub variants: Vec<Variant>,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Variant {
    pub name: String,
    pub fields: VariantFields,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum VariantFields {
    Unit,
    Tuple(Vec<Type>),
    Struct(Vec<Field>),
}

// === TRAITS AND IMPLS ===

#[derive(Debug, Clone, PartialEq)]
pub struct Trait {
    pub name: String,
    pub generics: Vec<Generic>,
    pub methods: Vec<TraitMethod>,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct TraitMethod {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub effects: Vec<Effect>,
    pub default_impl: Option<Block>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Impl {
    pub generics: Vec<Generic>,
    pub trait_name: Option<String>,
    pub target_type: Type,
    pub methods: Vec<Function>,
    pub span: Span,
}

// === TYPE SYSTEM ===

#[derive(Debug, Clone, PartialEq)]
pub enum PointerProvenance {
    Raw,
    ImportedC,
    ImportedAsm,
    LoweredRef,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    /// Named type: `Int`, `String`, `Vec<T>`
    Named {
        name: String,
        generics: Vec<Type>,
        span: Span,
    },
    /// Tuple: `(A, B, C)`
    Tuple(Vec<Type>, Span),
    /// Array: `[T; N]`
    Array(Box<Type>, usize, Span),
    /// Slice: `[T]`
    Slice(Box<Type>, Span),
    /// Reference: `&T`, `&mut T`
    Ref {
        mutable: bool,
        inner: Box<Type>,
        lifetime: Option<String>,
        span: Span,
    },
    /// Raw pointer: `ptr<T>`, `ptr_mut<T>`
    Ptr {
        mutable: bool,
        inner: Box<Type>,
        provenance: PointerProvenance,
        span: Span,
    },
    /// Function type: `fn(A, B) -> C with Effects`
    Function {
        params: Vec<Type>,
        return_type: Box<Type>,
        effects: Vec<Effect>,
        span: Span,
    },
    /// Option shorthand: `T?`
    Option(Box<Type>, Span),
    /// Result shorthand: `T!E`
    Result(Box<Type>, Box<Type>, Span),
    /// Inferred: `_`
    Infer(Span),
    /// Never type: `!`
    Never(Span),
    /// Unit type: `()`
    Unit(Span),
    /// impl Trait: `impl Future`, `impl Iterator<Item = T>`
    Impl {
        trait_name: String,
        generics: Vec<Type>,
        span: Span,
    },
}

impl Type {
    pub fn span(&self) -> Span {
        match self {
            Type::Named { span, .. }
            | Type::Tuple(_, span)
            | Type::Array(_, _, span)
            | Type::Slice(_, span)
            | Type::Ref { span, .. }
            | Type::Ptr { span, .. }
            | Type::Function { span, .. }
            | Type::Option(_, span)
            | Type::Result(_, _, span)
            | Type::Infer(span)
            | Type::Never(span)
            | Type::Unit(span)
            | Type::Impl { span, .. } => *span,
        }
    }

    pub fn contains_raw_ptr(&self) -> bool {
        match self {
            Type::Ptr { .. } => true,
            Type::Named { generics, .. } => generics.iter().any(Self::contains_raw_ptr),
            Type::Tuple(types, _) => types.iter().any(Self::contains_raw_ptr),
            Type::Array(inner, _, _)
            | Type::Slice(inner, _)
            | Type::Ref { inner, .. }
            | Type::Option(inner, _) => inner.contains_raw_ptr(),
            Type::Function {
                params,
                return_type,
                ..
            } => params.iter().any(Self::contains_raw_ptr) || return_type.contains_raw_ptr(),
            Type::Result(ok, err, _) => ok.contains_raw_ptr() || err.contains_raw_ptr(),
            Type::Impl { generics, .. } => generics.iter().any(Self::contains_raw_ptr),
            Type::Infer(_) | Type::Never(_) | Type::Unit(_) => false,
        }
    }
}

// === OTHER TOP-LEVEL ITEMS ===

#[derive(Debug, Clone, PartialEq)]
pub struct TypeAlias {
    pub name: String,
    pub generics: Vec<Generic>,
    pub target: Type,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Use {
    pub path: Vec<String>,
    pub alias: Option<String>,
    pub glob: bool,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Mod {
    pub name: String,
    pub inline: Option<Vec<Item>>,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Const {
    pub name: String,
    pub ty: Type,
    pub value: Expr,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct ComptimeBlock {
    pub body: Block,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct MacroDef {
    pub name: String,
    pub params: Vec<MacroParam>,
    pub body: MacroBody,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct MacroParam {
    pub name: String,
    pub kind: MacroParamKind,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum MacroParamKind {
    Expr,
    Type,
    Ident,
    Block,
    Token,
    Repetition(Box<MacroParamKind>),
}

#[derive(Debug, Clone, PartialEq)]
pub enum MacroBody {
    Tokens(Vec<MacroToken>),
    Block(Block),
}

#[derive(Debug, Clone, PartialEq)]
pub struct MacroToken {
    pub content: String,
    pub span: Span,
}

// === EXPRESSIONS ===

#[derive(Debug, Clone, PartialEq)]
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    /// `let pattern [: Type] = value`
    Let {
        pattern: Pattern,
        ty: Option<Type>,
        value: Option<Expr>,
        span: Span,
    },
    /// Expression statement
    Expr(Expr),
    /// `return [value]`
    Return(Option<Expr>, Span),
    /// `break [value]`
    Break(Option<Expr>, Span),
    /// `continue`
    Continue(Span),
    /// `for binding in iter: body`
    For {
        binding: Pattern,
        iter: Expr,
        body: Block,
        span: Span,
    },
    /// `while cond: body`
    While {
        condition: Expr,
        body: Block,
        span: Span,
    },
    /// `loop: body`
    Loop { body: Block, span: Span },
    /// Item declaration (nested function, struct, etc.)
    Item(Box<Item>),
}

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    /// Literals
    Int(i64, Span),
    Float(f64, Span),
    String(String, Span),
    FString(Vec<Expr>, Span),
    Bool(bool, Span),
    None(Span),

    /// Identifier
    Ident(String, Span),

    /// Macro call
    MacroCall {
        name: String,
        args: Vec<Expr>,
        span: Span,
    },

    /// Binary operation
    Binary {
        left: Box<Expr>,
        op: BinaryOp,
        right: Box<Expr>,
        span: Span,
    },

    /// Unary operation
    Unary {
        op: UnaryOp,
        operand: Box<Expr>,
        span: Span,
    },

    /// Function call: `func(args)`
    Call {
        callee: Box<Expr>,
        args: Vec<CallArg>,
        span: Span,
    },

    /// Method call: `obj.method(args)`
    MethodCall {
        receiver: Box<Expr>,
        method: String,
        args: Vec<CallArg>,
        span: Span,
    },

    /// Field access: `obj.field`
    Field {
        object: Box<Expr>,
        field: String,
        span: Span,
    },

    /// Index: `arr[i]`
    Index {
        object: Box<Expr>,
        index: Box<Expr>,
        span: Span,
    },

    /// Assignment: `target = value`
    Assign {
        target: Box<Expr>,
        value: Box<Expr>,
        span: Span,
    },

    /// Struct literal: `Point { x: 1, y: 2 }`
    Struct {
        name: String,
        fields: Vec<(String, Expr)>,
        span: Span,
    },

    AggregateInit {
        ty: Type,
        fields: Vec<(String, Expr)>,
        zero_fill_rest: bool,
        span: Span,
    },

    EnumVariant {
        enum_name: String,
        variant: String,
        fields: EnumVariantFields,
        span: Span,
    },

    /// Array literal: `[1, 2, 3]`
    Array(Vec<Expr>, Span),

    /// Tuple literal: `(a, b, c)`
    Tuple(Vec<Expr>, Span),

    /// Range: `start..end`, `start..=end`
    Range {
        start: Option<Box<Expr>>,
        end: Option<Box<Expr>>,
        inclusive: bool,
        span: Span,
    },

    /// If expression
    If {
        condition: Box<Expr>,
        then_branch: Block,
        else_branch: Option<Box<ElseBranch>>,
        span: Span,
    },

    /// Match expression
    Match {
        scrutinee: Box<Expr>,
        arms: Vec<MatchArm>,
        span: Span,
    },

    /// Lambda: `|args| body` or `|args| -> Type: body`
    Lambda {
        params: Vec<Param>,
        return_type: Option<Type>,
        body: Box<Expr>,
        span: Span,
    },

    /// Reference: `&value`, `&mut value`
    Ref {
        mutable: bool,
        value: Box<Expr>,
        span: Span,
    },

    /// Address of an addressable location: `addr_of(value)`
    AddrOf {
        value: Box<Expr>,
        pointee_ty: Option<Type>,
        span: Span,
    },

    /// Dereference: `*ptr`
    Deref(Box<Expr>, Span),

    /// Pointer arithmetic offset: `ptr_offset(ptr, i)`
    PtrOffset {
        pointer: Box<Expr>,
        offset: Box<Expr>,
        element_ty: Option<Type>,
        span: Span,
    },

    /// Raw memory load: `mem_load(ptr)`
    MemLoad {
        pointer: Box<Expr>,
        load_ty: Option<Type>,
        span: Span,
    },

    /// Raw memory store: `mem_store(ptr, value)`
    MemStore {
        pointer: Box<Expr>,
        value: Box<Expr>,
        store_ty: Option<Type>,
        span: Span,
    },

    /// Layout-backed size query: `sizeof_type("T")`
    SizeOfType {
        target: Type,
        span: Span,
    },

    /// Layout-backed alignment query: `alignof_type("T")`
    AlignOfType {
        target: Type,
        span: Span,
    },

    /// Explicit stack/local storage allocation.
    Alloca {
        ty: Type,
        span: Span,
    },

    /// Explicit uninitialized storage placeholder.
    Uninit {
        ty: Type,
        span: Span,
    },

    Alloc {
        size: Box<Expr>,
        ty: Option<Type>,
        zeroed: bool,
        span: Span,
    },

    Realloc {
        pointer: Box<Expr>,
        size: Box<Expr>,
        ty: Option<Type>,
        zeroed_new: bool,
        span: Span,
    },

    /// Cast: `value as Type`
    Cast {
        value: Box<Expr>,
        target: Type,
        span: Span,
    },

    /// Try: `expr?`
    Try(Box<Expr>, Span),

    /// Await: `await expr`
    Await(Box<Expr>, Span),

    /// Spawn actor: `spawn ActorName { state }`
    Spawn {
        actor: String,
        init: Vec<(String, Expr)>,
        span: Span,
    },

    /// Send message: `send target <- Message { data }`
    SendMsg {
        target: Box<Expr>,
        message: String,
        data: Vec<(String, Expr)>,
        span: Span,
    },

    /// Comptime expression: `comptime { expr }`
    Comptime(Box<Expr>, Span),

    /// Macro invocation: `name!(args)`
    // Already defined above, remove duplicate

    /// Block expression
    Block(Block, Span),

    /// JSX embedded in expression
    JSX(JSXNode, Span),

    /// Grouped expression: `(expr)`
    Paren(Box<Expr>, Span),

    /// Return expression: `return [expr]`
    Return(Option<Box<Expr>>, Span),

    /// Break expression: `break [expr]`
    Break(Option<Box<Expr>>, Span),

    /// Continue expression: `continue`
    Continue(Span),
}

impl Expr {
    pub fn span(&self) -> Span {
        match self {
            Expr::Int(_, s)
            | Expr::Float(_, s)
            | Expr::String(_, s)
            | Expr::FString(_, s)
            | Expr::Bool(_, s)
            | Expr::None(s)
            | Expr::Ident(_, s)
            | Expr::Binary { span: s, .. }
            | Expr::Unary { span: s, .. }
            | Expr::Call { span: s, .. }
            | Expr::MethodCall { span: s, .. }
            | Expr::Field { span: s, .. }
            | Expr::Index { span: s, .. }
            | Expr::Struct { span: s, .. }
            | Expr::AggregateInit { span: s, .. }
            | Expr::EnumVariant { span: s, .. }
            | Expr::Array(_, s)
            | Expr::Tuple(_, s)
            | Expr::Range { span: s, .. }
            | Expr::If { span: s, .. }
            | Expr::Match { span: s, .. }
            | Expr::Lambda { span: s, .. }
            | Expr::Ref { span: s, .. }
            | Expr::AddrOf { span: s, .. }
            | Expr::Deref(_, s)
            | Expr::PtrOffset { span: s, .. }
            | Expr::MemLoad { span: s, .. }
            | Expr::MemStore { span: s, .. }
            | Expr::SizeOfType { span: s, .. }
            | Expr::AlignOfType { span: s, .. }
            | Expr::Alloca { span: s, .. }
            | Expr::Uninit { span: s, .. }
            | Expr::Alloc { span: s, .. }
            | Expr::Realloc { span: s, .. }
            | Expr::Cast { span: s, .. }
            | Expr::Try(_, s)
            | Expr::Await(_, s)
            | Expr::Spawn { span: s, .. }
            | Expr::SendMsg { span: s, .. }
            | Expr::Comptime(_, s)
            | Expr::MacroCall { span: s, .. }
            | Expr::Block(_, s)
            | Expr::JSX(_, s)
            | Expr::Assign { span: s, .. }
            | Expr::Paren(_, s)
            | Expr::Return(_, s)
            | Expr::Break(_, s)
            | Expr::Continue(s) => *s,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum EnumVariantFields {
    Unit,
    Tuple(Vec<Expr>),
    Struct(Vec<(String, Expr)>),
}

#[derive(Debug, Clone, PartialEq)]
pub struct CallArg {
    pub name: Option<String>,
    pub value: Expr,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ElseBranch {
    Else(Block),
    ElseIf(Box<Expr>, Block, Option<Box<ElseBranch>>),
}

#[derive(Debug, Clone, PartialEq)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub guard: Option<Expr>,
    pub body: Expr,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    /// Wildcard: `_`
    Wildcard(Span),
    /// Literal: `1`, `"hello"`, `true`
    Literal(Expr),
    /// Binding: `x`, `mut x`
    Binding {
        name: String,
        mutable: bool,
        span: Span,
    },
    /// Struct: `Point { x, y }`
    Struct {
        name: String,
        fields: Vec<(String, Pattern)>,
        rest: bool,
        span: Span,
    },
    /// Tuple: `(a, b, c)`
    Tuple(Vec<Pattern>, Span),
    /// Enum variant: `Some(x)`, `None`
    Variant {
        enum_name: Option<String>,
        variant: String,
        fields: VariantPatternFields,
        span: Span,
    },
    /// Array/Slice: `[first, rest @ ..]`
    Slice {
        patterns: Vec<Pattern>,
        rest: Option<String>,
        span: Span,
    },
    /// Or pattern: `A | B`
    Or(Vec<Pattern>, Span),
    /// Range: `1..10`
    Range {
        start: Option<Box<Expr>>,
        end: Option<Box<Expr>>,
        inclusive: bool,
        span: Span,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum VariantPatternFields {
    Unit,
    Tuple(Vec<Pattern>),
    Struct(Vec<(String, Pattern)>),
}

// === OPERATORS ===

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BinaryOp {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,

    // Comparison
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,

    // Logical
    And,
    Or,

    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,

    // Assignment
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,

    // Range
    Range,
    RangeInclusive,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnaryOp {
    Neg,
    Not,
    BitNot,
    Ref,
    RefMut,
    Deref,
}

// === VISIBILITY ===

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Visibility {
    #[default]
    Private,
    Public,
    Crate,
    Super,
}

// === MATERIAL GRAPHS (UE5 Material System) ===

/// Material graph definition: `@material_graph Name: inputs, body, outputs`
#[derive(Debug, Clone, PartialEq)]
pub struct MaterialGraphDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub inputs: Vec<MaterialInput>,
    pub body: Vec<MaterialStatement>,
    pub outputs: Vec<MaterialOutput>,
    pub span: Span,
}

/// Material input parameter with optional default value
#[derive(Debug, Clone, PartialEq)]
pub struct MaterialInput {
    pub name: String,
    pub ty: Type,
    pub default: Option<Expr>,
    pub span: Span,
}

/// Statement within a material graph body
#[derive(Debug, Clone, PartialEq)]
pub enum MaterialStatement {
    Let {
        name: String,
        value: Expr,
        span: Span,
    },
    // Future: if statements, loops, etc.
}

/// Material output pin (base_color, emissive, roughness, etc.)
#[derive(Debug, Clone, PartialEq)]
pub struct MaterialOutput {
    pub name: String, // base_color, emissive, roughness, metallic, normal, opacity, etc.
    pub value: Expr,
    pub span: Span,
}

/// Material function definition: `@material_function Name: inputs, body, output`
/// Material functions are reusable node graphs that can be called from materials.
#[derive(Debug, Clone, PartialEq)]
pub struct MaterialFunctionDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub inputs: Vec<MaterialInput>,
    pub body: Vec<MaterialStatement>,
    pub output: Expr, // Single output expression
    pub span: Span,
}

// === GRAPH EDITORS ===

/// Graph editor definition: `@graph_editor Name: node_types, schema`
#[derive(Debug, Clone, PartialEq)]
pub struct GraphEditorDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub node_types: Vec<NodeTypeDef>,
    pub schema: Option<GraphSchemaDef>,
    pub span: Span,
}

/// Node type definition within a graph editor
#[derive(Debug, Clone, PartialEq)]
pub struct NodeTypeDef {
    pub name: String,
    pub category: Option<String>,
    pub inputs: Vec<PinDef>,
    pub outputs: Vec<PinDef>,
    pub properties: Vec<PropertyDef>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Pin definition for node inputs/outputs
#[derive(Debug, Clone, PartialEq)]
pub struct PinDef {
    pub name: String,
    pub ty: Type,
    pub is_array: bool,
    pub default: Option<Expr>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Property definition for node configuration
#[derive(Debug, Clone, PartialEq)]
pub struct PropertyDef {
    pub name: String,
    pub ty: Type,
    pub default: Option<Expr>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Graph schema definition for connection rules and validation
#[derive(Debug, Clone, PartialEq)]
pub struct GraphSchemaDef {
    pub rules: Vec<SchemaRule>,
    pub span: Span,
}

/// Schema rule for graph validation
#[derive(Debug, Clone, PartialEq)]
pub struct SchemaRule {
    pub name: String,
    pub condition: Expr,
    pub span: Span,
}

// === GRAPH RUNTIME (UE5 Runtime Graph System) ===

/// Graph runtime definition: `@graph_runtime Name: graph_data, node_data, instance, pins`
/// Generates runtime graph classes (GraphData, NodeData, Instance, PinData) for UE5.
#[derive(Debug, Clone, PartialEq)]
pub struct GraphRuntimeDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub graph_data: Option<GraphDataDef>,
    pub node_types: Vec<NodeDataDef>,
    pub instance: Option<GraphInstanceDef>,
    pub pin_config: Option<PinConfigDef>,
    pub span: Span,
}

/// Graph data container definition
#[derive(Debug, Clone, PartialEq)]
pub struct GraphDataDef {
    pub properties: Vec<Field>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Node data definition for runtime graph nodes
#[derive(Debug, Clone, PartialEq)]
pub struct NodeDataDef {
    pub name: String,
    pub base_class: Option<String>, // Optional base node class
    pub input_pins: Vec<PinDef>,
    pub output_pins: Vec<PinDef>,
    pub properties: Vec<Field>,
    pub methods: Vec<Function>,
    pub execute_logic: Option<Block>, // Optional ExecuteNode() implementation
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Graph instance definition for runtime execution
#[derive(Debug, Clone, PartialEq)]
pub struct GraphInstanceDef {
    pub state: Vec<Field>,
    pub methods: Vec<Function>,
    pub delegates: Vec<DelegateDef>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Pin configuration for graph connections
#[derive(Debug, Clone, PartialEq)]
pub struct PinConfigDef {
    pub properties: Vec<Field>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Delegate definition for graph events
#[derive(Debug, Clone, PartialEq)]
pub struct DelegateDef {
    pub name: String,
    pub params: Vec<Param>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

// === NETWORK SYNCHRONIZATION (UE5 Network Replication System) ===

/// Network synchronization definition for replicated components
/// Generates network replication code with interpolation, extrapolation, and compression
#[derive(Debug, Clone, PartialEq)]
pub struct NetworkSyncDef {
    pub component_name: String,
    pub replicated_properties: Vec<ReplicatedProperty>,
    pub config: NetworkConfig,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Replicated property with synchronization mode
#[derive(Debug, Clone, PartialEq)]
pub struct ReplicatedProperty {
    pub name: String,
    pub ty: Type,
    pub mode: ReplicationMode,
    pub compression: Option<CompressionSettings>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Replication mode for network synchronization
#[derive(Debug, Clone, PartialEq)]
pub enum ReplicationMode {
    /// Simple replication (no interpolation)
    Simple,

    /// Interpolated replication with back_time buffer
    Interpolated { back_time: f32 },

    /// Extrapolated replication for prediction
    Extrapolated { limit: f32 },

    /// Compressed replication with threshold
    Compressed { threshold: f32 },
}

/// Compression settings for replicated properties
#[derive(Debug, Clone, PartialEq)]
pub struct CompressionSettings {
    pub threshold: f32,
    pub use_half_float: bool,
}

/// Network configuration for component
#[derive(Debug, Clone, PartialEq)]
pub struct NetworkConfig {
    pub snap_threshold: Option<f32>,
    pub send_rate: Option<f32>,
    pub owner_time_sync: bool,
}

impl Default for NetworkConfig {
    fn default() -> Self {
        Self {
            snap_threshold: Some(500.0),
            send_rate: Some(10.0),
            owner_time_sync: true,
        }
    }
}

// === ANIMATION STATE MACHINE (UE5 Animation System) ===

/// State machine definition: `@state_machine struct Name: states`
/// Generates state machine runtime class with state enum, transitions, and update logic
#[derive(Debug, Clone, PartialEq)]
pub struct StateMachineDef {
    pub name: String,
    pub states: Vec<StateDef>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// State definition within a state machine
#[derive(Debug, Clone, PartialEq)]
pub struct StateDef {
    pub name: String,
    pub is_entry: bool,
    pub animation: Option<String>,
    pub properties: Vec<Field>,
    pub transitions: Vec<TransitionDef>,
    pub on_enter: Option<Block>,
    pub on_exit: Option<Block>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Transition definition between states
#[derive(Debug, Clone, PartialEq)]
pub struct TransitionDef {
    pub to_state: String,
    pub condition: Option<Block>,
    pub priority: i32,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Async task definition for offloading heavy computations to worker threads
/// Generates task queue class with thread pool, task class with DoWork method,
/// and completion callback dispatching to main thread
#[derive(Debug, Clone, PartialEq)]
pub struct AsyncTaskDef {
    pub name: String,
    pub input_fields: Vec<Field>,
    pub output_fields: Vec<Field>,
    pub callback: Option<AsyncTaskCallback>,
    pub do_work: Option<Block>,
    pub priority: Option<i32>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Callback definition for async task completion
#[derive(Debug, Clone, PartialEq)]
pub struct AsyncTaskCallback {
    pub name: String,
    pub thread: AsyncTaskThread,
    pub params: Vec<Param>,
    pub body: Block,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Thread specification for callback execution
#[derive(Debug, Clone, PartialEq)]
pub enum AsyncTaskThread {
    /// Execute callback on main game thread
    Main,
    /// Execute callback on worker thread (same thread as DoWork)
    Worker,
}

// === EDITOR EXTENSION SYSTEM (UE5 Editor Module) ===

/// Editor module definition for extending the UE5 editor
/// Generates IModuleInterface subclass with IMPLEMENT_MODULE macro,
/// menu extensions, toolbar extensions, and editor ticker registration
#[derive(Debug, Clone, PartialEq)]
pub struct EditorModuleDef {
    pub name: String,
    pub menu_entries: Vec<MenuEntryDef>,
    pub toolbar_buttons: Vec<ToolbarButtonDef>,
    pub toolbar_widgets: Vec<ToolbarWidgetDef>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Menu entry definition for editor menu extensions
#[derive(Debug, Clone, PartialEq)]
pub struct MenuEntryDef {
    pub path: String,     // e.g., "Tools/Weapons"
    pub label: String,    // e.g., "Open Weapon Editor"
    pub method: Function, // Callback method
    pub icon: Option<String>,
    pub tooltip: Option<String>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Toolbar button definition for editor toolbar extensions
#[derive(Debug, Clone, PartialEq)]
pub struct ToolbarButtonDef {
    pub section: String,       // e.g., "Content"
    pub label: Option<String>, // Optional label
    pub icon: String,          // e.g., "Icons.Weapon"
    pub method: Function,      // Callback method
    pub tooltip: Option<String>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Toolbar widget definition for custom editor toolbar widgets
#[derive(Debug, Clone, PartialEq)]
pub struct ToolbarWidgetDef {
    pub section: String, // e.g., "CameraSpeed"
    pub position: ToolbarPosition,
    pub widget_type: String, // Widget class name
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Position specification for toolbar widgets
#[derive(Debug, Clone, PartialEq)]
pub enum ToolbarPosition {
    Before,
    After,
    Start,
    End,
}

// ============================================================================
// STDLIB TREE-SHAKING: AST Type Reference Collector
// ============================================================================
//
// Recursively walks an AST Item and collects every type name it references.
// Used by the merge phase in ue5_pipeline.rs to determine which stdlib items
// are actually needed by user code (transitive dependency analysis).

use std::collections::HashSet;

/// Collect all type names referenced by an AST Item.
/// Returns a set of raw type name strings (e.g. "Vec3", "QuestStatus", "Actor").
pub fn collect_referenced_type_names(item: &Item) -> HashSet<String> {
    let mut names = HashSet::new();
    collect_type_names_from_item(item, &mut names);
    names
}

/// Master dispatcher: walks all fields of an Item variant that may contain type references.
fn collect_type_names_from_item(item: &Item, out: &mut HashSet<String>) {
    match item {
        Item::Function(f) => {
            for p in &f.params {
                collect_type_names_from_type(&p.ty, out);
                if let Some(default) = &p.default {
                    collect_type_names_from_expr(default, out);
                }
            }
            if let Some(ret) = &f.return_type {
                collect_type_names_from_type(ret, out);
            }
            collect_type_names_from_block(&f.body, out);
            for attr in &f.attributes {
                for arg in &attr.args {
                    collect_type_names_from_expr(arg, out);
                }
            }
        }
        Item::Struct(s) => {
            for f in &s.fields {
                collect_type_names_from_type(&f.ty, out);
                if let Some(default) = &f.default {
                    collect_type_names_from_expr(default, out);
                }
            }
            for method in &s.methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::Enum(e) => {
            for v in &e.variants {
                match &v.fields {
                    VariantFields::Unit => {}
                    VariantFields::Tuple(types) => {
                        for ty in types {
                            collect_type_names_from_type(ty, out);
                        }
                    }
                    VariantFields::Struct(fields) => {
                        for f in fields {
                            collect_type_names_from_type(&f.ty, out);
                        }
                    }
                }
            }
        }
        Item::Actor(a) => {
            for s in &a.state {
                collect_type_names_from_type(&s.ty, out);
                collect_type_names_from_expr(&s.initial, out);
            }
            for handler in &a.handlers {
                out.insert(handler.message_type.clone());
                for p in &handler.params {
                    collect_type_names_from_type(&p.ty, out);
                }
                collect_type_names_from_block(&handler.body, out);
            }
            for method in &a.methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::Component(c) => {
            for p in &c.props {
                collect_type_names_from_type(&p.ty, out);
            }
            for s in &c.state {
                collect_type_names_from_type(&s.ty, out);
                collect_type_names_from_expr(&s.initial, out);
            }
            for method in &c.methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
            collect_type_names_from_jsx(&c.body, out);
        }
        Item::Shader(s) => {
            for p in &s.inputs {
                collect_type_names_from_type(&p.ty, out);
            }
            collect_type_names_from_type(&s.outputs, out);
            for u in &s.uniforms {
                collect_type_names_from_type(&u.ty, out);
            }
            collect_type_names_from_block(&s.body, out);
        }
        Item::TypeAlias(ta) => {
            collect_type_names_from_type(&ta.target, out);
        }
        Item::Const(c) => {
            collect_type_names_from_type(&c.ty, out);
            collect_type_names_from_expr(&c.value, out);
        }
        Item::Trait(t) => {
            for method in &t.methods {
                for p in &method.params {
                    collect_type_names_from_type(&p.ty, out);
                }
                if let Some(ret) = &method.return_type {
                    collect_type_names_from_type(ret, out);
                }
                if let Some(body) = &method.default_impl {
                    collect_type_names_from_block(body, out);
                }
            }
        }
        Item::Impl(i) => {
            if let Some(trait_name) = &i.trait_name {
                out.insert(trait_name.clone());
            }
            collect_type_names_from_type(&i.target_type, out);
            for method in &i.methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::Comptime(b) => {
            collect_type_names_from_block(&b.body, out);
        }
        Item::Macro(m) => {
            match &m.body {
                MacroBody::Block(block) => collect_type_names_from_block(block, out),
                MacroBody::Tokens(_) => {} // Tokens don't contain resolved type refs
            }
        }
        Item::Test(t) => {
            collect_type_names_from_block(&t.body, out);
        }
        Item::MaterialGraph(mg) => {
            for input in &mg.inputs {
                collect_type_names_from_type(&input.ty, out);
                if let Some(default) = &input.default {
                    collect_type_names_from_expr(default, out);
                }
            }
            for stmt in &mg.body {
                match stmt {
                    MaterialStatement::Let { value, .. } => {
                        collect_type_names_from_expr(value, out);
                    }
                }
            }
            for output in &mg.outputs {
                collect_type_names_from_expr(&output.value, out);
            }
        }
        Item::MaterialFunction(mf) => {
            for input in &mf.inputs {
                collect_type_names_from_type(&input.ty, out);
                if let Some(default) = &input.default {
                    collect_type_names_from_expr(default, out);
                }
            }
            for stmt in &mf.body {
                match stmt {
                    MaterialStatement::Let { value, .. } => {
                        collect_type_names_from_expr(value, out);
                    }
                }
            }
            collect_type_names_from_expr(&mf.output, out);
        }
        Item::GraphEditor(ge) => {
            for nt in &ge.node_types {
                for pin in &nt.inputs {
                    collect_type_names_from_type(&pin.ty, out);
                    if let Some(default) = &pin.default {
                        collect_type_names_from_expr(default, out);
                    }
                }
                for pin in &nt.outputs {
                    collect_type_names_from_type(&pin.ty, out);
                    if let Some(default) = &pin.default {
                        collect_type_names_from_expr(default, out);
                    }
                }
                for prop in &nt.properties {
                    collect_type_names_from_type(&prop.ty, out);
                    if let Some(default) = &prop.default {
                        collect_type_names_from_expr(default, out);
                    }
                }
            }
            if let Some(schema) = &ge.schema {
                for rule in &schema.rules {
                    collect_type_names_from_expr(&rule.condition, out);
                }
            }
        }
        Item::GraphRuntime(gr) => {
            if let Some(gd) = &gr.graph_data {
                for f in &gd.properties {
                    collect_type_names_from_type(&f.ty, out);
                }
                for method in &gd.methods {
                    collect_type_names_from_item(&Item::Function(method.clone()), out);
                }
            }
            for nd in &gr.node_types {
                if let Some(base) = &nd.base_class {
                    out.insert(base.clone());
                }
                for pin in &nd.input_pins {
                    collect_type_names_from_type(&pin.ty, out);
                }
                for pin in &nd.output_pins {
                    collect_type_names_from_type(&pin.ty, out);
                }
                for f in &nd.properties {
                    collect_type_names_from_type(&f.ty, out);
                }
                for method in &nd.methods {
                    collect_type_names_from_item(&Item::Function(method.clone()), out);
                }
                if let Some(logic) = &nd.execute_logic {
                    collect_type_names_from_block(logic, out);
                }
            }
            if let Some(inst) = &gr.instance {
                for f in &inst.state {
                    collect_type_names_from_type(&f.ty, out);
                }
                for method in &inst.methods {
                    collect_type_names_from_item(&Item::Function(method.clone()), out);
                }
                for delegate in &inst.delegates {
                    for p in &delegate.params {
                        collect_type_names_from_type(&p.ty, out);
                    }
                }
            }
            if let Some(pc) = &gr.pin_config {
                for f in &pc.properties {
                    collect_type_names_from_type(&f.ty, out);
                }
                for method in &pc.methods {
                    collect_type_names_from_item(&Item::Function(method.clone()), out);
                }
            }
        }
        Item::StateMachine(sm) => {
            for state in &sm.states {
                for f in &state.properties {
                    collect_type_names_from_type(&f.ty, out);
                }
                for transition in &state.transitions {
                    if let Some(cond) = &transition.condition {
                        collect_type_names_from_block(cond, out);
                    }
                }
                if let Some(on_enter) = &state.on_enter {
                    collect_type_names_from_block(on_enter, out);
                }
                if let Some(on_exit) = &state.on_exit {
                    collect_type_names_from_block(on_exit, out);
                }
            }
        }
        Item::AsyncTask(at) => {
            for f in &at.input_fields {
                collect_type_names_from_type(&f.ty, out);
            }
            for f in &at.output_fields {
                collect_type_names_from_type(&f.ty, out);
            }
            if let Some(do_work) = &at.do_work {
                collect_type_names_from_block(do_work, out);
            }
            if let Some(callback) = &at.callback {
                for p in &callback.params {
                    collect_type_names_from_type(&p.ty, out);
                }
                collect_type_names_from_block(&callback.body, out);
            }
        }
        Item::EditorModule(em) => {
            for entry in &em.menu_entries {
                collect_type_names_from_item(&Item::Function(entry.method.clone()), out);
            }
            for btn in &em.toolbar_buttons {
                collect_type_names_from_item(&Item::Function(btn.method.clone()), out);
            }
        }
        Item::GameplayTags(_) => {
            // GameplayTags don't contain type references
        }
        Item::GameplayAbility(ability) => {
            // Collect types from ability methods
            for method in &ability.methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::GameplayEffect(_) => {
            // GameplayEffects don't contain type references (all configuration is in attributes)
        }
        Item::GameplayCue(cue) => {
            // Collect types from state fields
            for field in &cue.state_fields {
                collect_type_names_from_type(&field.ty, out);
            }
            // Collect types from lifecycle methods
            if let Some(on_execute) = &cue.on_execute {
                collect_type_names_from_item(&Item::Function(on_execute.clone()), out);
            }
            if let Some(on_add) = &cue.on_add {
                collect_type_names_from_item(&Item::Function(on_add.clone()), out);
            }
            if let Some(on_remove) = &cue.on_remove {
                collect_type_names_from_item(&Item::Function(on_remove.clone()), out);
            }
            if let Some(while_active) = &cue.while_active {
                collect_type_names_from_item(&Item::Function(while_active.clone()), out);
            }
        }
        Item::AbilityTask(task) => {
            // Collect types from state fields
            for field in &task.state_fields {
                collect_type_names_from_type(&field.ty, out);
            }
            // Collect types from lifecycle methods
            if let Some(activate) = &task.activate_method {
                collect_type_names_from_item(&Item::Function(activate.clone()), out);
            }
            if let Some(on_destroy) = &task.on_destroy_method {
                collect_type_names_from_item(&Item::Function(on_destroy.clone()), out);
            }
            for method in &task.custom_methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::TargetActor(target) => {
            // Collect types from filter
            if let Some(filter) = &target.filter {
                if let Some(custom_filter) = &filter.custom_filter_method {
                    collect_type_names_from_item(&Item::Function(custom_filter.clone()), out);
                }
            }
            // Collect types from custom methods
            for method in &target.custom_methods {
                collect_type_names_from_item(&Item::Function(method.clone()), out);
            }
        }
        Item::Use(_) | Item::Mod(_) => {
            // Uses and mods don't contain type references we can extract
        }
    }
}

/// Recursively collect type names from a Type AST node.
fn collect_type_names_from_type(ty: &Type, out: &mut HashSet<String>) {
    match ty {
        Type::Named { name, generics, .. } => {
            out.insert(name.clone());
            for g in generics {
                collect_type_names_from_type(g, out);
            }
        }
        Type::Tuple(types, _) => {
            for t in types {
                collect_type_names_from_type(t, out);
            }
        }
        Type::Array(inner, _, _) => {
            collect_type_names_from_type(inner, out);
        }
        Type::Slice(inner, _) => {
            collect_type_names_from_type(inner, out);
        }
        Type::Ref { inner, .. } => {
            collect_type_names_from_type(inner, out);
        }
        Type::Ptr { inner, .. } => {
            collect_type_names_from_type(inner, out);
        }
        Type::Function {
            params,
            return_type,
            ..
        } => {
            for p in params {
                collect_type_names_from_type(p, out);
            }
            collect_type_names_from_type(return_type, out);
        }
        Type::Option(inner, _) => {
            collect_type_names_from_type(inner, out);
        }
        Type::Result(ok, err, _) => {
            collect_type_names_from_type(ok, out);
            collect_type_names_from_type(err, out);
        }
        Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            out.insert(trait_name.clone());
            for g in generics {
                collect_type_names_from_type(g, out);
            }
        }
        Type::Infer(_) | Type::Never(_) | Type::Unit(_) => {}
    }
}

/// Recursively collect type names from an Expr AST node.
fn collect_type_names_from_expr(expr: &Expr, out: &mut HashSet<String>) {
    match expr {
        Expr::Cast { value, target, .. } => {
            collect_type_names_from_expr(value, out);
            collect_type_names_from_type(target, out);
        }
        Expr::Struct { name, fields, .. } => {
            out.insert(name.clone());
            for (_, field_expr) in fields {
                collect_type_names_from_expr(field_expr, out);
            }
        }
        Expr::AggregateInit { ty, fields, .. } => {
            collect_type_names_from_type(ty, out);
            for (_, field_expr) in fields {
                collect_type_names_from_expr(field_expr, out);
            }
        }
        Expr::EnumVariant {
            enum_name, fields, ..
        } => {
            out.insert(enum_name.clone());
            match fields {
                EnumVariantFields::Unit => {}
                EnumVariantFields::Tuple(exprs) => {
                    for e in exprs {
                        collect_type_names_from_expr(e, out);
                    }
                }
                EnumVariantFields::Struct(field_pairs) => {
                    for (_, e) in field_pairs {
                        collect_type_names_from_expr(e, out);
                    }
                }
            }
        }
        Expr::Spawn { actor, init, .. } => {
            out.insert(actor.clone());
            for (_, init_expr) in init {
                collect_type_names_from_expr(init_expr, out);
            }
        }
        Expr::Call { callee, args, .. } => {
            collect_type_names_from_expr(callee, out);
            for arg in args {
                collect_type_names_from_expr(&arg.value, out);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_type_names_from_expr(receiver, out);
            for arg in args {
                collect_type_names_from_expr(&arg.value, out);
            }
        }
        Expr::Field { object, .. } => {
            collect_type_names_from_expr(object, out);
        }
        Expr::Index { object, index, .. } => {
            collect_type_names_from_expr(object, out);
            collect_type_names_from_expr(index, out);
        }
        Expr::Binary { left, right, .. } => {
            collect_type_names_from_expr(left, out);
            collect_type_names_from_expr(right, out);
        }
        Expr::Unary { operand, .. } => {
            collect_type_names_from_expr(operand, out);
        }
        Expr::Assign { target, value, .. } => {
            collect_type_names_from_expr(target, out);
            collect_type_names_from_expr(value, out);
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_type_names_from_expr(condition, out);
            collect_type_names_from_block(then_branch, out);
            if let Some(else_b) = else_branch {
                collect_type_names_from_else_branch(else_b, out);
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            collect_type_names_from_expr(scrutinee, out);
            for arm in arms {
                collect_type_names_from_pattern(&arm.pattern, out);
                if let Some(guard) = &arm.guard {
                    collect_type_names_from_expr(guard, out);
                }
                collect_type_names_from_expr(&arm.body, out);
            }
        }
        Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => {
            for p in params {
                collect_type_names_from_type(&p.ty, out);
            }
            if let Some(ret) = return_type {
                collect_type_names_from_type(ret, out);
            }
            collect_type_names_from_expr(body, out);
        }
        Expr::Array(exprs, _) | Expr::Tuple(exprs, _) | Expr::FString(exprs, _) => {
            for e in exprs {
                collect_type_names_from_expr(e, out);
            }
        }
        Expr::MacroCall { args, .. } => {
            for arg in args {
                collect_type_names_from_expr(arg, out);
            }
        }
        Expr::SendMsg { target, data, .. } => {
            collect_type_names_from_expr(target, out);
            for (_, data_expr) in data {
                collect_type_names_from_expr(data_expr, out);
            }
        }
        Expr::Block(block, _) => {
            collect_type_names_from_block(block, out);
        }
        Expr::Range { start, end, .. } => {
            if let Some(s) = start {
                collect_type_names_from_expr(s, out);
            }
            if let Some(e) = end {
                collect_type_names_from_expr(e, out);
            }
        }
        Expr::Ref { value, .. } => {
            collect_type_names_from_expr(value, out);
        }
        Expr::AddrOf {
            value, pointee_ty, ..
        } => {
            collect_type_names_from_expr(value, out);
            if let Some(ty) = pointee_ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::Deref(inner, _)
        | Expr::Try(inner, _)
        | Expr::Await(inner, _)
        | Expr::Comptime(inner, _)
        | Expr::Paren(inner, _) => {
            collect_type_names_from_expr(inner, out);
        }
        Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => {
            collect_type_names_from_expr(pointer, out);
            collect_type_names_from_expr(offset, out);
            if let Some(ty) = element_ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::MemLoad {
            pointer, load_ty, ..
        } => {
            collect_type_names_from_expr(pointer, out);
            if let Some(ty) = load_ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::MemStore {
            pointer,
            value,
            store_ty,
            ..
        } => {
            collect_type_names_from_expr(pointer, out);
            collect_type_names_from_expr(value, out);
            if let Some(ty) = store_ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::SizeOfType { target, .. } => {
            collect_type_names_from_type(target, out);
        }
        Expr::AlignOfType { target, .. } => {
            collect_type_names_from_type(target, out);
        }
        Expr::Alloca { ty, .. } | Expr::Uninit { ty, .. } => {
            collect_type_names_from_type(ty, out);
        }
        Expr::Alloc { size, ty, .. } => {
            collect_type_names_from_expr(size, out);
            if let Some(ty) = ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::Realloc {
            pointer, size, ty, ..
        } => {
            collect_type_names_from_expr(pointer, out);
            collect_type_names_from_expr(size, out);
            if let Some(ty) = ty {
                collect_type_names_from_type(ty, out);
            }
        }
        Expr::Return(Some(inner), _) | Expr::Break(Some(inner), _) => {
            collect_type_names_from_expr(inner, out);
        }
        Expr::JSX(node, _) => {
            collect_type_names_from_jsx(node, out);
        }
        // Terminals with no type references
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Return(None, _)
        | Expr::Break(None, _)
        | Expr::Continue(_) => {}
    }
}

/// Recursively collect type names from a Block.
fn collect_type_names_from_block(block: &Block, out: &mut HashSet<String>) {
    for stmt in &block.stmts {
        collect_type_names_from_stmt(stmt, out);
    }
}

/// Recursively collect type names from a Stmt.
fn collect_type_names_from_stmt(stmt: &Stmt, out: &mut HashSet<String>) {
    match stmt {
        Stmt::Let {
            ty, value, pattern, ..
        } => {
            if let Some(ty) = ty {
                collect_type_names_from_type(ty, out);
            }
            if let Some(val) = value {
                collect_type_names_from_expr(val, out);
            }
            collect_type_names_from_pattern(pattern, out);
        }
        Stmt::Expr(expr) => {
            collect_type_names_from_expr(expr, out);
        }
        Stmt::Return(Some(expr), _) => {
            collect_type_names_from_expr(expr, out);
        }
        Stmt::For {
            iter,
            body,
            binding,
            ..
        } => {
            collect_type_names_from_pattern(binding, out);
            collect_type_names_from_expr(iter, out);
            collect_type_names_from_block(body, out);
        }
        Stmt::While {
            condition, body, ..
        } => {
            collect_type_names_from_expr(condition, out);
            collect_type_names_from_block(body, out);
        }
        Stmt::Loop { body, .. } => {
            collect_type_names_from_block(body, out);
        }
        Stmt::Item(item) => {
            collect_type_names_from_item(item, out);
        }
        Stmt::Return(None, _) | Stmt::Break(_, _) | Stmt::Continue(_) => {}
    }
}

/// Collect type names from else branches.
fn collect_type_names_from_else_branch(branch: &ElseBranch, out: &mut HashSet<String>) {
    match branch {
        ElseBranch::Else(block) => {
            collect_type_names_from_block(block, out);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            collect_type_names_from_expr(cond, out);
            collect_type_names_from_block(block, out);
            if let Some(next_else) = next {
                collect_type_names_from_else_branch(next_else, out);
            }
        }
    }
}

/// Collect type names from patterns (destructuring may reference types).
fn collect_type_names_from_pattern(pattern: &Pattern, out: &mut HashSet<String>) {
    match pattern {
        Pattern::Variant {
            enum_name, fields, ..
        } => {
            if let Some(name) = enum_name {
                out.insert(name.clone());
            }
            match fields {
                VariantPatternFields::Unit => {}
                VariantPatternFields::Tuple(patterns) => {
                    for p in patterns {
                        collect_type_names_from_pattern(p, out);
                    }
                }
                VariantPatternFields::Struct(field_pairs) => {
                    for (_, p) in field_pairs {
                        collect_type_names_from_pattern(p, out);
                    }
                }
            }
        }
        Pattern::Struct { name, fields, .. } => {
            out.insert(name.clone());
            for (_, p) in fields {
                collect_type_names_from_pattern(p, out);
            }
        }
        Pattern::Tuple(patterns, _) => {
            for p in patterns {
                collect_type_names_from_pattern(p, out);
            }
        }
        Pattern::Or(patterns, _) => {
            for p in patterns {
                collect_type_names_from_pattern(p, out);
            }
        }
        Pattern::Slice { patterns, .. } => {
            for p in patterns {
                collect_type_names_from_pattern(p, out);
            }
        }
        Pattern::Range { start, end, .. } => {
            if let Some(s) = start {
                collect_type_names_from_expr(s, out);
            }
            if let Some(e) = end {
                collect_type_names_from_expr(e, out);
            }
        }
        Pattern::Literal(expr) => {
            collect_type_names_from_expr(expr, out);
        }
        // Terminals with no type references
        Pattern::Wildcard(_) | Pattern::Binding { .. } => {}
    }
}

/// Collect type names from JSX nodes.
fn collect_type_names_from_jsx(node: &JSXNode, out: &mut HashSet<String>) {
    match node {
        JSXNode::Element {
            children,
            attributes,
            ..
        } => {
            for attr in attributes {
                match &attr.value {
                    JSXAttrValue::Expr(e) => collect_type_names_from_expr(e, out),
                    _ => {}
                }
            }
            for child in children {
                collect_type_names_from_jsx(child, out);
            }
        }
        JSXNode::ComponentCall {
            name,
            props,
            children,
            ..
        } => {
            out.insert(name.clone());
            for prop in props {
                match &prop.value {
                    JSXAttrValue::Expr(e) => collect_type_names_from_expr(e, out),
                    _ => {}
                }
            }
            for child in children {
                collect_type_names_from_jsx(child, out);
            }
        }
        JSXNode::Expression(expr) => {
            collect_type_names_from_expr(expr, out);
        }
        JSXNode::For { iter, body, .. } => {
            collect_type_names_from_expr(iter, out);
            collect_type_names_from_jsx(body, out);
        }
        JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_type_names_from_expr(condition, out);
            collect_type_names_from_jsx(then_branch, out);
            if let Some(else_b) = else_branch {
                collect_type_names_from_jsx(else_b, out);
            }
        }
        JSXNode::Fragment(children, _) => {
            for child in children {
                collect_type_names_from_jsx(child, out);
            }
        }
        JSXNode::Text(_, _) => {}
    }
}

// === GAMEPLAY TAGS ===

/// GameplayTags namespace definition
/// Syntax: @gameplay_tags namespace Name: tag_hierarchy
#[derive(Debug, Clone, PartialEq)]
pub struct GameplayTagsNamespace {
    pub name: String,
    pub children: Vec<GameplayTagNode>,
    pub span: Span,
}

/// Individual tag node in the hierarchy
#[derive(Debug, Clone, PartialEq)]
pub struct GameplayTagNode {
    pub name: String,
    pub full_path: String, // "Ability.Attack.Melee.Sword"
    pub comment: Option<String>,
    pub children: Vec<GameplayTagNode>,
    pub span: Span,
}

// === GAMEPLAY ABILITIES (GAS) ===

/// Gameplay Ability definition for UE5 Gameplay Ability System
/// Syntax: @ability struct Name: policies, tags, lifecycle_hooks
#[derive(Debug, Clone, PartialEq)]
pub struct GameplayAbilityDef {
    pub name: String,
    pub instancing_policy: Option<String>,
    pub replication_policy: Option<String>,
    pub net_execution_policy: Option<String>,
    pub ability_tags: Vec<String>,
    pub activation_required_tags: Vec<String>,
    pub activation_blocked_tags: Vec<String>,
    pub activation_owned_tags: Vec<String>,
    pub cancel_abilities_with_tag: Vec<String>,
    pub block_abilities_with_tag: Vec<String>,
    pub cost_effect: Option<String>,
    pub cooldown_effect: Option<String>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

/// Gameplay Effect definition for UE5 Gameplay Ability System
/// Syntax: @gameplay_effect struct Name: duration, modifiers, tags
#[derive(Debug, Clone, PartialEq)]
pub struct GameplayEffectDef {
    pub name: String,
    pub duration_policy: Option<String>, // "Instant", "Infinite", "HasDuration"
    pub duration_magnitude: Option<f32>,
    pub period: Option<f32>,
    pub execute_on_application: bool,
    pub modifiers: Vec<GameplayEffectModifier>,
    pub stacking_type: Option<String>, // "None", "AggregateBySource", "AggregateByTarget"
    pub stacking_limit: Option<i32>,
    pub owned_tags: Vec<String>,
    pub granted_tags: Vec<String>,
    pub application_required_tags: Vec<String>,
    pub application_ignored_tags: Vec<String>,
    pub ongoing_required_tags: Vec<String>,
    pub ongoing_ignored_tags: Vec<String>,
    pub removal_required_tags: Vec<String>,
    pub removal_ignored_tags: Vec<String>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct GameplayEffectModifier {
    pub attribute: String,
    pub operation: String, // "Add", "Multiply", "Divide", "Override"
    pub magnitude: f32,
    pub span: Span,
}

// === GAMEPLAY CUES (UE5 Gameplay Ability System - Cosmetic Events) ===

/// Gameplay cue definition (cosmetic events)
/// Syntax: @gameplay_cue struct Name: tag, type, lifecycle_hooks
#[derive(Debug, Clone, PartialEq)]
pub struct GameplayCueDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub tag: String,
    pub cue_type: CueType,
    pub auto_destroy: bool,
    pub state_fields: Vec<Field>,
    pub on_execute: Option<Function>,
    pub on_add: Option<Function>,
    pub on_remove: Option<Function>,
    pub while_active: Option<Function>,
    pub span: Span,
}

/// Cue type (Static or Actor)
#[derive(Debug, Clone, PartialEq)]
pub enum CueType {
    Static,
    Actor,
}

impl Default for CueType {
    fn default() -> Self {
        CueType::Static
    }
}

// === ABILITY TASKS (UE5 Gameplay Ability System - Async Operations) ===

/// Ability Task Definition
/// Syntax: @ability_task struct Name: delegates, state, lifecycle_hooks
#[derive(Debug, Clone, PartialEq)]
pub struct AbilityTaskDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub delegates: Vec<TaskDelegateDef>,
    pub state_fields: Vec<Field>,
    pub activate_method: Option<Function>,
    pub on_destroy_method: Option<Function>,
    pub custom_methods: Vec<Function>,
    pub span: Span,
}

/// Ability Task Delegate Definition
#[derive(Debug, Clone, PartialEq)]
pub struct TaskDelegateDef {
    pub name: String,
    pub delegate_type: String,
    pub span: Span,
}

// === TARGET ACTORS (UE5 Gameplay Ability System - Targeting System) ===

/// Target Actor Definition
/// Syntax: @target_actor struct Name: trace_type, filters, reticle
#[derive(Debug, Clone, PartialEq)]
pub struct TargetActorDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub trace_type: TraceType,
    pub max_range: Option<f64>,
    pub trace_channel: Option<String>,
    pub filter: Option<TargetFilter>,
    pub reticle_class: Option<String>,
    pub custom_methods: Vec<Function>,
    pub span: Span,
}

/// Trace Type
#[derive(Debug, Clone, PartialEq)]
pub enum TraceType {
    Line,
    Sphere,
    Cone,
    Box,
    Cylinder,
}

impl Default for TraceType {
    fn default() -> Self {
        TraceType::Line
    }
}

/// Target Filter
#[derive(Debug, Clone, PartialEq)]
pub struct TargetFilter {
    pub self_filter: Option<String>,
    pub required_actor_class: Option<String>,
    pub require_tags: Vec<String>,
    pub ignore_tags: Vec<String>,
    pub custom_filter_method: Option<Function>,
    pub span: Span,
}
