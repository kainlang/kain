# Parser Test Specification — Stream DELTA

**Purpose:** Test cases for the recursive-descent + Pratt parser that converts `Array<Token>` into flat `Array<AstNode>`.
**Format:** Markscript-compatible table format. Each case compiles a source snippet and asserts the expected AST structure.

---

## 1. Item Parsing — Hard Keywords

| Case | Source | Expected AST Kind | Expected Children |
|------|--------|-------------------|-------------------|
| fn simple | `fn foo() -> Int: 42` | AST_ITEM_FUNCTION | name="foo", param_count=0, ret=Int, body=Int(42) |
| fn with params | `fn add(a: Int, b: Int) -> Int with Pure: return a + b` | AST_ITEM_FUNCTION | name="add", param_count=2, effects=[Pure], body=Return |
| fn generic | `fn id<T>(x: T) -> T: return x` | AST_ITEM_FUNCTION | name="id", generic_count=1, param_count=1, body=Return |
| fn async | `async fn fetch() -> Int with Async: return 1` | AST_ITEM_FUNCTION | name="fetch", is_async=1, effects=[Async] |
| fn with where | `fn foo<T>(x: T) -> T where T: Default: return x` | AST_ITEM_FUNCTION | name="foo", where_clause present |
| struct simple | `struct Point: x: Float y: Float` | AST_ITEM_STRUCT | name="Point", field_count=2 |
| struct generic | `struct Vec<T>: data: ptr<T> len: Int` | AST_ITEM_STRUCT | name="Vec", generic_count=1, field_count=2 |
| enum simple | `enum Color: Red Green Blue` | AST_ITEM_ENUM | name="Color", variant_count=3 |
| enum with payload | `enum Option<T>: Some(value: T) None` | AST_ITEM_ENUM | name="Option", variant_count=2 |
| trait simple | `trait Metric: fn score(self: Self_) -> Int` | AST_ITEM_TRAIT | name="Metric", body present |
| impl for type | `impl Point: fn new(x: Float, y: Float) -> Point: return Point { x: x, y: y }` | AST_ITEM_IMPL | trait=-1, type="Point", body present |
| impl trait | `impl Metric for Point: fn score(self: Self_) -> Int: return 0` | AST_ITEM_IMPL | trait="Metric", type="Point" |
| type alias | `type Score = Int` | AST_ITEM_TYPE_ALIAS | name="Score", aliased=Int |
| use path | `use std::math::sin` | AST_ITEM_USE | segments=["std","math","sin"] |
| mod simple | `mod util: fn helper() -> Int: return 0` | AST_ITEM_MOD | name="util", body present |
| const | `const MAX: Int = 100` | AST_ITEM_CONST | name="MAX", type=Int, value=100 |
| test | `test fn my_test(): assert 1 == 1` | AST_ITEM_TEST | body present |

## 2. Item Parsing — Contextual Keywords

| Case | Source | Expected AST Kind |
|------|--------|-------------------|
| patch | `patch commit(auth: World) -> Int: auth.epoch = auth.epoch + 1 return 0` | AST_ITEM_PATCH |
| law | `law valid(v: Int) -> Bool: return v >= 0` | AST_ITEM_LAW |
| axiom | `axiom truth: when target("llvm") guarantee "works"` | AST_ITEM_AXIOM |
| converge | `converge select(v: Int) -> Int: spec ref: return v fast avx when capability("avx2"): return v * 2 verify random(4)` | AST_ITEM_CONVERGE |
| world | `world AppState: state count: Int = 0 state name: String = ""` | AST_ITEM_WORLD |
| entangle | `entangle A.x <-> B.y with single_writer` | AST_ITEM_ENTANGLE |
| orchestrate | `orchestrate pipeline(v: Int) -> Int: stage s1: cpu v + 1 return 0` | AST_ITEM_ORCHESTRATE |
| pulse | `pulse clock every 16ms jitter 2ms: tick() return 0` | AST_ITEM_PULSE |
| resonate | `resonate Signal.value dampen 0ms: handle() return 0` | AST_ITEM_RESONATE |
| shatter struct | `shatter struct Particle: x: Float y: Float vx: Float vy: Float` | AST_ITEM_STRUCT |
| include | `include <stdio.h> as libc` | AST_ITEM_IMPORT |
| import | `import json as py_json` | AST_ITEM_IMPORT |
| from import | `from json import dumps` | AST_ITEM_IMPORT |
| component | `component Button(label: String): render <text value={label} />` | AST_ITEM_COMPONENT |
| actor | `actor Relay: state bias: Int = 7 on Compute(payload: Int): return payload + bias` | AST_ITEM_ACTOR |
| shader compute | `shader compute Kernel(id: UVec3) -> Void workgroup(8, 8, 1): uniform data: StorageBuffer<UInt> @0 data[id.x] = UInt(1)` | AST_ITEM_SHADER |

## 3. Statement Parsing

| Case | Source | Expected AST Kind |
|------|--------|-------------------|
| let simple | `let x: Int = 42` | AST_STMT_LET |
| let mut | `let mut counter: Int = 0` | AST_STMT_LET (mut=1) |
| var | `var acc: Int = 0` | AST_STMT_LET |
| return | `return 42` | AST_STMT_RETURN |
| return void | `return` | AST_STMT_RETURN (value=-1) |
| defer | `defer cleanup()` | AST_STMT_DEFER |
| for-range | `for i in 0..10: acc = acc + i` | AST_STMT_FOR |
| for-array | `for x in arr: sum = sum + x` | AST_STMT_FOR |
| fanout | `fanout w in 0..4: lane[w] = compute(w)` | AST_STMT_FANOUT |
| while | `while i < 10: i = i + 1` | AST_STMT_WHILE |
| loop | `loop: if done: break 0` | AST_STMT_LOOP |
| break | `break` | AST_STMT_BREAK |
| break with value | `break 42` | AST_STMT_BREAK (value=42) |
| continue | `continue` | AST_STMT_CONTINUE |
| dispatch | `dispatch "shader::Kernel::compute" [64, 1, 1]` | AST_STMT_DISPATCH |
| expr stmt | `compute(1, 2)` | AST_STMT_EXPR |

## 4. Pratt Expression Precedence (16 levels)

| Case | Source | Expected Parse Tree |
|------|--------|-------------------|
| add-mul (level 9 vs 10) | `1 + 2 * 3` | `(+ 1 (* 2 3))` — mul binds tighter |
| mul-add (level 10 vs 9) | `1 * 2 + 3` | `(+ (* 1 2) 3)` — left-assoc |
| and-or (level 2 vs 1) | `a && b \|\| c` | `(\|\| (&& a b) c)` |
| or-and (level 1 vs 2) | `a \|\| b && c` | `(\|\| a (&& b c))` |
| compare-add (level 7 vs 9) | `a + b < c + d` | `(< (+ a b) (+ c d))` |
| power-right (level 11) | `2 ** 3 ** 2` | `(** 2 (** 3 2))` — right-assoc |
| eq-compare (level 6 vs 7) | `a == b < c` | `(== a (< b c))` |
| shift-add (level 8 vs 9) | `a << b + c` | `(<< a (+ b c))` |
| bitwise (levels 3-5) | `a | b & c ^ d` | `(\| a (& b (^ c d)))` |
| unary-binary | `-a + b` | `(+ (- a) b)` |
| paren override | `(1 + 2) * 3` | `(* (+ 1 2) 3)` |

## 5. Expression Variants

| Case | Source | Expected AST Kind |
|------|--------|-------------------|
| int literal | `42` | AST_EXPR_INT (value=42) |
| float literal | `3.14` | AST_EXPR_FLOAT |
| string literal | `"hello"` | AST_EXPR_STRING |
| bool true | `true` | AST_EXPR_BOOL (value=1) |
| bool false | `false` | AST_EXPR_BOOL (value=0) |
| none literal | `none` | AST_EXPR_NONE |
| ident | `my_var` | AST_EXPR_IDENT |
| binary | `a + b` | AST_EXPR_BINARY (op=ADD) |
| unary neg | `-x` | AST_EXPR_UNARY (op=NEG) |
| unary not | `!x` | AST_EXPR_UNARY (op=NOT) |
| call no args | `foo()` | AST_EXPR_CALL (arg_count=0) |
| call with args | `add(1, 2)` | AST_EXPR_CALL (arg_count=2) |
| method call | `obj.method(1, 2)` | AST_EXPR_METHOD_CALL |
| field access | `obj.field` | AST_EXPR_FIELD |
| index | `arr[0]` | AST_EXPR_INDEX |
| assignment | `x = 42` | AST_EXPR_ASSIGN |
| compound assign | `x += 1` | AST_EXPR_ASSIGN (desugared to x = x + 1) |
| if expression | `if x: 1 else: 2` | AST_EXPR_IF |
| match | `match v: 1 => "one" 2 => "two"` | AST_EXPR_MATCH |
| block | `{ let x = 1 return x }` | AST_EXPR_BLOCK |
| range | `0..10` | AST_EXPR_RANGE |
| range inclusive | `0..=10` | AST_EXPR_RANGE |
| struct literal | `Point { x: 1.0, y: 2.0 }` | AST_EXPR_STRUCT_LIT |
| array literal | `[1, 2, 3]` | AST_EXPR_ARRAY |
| tuple | `(1, "hello")` | AST_EXPR_TUPLE |
| ref | `&x` | AST_EXPR_REF |
| ref mut | `&mut x` | AST_EXPR_REF |
| deref | `*ptr` | AST_EXPR_DEREF |
| cast | `x as Float` | AST_EXPR_CAST |
| try | `result?` | AST_EXPR_TRY |
| await | `await fut` | AST_EXPR_AWAIT |
| lambda | `fn(x: Int) -> Int: return x + 1` | AST_EXPR_LAMBDA |
| spawn | `spawn Actor(bias = 7)` | AST_EXPR_SPAWN |
| send | `send actor.Msg(val = 1)` | AST_EXPR_SEND |
| emit | `emit Event(val = 1)` | AST_EXPR_EMIT |
| collapse | `collapse ptr: store(ptr, 42)` | AST_EXPR_COLLAPSE |
| observe | `observe ptr: load(ptr)` | AST_EXPR_OBSERVE |
| decay | `decay ptr` | AST_EXPR_DECAY |
| share | `share buffer: fanout w in 0..4: store(slot, w)` | AST_EXPR_SHARE |
| teleport | `teleport val from A to B via bus` | AST_EXPR_TELEPORT |
| macro call | `my_macro!(arg)` | AST_EXPR_MACRO_CALL |
| paren | `(1 + 2)` | AST_EXPR_PAREN |
| jsx self-close | `<panel />` | AST_EXPR_JSX |
| jsx with attrs | `<box width={100} />` | AST_EXPR_JSX |
| jsx with children | `<stack><text value="a" /><text value="b" /></stack>` | AST_EXPR_JSX |

## 6. JSX Parsing

| Case | Source | Expected |
|------|--------|----------|
| self-closing | `<panel />` | tag="panel", attr_count=0, child_count=0 |
| with string attr | `<text value="hello" />` | tag="text", attr name="value", attr value="hello" |
| with expr attr | `<box width={100 + 50} />` | tag="box", attr name="width", attr value=expr |
| nested | `<stack><panel /><text value="x" /></stack>` | tag="stack", child_count=2 |
| text content | `<text>hello world</text>` | tag="text", one string child "hello world" |
| expression child | `<box>{compute(1)}</box>` | tag="box", child is expr |
| mixed | `<stack><text>hello</text>{value}<text>bye</text></stack>` | tag="stack", 3 children (string, expr, string) |

## 7. Type Parsing

| Case | Source | Expected |
|------|--------|----------|
| named | `Int` | AST_TYPE_NAMED name="Int" |
| generic | `Vec<Int>` | AST_TYPE_NAMED name="Vec" generic=[Int] |
| nested generic | `Option<Vec<Int>>` | AST_TYPE_NAMED name="Option" generic=[Vec<Int>] |
| ptr | `ptr<Int>` | AST_TYPE_PTR inner=Int |
| array | `[Int; 16]` | AST_TYPE_ARRAY inner=Int len=16 |
| slice | `[Int]` | AST_TYPE_SLICE inner=Int |
| ref | `&Int` | AST_TYPE_REF inner=Int |
| ref mut | `&mut Int` | AST_TYPE_REF inner=Int mutable=1 |
| tuple | `(Int, Float)` | AST_TYPE_TUPLE elements=[Int, Float] |

## 8. Error Recovery

| Case | Source | Expected Behavior |
|------|--------|-------------------|
| missing ident | `let = 42` | Error: expected identifier, recovers |
| missing colon | `fn foo() -> Int 42` | Error: expected ':', recovers |
| unclosed paren | `fn foo(): (1 + 2` | Error: expected ')', recovers |
| garbage | `@#$%^` | Error: unexpected character, recovers |
| reserved keyword as ident | `let fn: Int = 1` | Error: reserved identifier |
| max errors | 55 consecutive errors | Bails at 50 errors |

## 9. Generics

| Case | Source | Expected |
|------|--------|----------|
| single generic | `<T>` | generic_params=[T, -1] |
| bounded | `<T: Numeric>` | generic_params=[T, Numeric] |
| multiple | `<T, U>` | generic_params=[T, -1, U, -1] |
| >> injection | `<Vec<Vec<Int>>>` | handled by splitting Shr into Gt tokens |

---

## Test Count Summary

- **Item parsing (hard keywords):** 17 cases
- **Item parsing (contextual keywords):** 16 cases
- **Statement parsing:** 16 cases
- **Pratt precedence:** 11 cases
- **Expression variants:** 38 cases
- **JSX parsing:** 7 cases
- **Type parsing:** 9 cases
- **Error recovery:** 6 cases
- **Generics:** 4 cases

**Total: 124 test cases**

---

*Generated by Stream DELTA — Parser + AST Implementation*
