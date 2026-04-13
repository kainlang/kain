# KAIN Parser & AST Architecture Guide

> **For LLMs working on the KAIN compiler**  
> **Last Updated:** 2026-02-19  
> **Purpose:** Understand parser/AST structure to add new syntax, attributes, and language features

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Token Flow](#token-flow)
3. [Parser Structure](#parser-structure)
4. [AST Structure](#ast-structure)
5. [Adding New Syntax](#adding-new-syntax)
6. [Adding New Attributes](#adding-new-attributes)
7. [Common Patterns](#common-patterns)
8. [Testing](#testing)

---

## Architecture Overview

### The Three-Stage Pipeline

```
Source Code → Lexer → Tokens → Parser → AST → Type Checker → Codegen
```

**Files:**
- `crates/kain-core/src/lexer.rs` - Tokenization + indentation processing
- `crates/kain-core/src/parser.rs` - Recursive descent parser
- `crates/kain-core/src/ast.rs` - AST node definitions
- `crates/kain-core/src/span.rs` - Source location tracking
- `crates/kain-core/src/error.rs` - Error types

### Key Design Principles

1. **Python-style indentation** - INDENT/DEDENT tokens instead of braces
2. **Rust-like syntax** - Type annotations, pattern matching, effects
3. **First-class citizens** - `component`, `shader`, `actor`, `@material_graph`
4. **Attribute-driven** - `@blueprint`, `@component`, `@slate`, etc.
5. **Expression-oriented** - Everything is an expression (if, match, blocks)

---

## Token Flow

### Lexer → Parser

**Lexer responsibilities:**
- Convert source text to tokens
- Process indentation (INDENT/DEDENT)
- Skip comments
- Unescape string literals

**Parser responsibilities:**
- Build AST from token stream
- Validate syntax
- Track source spans for error messages


### Token Types (lexer.rs)

```rust
pub enum TokenKind {
    // Keywords
    Fn, Let, Mut, Var, Const, If, Else, Match, For, While, Loop,
    Return, Await, In, With, As, TypeKw, Struct, Enum, Trait, Impl,
    
    // First-class citizens
    Component, Shader, Actor, State, Spawn, Send, Comptime, Macro,
    
    // Shader stages
    Vertex, Fragment,  // Note: 'compute' is an identifier, not keyword
    
    // Effect keywords
    Pure, Io, AsyncKw, Async, Gpu, Reactive, Unsafe,
    
    // Literals
    Int(i64), Float(f64), String(String), FString(String), Char(String),
    True, False, None,
    
    // Identifiers
    Ident(String),
    
    // Operators
    Plus, Minus, Star, Slash, Percent, Power,
    EqEq, NotEq, Lt, Gt, LtEq, GtEq,
    And, Or, Not,
    Eq, PlusEq, MinusEq, StarEq, SlashEq,
    
    // Punctuation
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Comma, Dot, DotDot, DotDotDot,
    Colon, ColonColon, Semi,
    Arrow, FatArrow, At, Question,
    
    // JSX
    LtSlash,  // </
    
    // Indentation (synthetic)
    Indent, Dedent, Newline(String), Eof,
}
```

### Indentation Processing

The lexer converts Python-style indentation to INDENT/DEDENT tokens:

```python
fn foo():
    let x = 1
    let y = 2
```

Becomes:

```
Fn Ident("foo") LParen RParen Colon
Newline("\n    ") Indent
Let Ident("x") Eq Int(1)
Newline("\n    ")
Let Ident("y") Eq Int(2)
Newline("\n") Dedent
Eof
```

**Rules:**
- Tabs = 4 spaces
- Blank lines ignored
- Multiple DEDENTs emitted when unindenting multiple levels
- Final DEDENT added at EOF

---

## Parser Structure

### Parser State

```rust
pub struct Parser<'a> {
    tokens: &'a [Token],
    pos: usize,
}
```

**Simple state:**
- Token array + position
- No backtracking (recursive descent)
- Lookahead via `peek_kind()`

### Entry Point

```rust
pub fn parse(&mut self) -> KainResult<Program>
```

**Flow:**
1. Parse top-level items (`parse_item()`)
2. Wrap loose statements in implicit `main()` function
3. Return `Program { items, span }`



### Key Parsing Methods

#### Item Parsing

```rust
fn parse_item(&mut self) -> KainResult<Item>
```

**Handles:**
- Attributes (`@attr`)
- Visibility (`pub`)
- Functions, components, shaders, actors
- Structs, enums, traits, impls
- Type aliases, use statements, const declarations
- Material graphs (`@material_graph`)

**Pattern:**
```rust
fn parse_item(&mut self) -> KainResult<Item> {
    let attributes = self.parse_attributes()?;  // Collect @attrs
    
    // Check for @material_graph early
    if attributes.iter().any(|a| a.name == "material_graph") {
        return self.parse_material_graph(attributes);
    }
    
    let vis = self.parse_visibility();  // pub or private
    
    match self.peek_kind() {
        TokenKind::Fn => self.parse_function_with_attrs(vis, attributes),
        TokenKind::Component => self.parse_component_with_attrs(vis, attributes),
        TokenKind::Struct => self.parse_struct_with_attrs(vis, attributes),
        // ... etc
    }
}
```

#### Attribute Parsing

```rust
fn parse_attributes(&mut self) -> KainResult<Vec<Attribute>>
```

**Syntax:** `@name` or `@name(arg1, arg2)`

**Examples:**
```kain
@blueprint
@component
@slate
@material_graph
@slider(min: 0.0, max: 100.0)
@color_picker
```

**Implementation:**
```rust
while self.check(TokenKind::At) {
    self.advance();  // consume @
    let name = self.parse_attribute_name()?;
    
    let args = if self.check(TokenKind::LParen) {
        self.advance();
        let mut arg_list = Vec::new();
        while !self.check(TokenKind::RParen) {
            arg_list.push(self.parse_expr()?);
            if !self.check(TokenKind::RParen) {
                self.expect(TokenKind::Comma)?;
            }
        }
        self.expect(TokenKind::RParen)?;
        arg_list
    } else {
        vec![]
    };
    
    attrs.push(Attribute { name, args, span });
}
```



#### Expression Parsing (Pratt Parser)

```rust
fn parse_expr(&mut self) -> KainResult<Expr>
```

**Precedence climbing:**
```
parse_expr
  └─> parse_assignment
       └─> parse_binary(min_prec)
            └─> parse_unary
                 └─> parse_postfix
                      └─> parse_primary
```

**Precedence levels:**
1. `||`, `or` - Logical OR
2. `&&`, `and` - Logical AND
3. `==`, `!=` - Equality
4. `<`, `>`, `<=`, `>=` - Comparison
5. `+`, `-` - Addition/Subtraction
6. `*`, `/`, `%` - Multiplication/Division
7. `**` - Exponentiation

**Postfix operators:**
- `.field` - Field access
- `[index]` - Index
- `(args)` - Call
- `?` - Try
- `!` - Macro invocation
- `as Type` - Cast

#### Type Parsing

```rust
fn parse_type(&mut self) -> KainResult<Type>
```

**Handles:**
- Named types: `Int`, `String`, `Vec<T>`
- Tuples: `(A, B, C)`
- Unit: `()`
- Arrays: `[T; N]`
- Slices: `[T]`
- References: `&T`, `&mut T`
- Functions: `fn(A, B) -> C`
- Delegates: `delegate(A, B)` (UE5-specific)
- impl Trait: `impl Future`
- Module paths: `Module::Type`

**Example:**
```rust
// Parse: Vec<Option<Int>>
Type::Named {
    name: "Vec",
    generics: vec![
        Type::Named {
            name: "Option",
            generics: vec![
                Type::Named { name: "Int", generics: vec![], span }
            ],
            span
        }
    ],
    span
}
```



#### Block Parsing

```rust
fn parse_block(&mut self) -> KainResult<Block>
```

**Pattern:**
```rust
self.skip_newlines();
self.expect(TokenKind::Indent)?;

let mut stmts = Vec::new();
while !self.check(TokenKind::Dedent) && !self.at_end() {
    self.skip_newlines();
    if self.check(TokenKind::Dedent) { break; }
    stmts.push(self.parse_stmt()?);
    self.skip_newlines();
}

if self.check(TokenKind::Dedent) { self.advance(); }
Ok(Block { stmts, span })
```

**Critical:** Always check for DEDENT before parsing next statement!

#### JSX Parsing

```rust
fn parse_jsx_element(&mut self) -> KainResult<JSXNode>
```

**Syntax:**
```jsx
<div class="container">
    <h1>Title</h1>
    <p>Count is: {count}</p>
</div>
```

**Handles:**
- Self-closing tags: `<img src="..." />`
- Attributes: `name="value"` or `name={expr}`
- Children: text, expressions `{...}`, nested elements
- Whitespace collapsing

**Text handling:**
- Identifiers, keywords, operators → text nodes
- Gaps between tokens → spaces
- `{expr}` → expression nodes

---

## AST Structure

### Top-Level Items

```rust
pub enum Item {
    Function(Function),
    Component(Component),
    Shader(Shader),
    Actor(Actor),
    Struct(Struct),
    Enum(Enum),
    Trait(Trait),
    Impl(Impl),
    TypeAlias(TypeAlias),
    Use(Use),
    Mod(Mod),
    Const(Const),
    Comptime(ComptimeBlock),
    Macro(MacroDef),
    Test(TestDef),
    MaterialGraph(MaterialGraphDef),  // NEW
}
```



### Function AST

```rust
pub struct Function {
    pub name: String,
    pub generics: Vec<Generic>,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub effects: Vec<Effect>,
    pub body: Block,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,  // @blueprint, @inline, etc.
    pub span: Span,
}
```

**Example:**
```kain
@blueprint
pub fn calculate_damage(base: Float, multiplier: Float) -> Float with Pure:
    return base * multiplier
```

### Component AST (React-like UI)

```rust
pub struct Component {
    pub name: String,
    pub props: Vec<Param>,
    pub state: Vec<StateDecl>,
    pub methods: Vec<Function>,
    pub effects: Vec<Effect>,
    pub body: JSXNode,  // Render output
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}
```

**Example:**
```kain
@component
component Counter(initial: Int) with Reactive:
    state count: Int = initial
    
    fn increment():
        count = count + 1
    
    render:
        <div>
            <p>Count: {count}</p>
            <button onclick={increment}>+</button>
        </div>
```

### Shader AST (GPU Programs)

```rust
pub struct Shader {
    pub name: String,
    pub stage: ShaderStage,  // Vertex, Fragment, Compute, Surface
    pub inputs: Vec<Param>,
    pub outputs: Type,
    pub uniforms: Vec<Uniform>,
    pub body: Block,
    pub span: Span,
}

pub struct Uniform {
    pub name: String,
    pub ty: Type,
    pub binding: u32,  // @0, @1, @2, etc.
    pub span: Span,
}
```

**Example:**
```kain
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform intensity: Float @1
    uniform albedo_map: Sampler2D @2
    
    let tex_color = sample(albedo_map, uv).rgb
    let final_color = tex_color * base_color * intensity
    return vec4(final_color, 1.0)
```



### Actor AST (Erlang-style Concurrency)

```rust
pub struct Actor {
    pub name: String,
    pub state: Vec<StateDecl>,
    pub handlers: Vec<MessageHandler>,
    pub methods: Vec<Function>,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct MessageHandler {
    pub message_type: String,  // RPC name (Server_*, Client_*, Multicast_*)
    pub params: Vec<Param>,
    pub body: Block,
    pub span: Span,
}
```

**Example:**
```kain
actor GameMode:
    state score: Int = 0
    state time_remaining: Float = 300.0
    
    on Server_StartMatch():
        score = 0
        Multicast_AnnounceStart()
    
    on Server_AddScore(points: Int):
        score = score + points
```

### Material Graph AST (NEW)

```rust
pub struct MaterialGraphDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub inputs: Vec<MaterialInput>,
    pub body: Vec<MaterialStatement>,
    pub outputs: Vec<MaterialOutput>,
    pub span: Span,
}

pub struct MaterialInput {
    pub name: String,
    pub ty: Type,
    pub default: Option<Expr>,
    pub span: Span,
}

pub struct MaterialOutput {
    pub name: String,  // base_color, emissive, roughness, etc.
    pub value: Expr,
    pub span: Span,
}
```

**Example:**
```kain
@material_graph
material TintedMaterial:
    input base_color: Vec3 = vec3(1, 1, 1)
    input tint: Vec3 = vec3(1, 0.5, 0)
    input roughness: Float = 0.5
    
    let final_color = base_color * tint
    
    output base_color = final_color
    output roughness = roughness
    output metallic = 0.0
```



### Expression AST

```rust
pub enum Expr {
    // Literals
    Int(i64, Span),
    Float(f64, Span),
    String(String, Span),
    FString(Vec<Expr>, Span),  // f"Hello {name}"
    Bool(bool, Span),
    None(Span),
    
    // Identifiers
    Ident(String, Span),
    
    // Operations
    Binary { left: Box<Expr>, op: BinaryOp, right: Box<Expr>, span: Span },
    Unary { op: UnaryOp, operand: Box<Expr>, span: Span },
    
    // Calls
    Call { callee: Box<Expr>, args: Vec<CallArg>, span: Span },
    MethodCall { receiver: Box<Expr>, method: String, args: Vec<CallArg>, span: Span },
    
    // Access
    Field { object: Box<Expr>, field: String, span: Span },
    Index { object: Box<Expr>, index: Box<Expr>, span: Span },
    
    // Assignment
    Assign { target: Box<Expr>, value: Box<Expr>, span: Span },
    
    // Literals
    Struct { name: String, fields: Vec<(String, Expr)>, span: Span },
    EnumVariant { enum_name: String, variant: String, fields: EnumVariantFields, span: Span },
    Array(Vec<Expr>, Span),
    Tuple(Vec<Expr>, Span),
    
    // Control flow
    If { condition: Box<Expr>, then_branch: Block, else_branch: Option<Box<ElseBranch>>, span: Span },
    Match { scrutinee: Box<Expr>, arms: Vec<MatchArm>, span: Span },
    
    // Functions
    Lambda { params: Vec<Param>, return_type: Option<Type>, body: Box<Expr>, span: Span },
    
    // Async/Actor
    Await(Box<Expr>, Span),
    Spawn { actor: String, init: Vec<(String, Expr)>, span: Span },
    SendMsg { target: Box<Expr>, message: String, data: Vec<(String, Expr)>, span: Span },
    
    // Other
    Cast { value: Box<Expr>, target: Type, span: Span },
    Try(Box<Expr>, Span),
    Comptime(Box<Expr>, Span),
    MacroCall { name: String, args: Vec<Expr>, span: Span },
    Block(Block, Span),
    JSX(JSXNode, Span),
    
    // Control flow as expressions
    Return(Option<Box<Expr>>, Span),
    Break(Option<Box<Expr>>, Span),
    Continue(Span),
}
```



---

## Adding New Syntax

### Step-by-Step Guide

#### 1. Add Token (if needed)

**File:** `crates/kain-core/src/lexer.rs`

```rust
#[derive(Logos, Debug, Clone, PartialEq)]
pub enum TokenKind {
    // ... existing tokens
    
    #[token("mynewkeyword")]
    MyNewKeyword,
}
```

**Test:**
```rust
#[test]
fn test_new_keyword() {
    let source = "mynewkeyword";
    let tokens = Lexer::new(source).tokenize().unwrap();
    assert!(matches!(tokens[0].kind, TokenKind::MyNewKeyword));
}
```

#### 2. Add AST Node

**File:** `crates/kain-core/src/ast.rs`

```rust
// Add to Item enum if top-level
pub enum Item {
    // ... existing variants
    MyNewItem(MyNewItemDef),
}

// Define the struct
#[derive(Debug, Clone, PartialEq)]
pub struct MyNewItemDef {
    pub name: String,
    pub some_field: Type,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}
```

#### 3. Add Parser Method

**File:** `crates/kain-core/src/parser.rs`

```rust
fn parse_my_new_item(&mut self) -> KainResult<Item> {
    let start = self.current_span();
    self.expect(TokenKind::MyNewKeyword)?;
    
    let name = self.parse_ident()?;
    self.expect(TokenKind::Colon)?;
    let some_field = self.parse_type()?;
    
    Ok(Item::MyNewItem(MyNewItemDef {
        name,
        some_field,
        attributes: vec![],
        span: start.merge(self.current_span()),
    }))
}
```

#### 4. Wire into parse_item()

```rust
fn parse_item(&mut self) -> KainResult<Item> {
    let attributes = self.parse_attributes()?;
    let vis = self.parse_visibility();
    
    match self.peek_kind() {
        // ... existing cases
        TokenKind::MyNewKeyword => self.parse_my_new_item(),
        _ => Err(KainError::parser("Expected item", self.current_span())),
    }
}
```

#### 5. Add Codegen

**File:** `crates/ue5/src/codegen_ue5.rs` or `crates/ue5-editor/src/editor/codegen.rs`

```rust
pub fn gen_my_new_item(item: &MyNewItemDef, ctx: &Ue5Context) -> String {
    format!("// Generated code for {}", item.name)
}
```

#### 6. Test

**File:** `crates/kain-core/tests/parser_tests.rs`

```rust
#[test]
fn test_parse_my_new_item() {
    let source = "mynewkeyword Foo: Int";
    let tokens = Lexer::new(source).tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    match &program.items[0] {
        Item::MyNewItem(def) => {
            assert_eq!(def.name, "Foo");
        }
        _ => panic!("Expected MyNewItem"),
    }
}
```



---

## Adding New Attributes

### Step-by-Step Guide

#### 1. Parse Attribute (Already Handled)

Attributes are parsed automatically by `parse_attributes()`. No lexer changes needed!

```kain
@my_new_attr
@my_new_attr(arg1, arg2)
```

#### 2. Check Attribute in Parser

If the attribute changes parsing behavior:

```rust
fn parse_item(&mut self) -> KainResult<Item> {
    let attributes = self.parse_attributes()?;
    
    // Check for special attribute
    if attributes.iter().any(|a| a.name == "my_new_attr") {
        return self.parse_special_item(attributes);
    }
    
    // ... normal parsing
}
```

#### 3. Pass to AST Node

Attributes are stored in `attributes: Vec<Attribute>` field:

```rust
pub struct Function {
    // ... other fields
    pub attributes: Vec<Attribute>,
}
```

#### 4. Handle in Codegen

**File:** `crates/ue5/src/codegen_ue5.rs`

```rust
pub fn gen_function(func: &Function, ctx: &Ue5Context) -> String {
    let mut output = String::new();
    
    // Check for attribute
    if func.attributes.iter().any(|a| a.name == "my_new_attr") {
        output.push_str("// Special handling\n");
    }
    
    // ... generate function
    output
}
```

#### 5. Extract Attribute Arguments

```rust
// Find attribute
let attr = func.attributes.iter()
    .find(|a| a.name == "slider")
    .ok_or_else(|| error("Missing @slider attribute"))?;

// Extract named argument
fn extract_named_arg(args: &[Expr], name: &str) -> Option<f64> {
    for arg in args {
        if let Expr::Call { callee, args: inner_args, .. } = arg {
            if let Expr::Ident(id, _) = &**callee {
                if id == name && !inner_args.is_empty() {
                    if let Expr::Float(val, _) = &inner_args[0].value {
                        return Some(*val);
                    }
                }
            }
        }
    }
    None
}

let min = extract_named_arg(&attr.args, "min").unwrap_or(0.0);
let max = extract_named_arg(&attr.args, "max").unwrap_or(100.0);
```



---

## Common Patterns

### Pattern 1: Indented Block Parsing

```rust
fn parse_my_block(&mut self) -> KainResult<Vec<Item>> {
    self.expect(TokenKind::Colon)?;
    self.skip_newlines();
    self.expect(TokenKind::Indent)?;
    
    let mut items = Vec::new();
    while !self.check(TokenKind::Dedent) && !self.at_end() {
        self.skip_newlines();
        if self.check(TokenKind::Dedent) { break; }
        
        items.push(self.parse_item()?);
        self.skip_newlines();
    }
    
    if self.check(TokenKind::Dedent) { self.advance(); }
    Ok(items)
}
```

**Critical:** Always `skip_newlines()` before checking `Dedent`!

### Pattern 2: Optional Syntax

```rust
// Optional type annotation: name [: Type]
let ty = if self.check(TokenKind::Colon) {
    self.advance();
    Some(self.parse_type()?)
} else {
    None
};

// Optional default value: param [= expr]
let default = if self.check(TokenKind::Eq) {
    self.advance();
    Some(self.parse_expr()?)
} else {
    None
};
```

### Pattern 3: Comma-Separated Lists

```rust
fn parse_list(&mut self) -> KainResult<Vec<Item>> {
    let mut items = Vec::new();
    
    while !self.check(TokenKind::RParen) && !self.at_end() {
        items.push(self.parse_item()?);
        
        if !self.check(TokenKind::RParen) {
            self.expect(TokenKind::Comma)?;
        }
    }
    
    Ok(items)
}
```

**Trailing comma support:** Check for closing delimiter before expecting comma.

### Pattern 4: Lookahead for Disambiguation

```rust
// Distinguish between:
// - Ident (variable)
// - Ident { ... } (struct literal)
// - Ident::Variant (enum variant)

if let TokenKind::Ident(name) = self.peek_kind() {
    self.advance();
    
    if self.check(TokenKind::ColonColon) {
        // Enum variant
        self.advance();
        let variant = self.parse_ident()?;
        // ...
    } else if self.check(TokenKind::LBrace) {
        // Struct literal
        self.advance();
        // ...
    } else {
        // Just an identifier
        return Ok(Expr::Ident(name, span));
    }
}
```



### Pattern 5: Span Tracking

```rust
fn parse_item(&mut self) -> KainResult<Item> {
    let start = self.current_span();  // Capture start
    
    // ... parse item
    
    let end = self.current_span();  // Capture end
    Ok(Item::MyItem(MyItemDef {
        // ... fields
        span: start.merge(end),  // Merge spans
    }))
}
```

**Why spans matter:**
- Error messages show exact location
- IDE features (go-to-definition, hover)
- Source maps for debugging

### Pattern 6: Error Recovery

```rust
fn parse_item(&mut self) -> KainResult<Item> {
    match self.peek_kind() {
        TokenKind::Fn => self.parse_function(),
        TokenKind::Struct => self.parse_struct(),
        _ => Err(KainError::parser(
            format!("Expected item, got {:?}", self.peek_kind()),
            self.current_span()
        )),
    }
}
```

**Best practices:**
- Include expected token in error message
- Include actual token in error message
- Include span for precise location

### Pattern 7: Handling Inline vs Block Syntax

```rust
// Support both:
// if cond: stmt
// if cond:
//     block

let is_block = matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent);

let body = if is_block {
    self.parse_block()?
} else {
    // Inline: parse single statement
    let stmt = self.parse_stmt()?;
    Block { stmts: vec![stmt], span }
};
```

---

## Testing

### Unit Tests

**File:** `crates/kain-core/tests/parser_tests.rs`

```rust
#[test]
fn test_parse_function() {
    let source = r#"
fn add(a: Int, b: Int) -> Int:
    return a + b
"#;
    let tokens = Lexer::new(source).tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    match &program.items[0] {
        Item::Function(func) => {
            assert_eq!(func.name, "add");
            assert_eq!(func.params.len(), 2);
            assert!(func.return_type.is_some());
        }
        _ => panic!("Expected function"),
    }
}
```



### Integration Tests

**File:** `testing/Phase3/SlateTest4/ultimate.kn`

```kain
@material_graph
material TestMaterial:
    input base_color: Vec3 = vec3(1, 1, 1)
    output base_color = base_color
```

**Build:**
```bash
cd testing/Phase3/SlateTest4
kain build --ue5
```

### Test Checklist

- [ ] Lexer tokenizes correctly
- [ ] Parser builds correct AST
- [ ] Spans are accurate
- [ ] Error messages are clear
- [ ] Codegen produces valid C++
- [ ] UE5 compiles without errors
- [ ] Runtime behavior is correct

---

## Files LLMs Will Touch

### When to Modify Each File

| File | When to Modify |
|------|----------------|
| `lexer.rs` | Adding new keywords, operators, or literal types |
| `ast.rs` | Adding new AST node types or fields |
| `parser.rs` | Adding new syntax, parsing logic, or grammar rules |
| `codegen_ue5.rs` | Adding runtime codegen (actors, components, structs) |
| `editor/codegen.rs` | Adding editor codegen (Slate, Details, Viewports) |
| `packager.rs` | Changing build orchestration or file output |

### Typical Workflow

1. **Add keyword** → `lexer.rs`
2. **Add AST node** → `ast.rs`
3. **Add parser method** → `parser.rs`
4. **Wire into parse_item()** → `parser.rs`
5. **Add codegen** → `codegen_ue5.rs` or `editor/codegen.rs`
6. **Test** → `parser_tests.rs` + integration test

---

## Quick Reference

### Parser Helper Methods

```rust
// Token inspection
fn peek_kind(&self) -> TokenKind
fn current_span(&self) -> Span
fn at_end(&self) -> bool
fn check(&self, k: TokenKind) -> bool
fn check_line_end(&self) -> bool

// Token consumption
fn advance(&mut self)
fn expect(&mut self, k: TokenKind) -> KainResult<()>

// Whitespace handling
fn skip_newlines(&mut self)
fn skip_formatting(&mut self)  // Skip newlines + indent/dedent

// Parsing
fn parse_ident(&mut self) -> KainResult<String>
fn parse_type(&mut self) -> KainResult<Type>
fn parse_expr(&mut self) -> KainResult<Expr>
fn parse_block(&mut self) -> KainResult<Block>
fn parse_pattern(&mut self) -> KainResult<Pattern>
```

### Common Errors

**❌ Forgot to skip newlines:**
```rust
self.expect(TokenKind::Indent)?;
// Missing: self.skip_newlines();
while !self.check(TokenKind::Dedent) { ... }
```

**✅ Correct:**
```rust
self.expect(TokenKind::Indent)?;
self.skip_newlines();  // Always skip after Indent!
while !self.check(TokenKind::Dedent) { ... }
```

**❌ Forgot to check Dedent before parsing:**
```rust
while !self.check(TokenKind::Dedent) {
    items.push(self.parse_item()?);  // May fail on Dedent
}
```

**✅ Correct:**
```rust
while !self.check(TokenKind::Dedent) && !self.at_end() {
    self.skip_newlines();
    if self.check(TokenKind::Dedent) { break; }  // Double-check!
    items.push(self.parse_item()?);
}
```

---

## Summary

**Key Takeaways:**

1. **Lexer** converts source → tokens, handles indentation
2. **Parser** converts tokens → AST, recursive descent
3. **AST** represents program structure, passed to codegen
4. **Attributes** are first-class, parsed automatically
5. **Indentation** requires careful INDENT/DEDENT handling
6. **Spans** track source locations for errors
7. **Testing** at every layer: lexer, parser, codegen, UE5

**When adding new syntax:**
- Start with lexer (if new keyword)
- Add AST node
- Add parser method
- Wire into parse_item()
- Add codegen
- Test thoroughly

**The parser is the gateway to all KAIN features. Master it, and you control the language.**

