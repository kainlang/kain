# KAIN Standard Library Implementation Guide

> **Last Updated:** Feb 19, 2026  
> **Purpose:** Complete reference for implementing KAIN stdlib across all backends  
> **Status:** Research complete — 80+ functions cataloged, UE5 mappings identified

---

## Executive Summary

KAIN has a comprehensive standard library with **80+ built-in functions** spanning math, collections, strings, I/O, HTTP, JSON, async, and Python FFI. Currently:

- ✅ **Interpreter (runtime.rs):** All 80+ functions fully implemented
- ⚠️ **UE5 Backend:** Only `print`/`println` mapped, math functions partially mapped in codegen
- ❌ **WASM Backend:** No stdlib support
- ❌ **LLVM Backend:** No stdlib support
- ❌ **Rust Backend:** No stdlib support

**This document provides the roadmap to achieve full stdlib parity across all backends.**

---

## Table of Contents

1. [Function Inventory](#1-function-inventory)
2. [Backend Mapping Tables](#2-backend-mapping-tables)
3. [Implementation Priority](#3-implementation-priority)
4. [Code Generation Strategy](#4-code-generation-strategy)
5. [UE5 Deep Dive](#5-ue5-deep-dive)
6. [Runtime Linking Strategy](#6-runtime-linking-strategy)
7. [Testing Strategy](#7-testing-strategy)

---

## 1. Function Inventory

Complete catalog of all 80+ KAIN standard library functions organized by category.

### 1.1 Math Functions (20 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `abs` | `(Int\|Float) -> Int\|Float` | Absolute value | ✅ | `FMath::Abs` |
| `sqrt` | `(Float) -> Float` | Square root | ✅ | `FMath::Sqrt` |
| `pow` | `(Float, Float) -> Float` | Power | ❌ | `FMath::Pow` |
| `sin` | `(Float) -> Float` | Sine | ✅ | `FMath::Sin` |
| `cos` | `(Float) -> Float` | Cosine | ✅ | `FMath::Cos` |
| `tan` | `(Float) -> Float` | Tangent | ✅ | `FMath::Tan` |
| `asin` | `(Float) -> Float` | Arcsine | ❌ | `FMath::Asin` |
| `acos` | `(Float) -> Float` | Arccosine | ❌ | `FMath::Acos` |
| `atan` | `(Float) -> Float` | Arctangent | ❌ | `FMath::Atan` |
| `atan2` | `(Float, Float) -> Float` | Two-argument arctangent | ❌ | `FMath::Atan2` |
| `floor` | `(Float) -> Int` | Floor | ❌ | `FMath::Floor` |
| `ceil` | `(Float) -> Int` | Ceiling | ❌ | `FMath::CeilToFloat` |
| `round` | `(Float) -> Int` | Round to nearest | ❌ | `FMath::RoundToFloat` |
| `min` | `(Int, Int) -> Int` | Minimum of two values | ✅ | `FMath::Min` |
| `max` | `(Int, Int) -> Int` | Maximum of two values | ✅ | `FMath::Max` |
| `clamp` | `(Int, Int, Int) -> Int` | Clamp between bounds | ❌ | `FMath::Clamp` |
| `lerp` / `mix` | `(Float, Float, Float) -> Float` | Linear interpolation | ❌ | `FMath::Lerp` |
| `smoothstep` | `(Float, Float, Float) -> Float` | Smooth step interpolation | ❌ | `FMath::SmoothStep` |
| `random` / `rand` | `() -> Float` | Random float [0,1) | ✅ | `FMath::FRand()` |
| `random_range` | `(Float, Float) -> Float` | Random in range | ❌ | `FMath::FRandRange` |

### 1.2 Vector Math Functions (10 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `vec2` | `(Float, Float) -> Vec2` | Create 2D vector | ❌ | `FVector2D(x, y)` |
| `vec3` | `(Float, Float, Float) -> Vec3` | Create 3D vector | ❌ | `FVector(x, y, z)` |
| `vec4` | `(Float, Float, Float, Float) -> Vec4` | Create 4D vector | ❌ | `FVector4(x, y, z, w)` |
| `dot` | `(Vec3, Vec3) -> Float` | Dot product | ❌ | `FVector::DotProduct` |
| `cross` | `(Vec3, Vec3) -> Vec3` | Cross product | ❌ | `FVector::CrossProduct` |
| `normalize` | `(Vec3) -> Vec3` | Normalize vector | ❌ | `.GetSafeNormal()` |
| `length` | `(Vec3) -> Float` | Vector length | ❌ | `.Size()` |
| `distance` | `(Vec3, Vec3) -> Float` | Distance between points | ❌ | `FVector::Dist` |
| `reflect` | `(Vec3, Vec3) -> Vec3` | Reflect vector | ❌ | `FMath::GetReflectionVector` |
| `refract` | `(Vec3, Vec3, Float) -> Vec3` | Refract vector | ❌ | Custom impl |

### 1.3 Collection Functions (12 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `len` | `(Array\|String) -> Int` | Get length | ✅ | `.Num()` / `.Len()` |
| `push` | `(Array, Any) -> Unit` | Push to array | ✅ | `.Add(value)` |
| `pop` | `(Array) -> Any` | Pop from array | ✅ | `.Pop()` |
| `map` | `(Array, Function) -> Array` | Map over array | ✅ | Custom loop |
| `filter` | `(Array, Function) -> Array` | Filter array | ✅ | Custom loop |
| `reduce` | `(Array, Any, Function) -> Any` | Reduce array | ✅ | Custom loop |
| `foreach` | `(Array, Function) -> Unit` | Iterate over array | ✅ | `for` loop |
| `range` | `(Int, Int) -> Array` | Create range | ✅ | Custom loop |
| `first` | `(Array) -> Any` | Get first element | ✅ | `[0]` |
| `last` | `(Array) -> Any` | Get last element | ✅ | `[Num()-1]` |
| `reverse` | `(Array) -> Array` | Reverse array | ✅ | `Algo::Reverse` |
| `sum` | `(Array<Int>) -> Int` | Sum integers | ✅ | Custom loop |

### 1.4 String Functions (15 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `split` | `(String, String) -> Array` | Split string | ✅ | `.ParseIntoArray` |
| `join` | `(Array, String) -> String` | Join array to string | ✅ | `FString::Join` |
| `trim` | `(String) -> String` | Trim whitespace | ✅ | `.TrimStartAndEnd()` |
| `to_upper` / `upper` | `(String) -> String` | To uppercase | ✅ | `.ToUpper()` |
| `to_lower` / `lower` | `(String) -> String` | To lowercase | ✅ | `.ToLower()` |
| `contains` | `(String, String) -> Bool` | Check contains | ✅ | `.Contains()` |
| `starts_with` | `(String, String) -> Bool` | Check starts with | ✅ | `.StartsWith()` |
| `ends_with` | `(String, String) -> Bool` | Check ends with | ✅ | `.EndsWith()` |
| `replace` | `(String, String, String) -> String` | Replace substring | ✅ | `.Replace()` |
| `char_at` | `(String, Int) -> String` | Get character at index | ✅ | `[index]` |
| `substring` | `(String, Int, Int?) -> String` | Extract substring | ✅ | `.Mid()` |
| `ord` | `(String) -> Int` | Get ASCII/Unicode code | ✅ | `(int32)str[0]` |
| `chr` | `(Int) -> String` | Convert code to character | ✅ | `FString::Chr` |
| `to_string` / `str` | `(Any) -> String` | Convert to string | ✅ | `FString::Printf` |

### 1.5 Type Conversion Functions (5 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `int` / `to_int` | `(Any) -> Int` | Convert to int | ✅ | `FCString::Atoi` |
| `float` | `(Any) -> Float` | Convert to float | ✅ | `FCString::Atof` |
| `bool` | `(Any) -> Bool` | Convert to bool | ✅ | `!!value` |
| `str` / `to_string` | `(Any) -> String` | Convert to string | ✅ | `FString::Printf` |
| `type_of` | `(Any) -> String` | Get type name | ✅ | Runtime only |

### 1.6 I/O Functions (5 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `print` | `(Any) -> Unit` | Print to console | ✅ | `UE_LOG(LogTemp, Warning, TEXT("%s"), *str)` |
| `println` | `(Any) -> Unit` | Print with newline | ✅ | `UE_LOG(LogTemp, Warning, TEXT("%s"), *str)` |
| `read_line` | `() -> String` | Read line from stdin | ✅ | Not applicable (UE5) |
| `read_file` | `(String) -> String` | Read file contents | ✅ | `FFileHelper::LoadFileToString` |
| `write_file` | `(String, String) -> Unit` | Write to file | ✅ | `FFileHelper::SaveStringToFile` |
| `file_exists` | `(String) -> Bool` | Check file exists | ✅ | `FPaths::FileExists` |

### 1.7 HTTP Functions (2 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `http_get` | `(String) -> String` | HTTP GET request | ✅ | `FHttpModule::Get()->CreateRequest()` |
| `http_post_json` | `(String, String) -> String` | HTTP POST JSON | ✅ | `FHttpModule::Get()->CreateRequest()` |

### 1.8 JSON Functions (2 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `json_parse` | `(String) -> Any` | Parse JSON string | ✅ | `FJsonSerializer::Deserialize` |
| `json_string` | `(Any) -> String` | Convert to JSON string | ✅ | `FJsonSerializer::Serialize` |

### 1.9 HashMap Functions (3 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `map_new` | `() -> Map` | Create new map | ✅ | `TMap<FString, T>()` |
| `map_set` | `(Map, String, Any) -> Unit` | Set map key | ✅ | `.Add(key, value)` |
| `map_get` | `(Map, String) -> Any` | Get map value | ✅ | `.FindRef(key)` |

### 1.10 Time Functions (3 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `now` | `() -> Float` | Current time in seconds | ✅ | `FPlatformTime::Seconds()` |
| `time` | `() -> Float` | Unix timestamp | ✅ | `FDateTime::UtcNow().ToUnixTimestamp()` |
| `sleep` | `(Float) -> Unit` | Sleep for seconds | ✅ | `FPlatformProcess::Sleep` |

### 1.11 Debug Functions (4 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `dbg` | `(Any) -> Any` | Debug print and return | ✅ | `UE_LOG` + return |
| `assert` | `(Bool, String?) -> Unit` | Assert condition | ✅ | `check(condition)` |
| `panic` | `(String) -> Never` | Panic with message | ✅ | `checkf(false, TEXT("%s"), *msg)` |
| `exit` | `(Int?) -> Never` | Exit program | ✅ | `FGenericPlatformMisc::RequestExit` |

### 1.12 Utility Functions (3 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `env` | `(String) -> String?` | Get environment variable | ✅ | `FPlatformMisc::GetEnvironmentVariable` |
| `variant_of` | `(Enum) -> String` | Get enum variant name | ✅ | Runtime only |
| `variant_field` | `(Enum, Int) -> Any` | Get enum field by index | ✅ | Runtime only |

### 1.13 Actor System Functions (2 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `spawn` | `(Actor) -> ActorRef` | Spawn actor | ✅ | `GetWorld()->SpawnActor` |
| `send` | `(ActorRef, String, ...Any) -> Unit` | Send message to actor | ✅ | RPC call |

### 1.14 Python FFI Functions (3 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `py_eval` | `(String) -> Any` | Evaluate Python expression | ✅ | Not applicable |
| `py_exec` | `(String) -> Unit` | Execute Python code | ✅ | Not applicable |
| `py_import` | `(String) -> Any` | Import Python module | ✅ | Not applicable |

### 1.15 Async Functions (6 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `block_on` | `(Future) -> Any` | Run future to completion | ✅ | Not applicable |
| `spawn_task` | `(Future) -> Unit` | Spawn async task | ✅ | Not applicable |
| `poll_once` | `(Future) -> Poll` | Poll future once | ✅ | Not applicable |
| `is_ready` | `(Poll) -> Bool` | Check if poll is ready | ✅ | Not applicable |
| `is_pending` | `(Poll) -> Bool` | Check if poll is pending | ✅ | Not applicable |
| `unwrap_ready` | `(Poll) -> Any` | Extract value from ready poll | ✅ | Not applicable |

### 1.16 Result/Error Handling (2 functions)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `ok` | `(Any) -> Result` | Create Ok result | ✅ | Custom struct |
| `err` | `(Any) -> Result` | Create Err result | ✅ | Custom struct |

### 1.17 UI Functions (1 function)

| Function | Signature | Description | Runtime | UE5 Mapping |
|----------|-----------|-------------|---------|-------------|
| `mount` | `(Component, String) -> Unit` | Mount component to DOM | ✅ | Not applicable |

---

## 2. Backend Mapping Tables

Detailed mappings for each backend showing how to implement each stdlib function.

### 2.1 UE5 C++ Backend

#### Math Functions

```cpp
// KAIN: abs(x)
FMath::Abs(x)

// KAIN: sqrt(x)
FMath::Sqrt(x)

// KAIN: pow(base, exp)
FMath::Pow(base, exp)

// KAIN: sin(x), cos(x), tan(x)
FMath::Sin(x), FMath::Cos(x), FMath::Tan(x)

// KAIN: asin(x), acos(x), atan(x)
FMath::Asin(x), FMath::Acos(x), FMath::Atan(x)

// KAIN: atan2(y, x)
FMath::Atan2(y, x)

// KAIN: floor(x), ceil(x), round(x)
FMath::Floor(x), FMath::CeilToFloat(x), FMath::RoundToFloat(x)

// KAIN: min(a, b), max(a, b)
FMath::Min(a, b), FMath::Max(a, b)

// KAIN: clamp(x, lo, hi)
FMath::Clamp(x, lo, hi)

// KAIN: lerp(a, b, t) or mix(a, b, t)
FMath::Lerp(a, b, t)

// KAIN: smoothstep(edge0, edge1, x)
FMath::SmoothStep(edge0, edge1, x)

// KAIN: random() or rand()
FMath::FRand()

// KAIN: random_range(min, max)
FMath::FRandRange(min, max)
```

#### Vector Functions

```cpp
// KAIN: vec2(x, y)
FVector2D(x, y)

// KAIN: vec3(x, y, z)
FVector(x, y, z)

// KAIN: vec4(x, y, z, w)
FVector4(x, y, z, w)

// KAIN: dot(a, b)
FVector::DotProduct(a, b)

// KAIN: cross(a, b)
FVector::CrossProduct(a, b)

// KAIN: normalize(v)
v.GetSafeNormal()

// KAIN: length(v)
v.Size()

// KAIN: distance(a, b)
FVector::Dist(a, b)
```

#### Collection Functions

```cpp
// KAIN: len(arr) or len(str)
arr.Num()  // for TArray
str.Len()  // for FString

// KAIN: push(arr, value)
arr.Add(value)

// KAIN: pop(arr)
arr.Pop()

// KAIN: first(arr)
arr[0]

// KAIN: last(arr)
arr[arr.Num() - 1]

// KAIN: reverse(arr)
Algo::Reverse(arr)

// KAIN: range(start, end)
// Generate inline loop or use TArray with Reserve + loop
```

#### String Functions

```cpp
// KAIN: split(str, delimiter)
TArray<FString> parts;
str.ParseIntoArray(parts, *delimiter);

// KAIN: join(arr, delimiter)
FString::Join(arr, *delimiter)

// KAIN: trim(str)
str.TrimStartAndEnd()

// KAIN: to_upper(str) or upper(str)
str.ToUpper()

// KAIN: to_lower(str) or lower(str)
str.ToLower()

// KAIN: contains(str, sub)
str.Contains(sub)

// KAIN: starts_with(str, prefix)
str.StartsWith(prefix)

// KAIN: ends_with(str, suffix)
str.EndsWith(suffix)

// KAIN: replace(str, from, to)
str.Replace(*from, *to)

// KAIN: char_at(str, index)
FString(1, &str[index])

// KAIN: substring(str, start, end)
str.Mid(start, end - start)

// KAIN: ord(str)
(int32)str[0]

// KAIN: chr(code)
FString::Chr(code)
```

#### I/O Functions

```cpp
// KAIN: print(value) or println(value)
UE_LOG(LogTemp, Warning, TEXT("%s"), *value.ToString())

// KAIN: read_file(path)
FString content;
FFileHelper::LoadFileToString(content, *path);

// KAIN: write_file(path, content)
FFileHelper::SaveStringToFile(content, *path);

// KAIN: file_exists(path)
FPaths::FileExists(path)
```

#### HTTP Functions

```cpp
// KAIN: http_get(url)
TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
Request->SetURL(url);
Request->SetVerb("GET");
Request->ProcessRequest();
// Note: Async callback required

// KAIN: http_post_json(url, json)
TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
Request->SetURL(url);
Request->SetVerb("POST");
Request->SetHeader("Content-Type", "application/json");
Request->SetContentAsString(json);
Request->ProcessRequest();
// Note: Async callback required
```

#### JSON Functions

```cpp
// KAIN: json_parse(str)
TSharedPtr<FJsonObject> JsonObject;
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(str);
FJsonSerializer::Deserialize(Reader, JsonObject);

// KAIN: json_string(obj)
FString OutputString;
TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
```

#### HashMap Functions

```cpp
// KAIN: map_new()
TMap<FString, T>()

// KAIN: map_set(map, key, value)
map.Add(key, value)

// KAIN: map_get(map, key)
map.FindRef(key)
```

#### Time Functions

```cpp
// KAIN: now()
FPlatformTime::Seconds()

// KAIN: time()
FDateTime::UtcNow().ToUnixTimestamp()

// KAIN: sleep(seconds)
FPlatformProcess::Sleep(seconds)
```

#### Debug Functions

```cpp
// KAIN: dbg(value)
UE_LOG(LogTemp, Warning, TEXT("[DEBUG] %s"), *value.ToString());
// Return value

// KAIN: assert(condition, message)
check(condition);  // or checkf(condition, TEXT("%s"), *message)

// KAIN: panic(message)
checkf(false, TEXT("%s"), *message);

// KAIN: exit(code)
FGenericPlatformMisc::RequestExit(false);
```

#### Type Conversion

```cpp
// KAIN: int(value)
FCString::Atoi(*value)  // from string
(int32)value            // from float

// KAIN: float(value)
FCString::Atof(*value)  // from string
(float)value            // from int

// KAIN: str(value) or to_string(value)
FString::Printf(TEXT("%d"), value)  // int
FString::Printf(TEXT("%f"), value)  // float
FString::Printf(TEXT("%s"), value ? TEXT("true") : TEXT("false"))  // bool
```

### 2.2 WASM Backend

For WASM, stdlib functions need to be either:
1. **Compiled inline** (math functions via LLVM intrinsics)
2. **Imported from JS** (I/O, HTTP, DOM manipulation)

```javascript
// Import object for WASM
const imports = {
  env: {
    // Math (use Math.* from JS)
    kain_sqrt: (x) => Math.sqrt(x),
    kain_sin: (x) => Math.sin(x),
    kain_cos: (x) => Math.cos(x),
    
    // I/O
    kain_print: (ptr, len) => {
      const str = readString(ptr, len);
      console.log(str);
    },
    
    // HTTP
    kain_http_get: async (url_ptr, url_len) => {
      const url = readString(url_ptr, url_len);
      const response = await fetch(url);
      return await response.text();
    },
    
    // Collections (allocate in WASM memory)
    kain_array_new: () => allocateArray(),
    kain_array_push: (arr_ptr, value) => { /* ... */ },
  }
};
```

### 2.3 LLVM Backend

For LLVM, stdlib functions map to:
1. **LLVM intrinsics** (math functions)
2. **libc calls** (I/O, strings)
3. **Custom runtime** (collections, async)

```llvm
; Math functions via LLVM intrinsics
declare double @llvm.sqrt.f64(double)
declare double @llvm.sin.f64(double)
declare double @llvm.cos.f64(double)
declare double @llvm.pow.f64(double, double)
declare double @llvm.floor.f64(double)
declare double @llvm.ceil.f64(double)

; I/O via libc
declare i32 @printf(i8*, ...)
declare i8* @fgets(i8*, i32, %struct.FILE*)
declare %struct.FILE* @fopen(i8*, i8*)
declare i32 @fclose(%struct.FILE*)

; String functions via libc
declare i64 @strlen(i8*)
declare i8* @strcpy(i8*, i8*)
declare i8* @strcat(i8*, i8*)
declare i32 @strcmp(i8*, i8*)

; Collections require custom runtime
; Link against kain_runtime.a or kain_runtime.so
declare %Array* @kain_array_new()
declare void @kain_array_push(%Array*, i8*)
declare i8* @kain_array_get(%Array*, i64)
```

### 2.4 Rust Backend

For Rust, stdlib functions map directly to Rust std library:

```rust
// Math
x.abs()
x.sqrt()
x.powf(y)
x.sin(), x.cos(), x.tan()
x.floor(), x.ceil(), x.round()
x.min(y), x.max(y)
x.clamp(lo, hi)

// Collections
vec.len()
vec.push(value)
vec.pop()
vec.iter().map(|x| ...)
vec.iter().filter(|x| ...)
vec.iter().fold(init, |acc, x| ...)

// Strings
s.split(delim).collect()
parts.join(delim)
s.trim()
s.to_uppercase(), s.to_lowercase()
s.contains(sub)
s.starts_with(prefix), s.ends_with(suffix)
s.replace(from, to)

// I/O
println!("{}", value)
std::fs::read_to_string(path)
std::fs::write(path, content)
std::path::Path::new(path).exists()

// HTTP (requires reqwest crate)
reqwest::blocking::get(url)?.text()?
reqwest::blocking::Client::new().post(url).json(&data).send()?

// JSON (requires serde_json crate)
serde_json::from_str(s)?
serde_json::to_string(&value)?

// Time
std::time::SystemTime::now()
std::thread::sleep(Duration::from_secs_f64(seconds))
```

---

## 3. Implementation Priority

Functions ranked by impact and difficulty.

### 3.1 High Priority (Must Have for UE5)

These functions are critical for game development and should be implemented first:

| Priority | Function | Reason | Difficulty |
|----------|----------|--------|------------|
| 🔴 P0 | `print`, `println` | Debugging essential | ✅ Done |
| 🔴 P0 | Math functions | Game logic, physics | Easy |
| 🔴 P0 | Vector functions | 3D math essential | Easy |
| 🔴 P0 | `len`, `push`, `pop` | Array manipulation | Easy |
| 🔴 P0 | String functions | Text processing | Easy |
| 🟡 P1 | `read_file`, `write_file` | Asset loading | Medium |
| 🟡 P1 | `json_parse`, `json_string` | Config files | Medium |
| 🟡 P1 | `spawn`, `send` | Actor system | Hard |

### 3.2 Medium Priority (Nice to Have)

| Priority | Function | Reason | Difficulty |
|----------|----------|--------|------------|
| 🟢 P2 | `http_get`, `http_post_json` | Online features | Hard (async) |
| 🟢 P2 | `map`, `filter`, `reduce` | Functional programming | Medium |
| 🟢 P2 | HashMap functions | Data structures | Medium |
| 🟢 P2 | `now`, `time`, `sleep` | Timing/delays | Easy |

### 3.3 Low Priority (Future)

| Priority | Function | Reason | Difficulty |
|----------|----------|--------|------------|
| ⚪ P3 | Python FFI | Not applicable to UE5 | N/A |
| ⚪ P3 | Async functions | Complex runtime | Very Hard |
| ⚪ P3 | `mount` (UI) | Not applicable to UE5 | N/A |

---

## 4. Code Generation Strategy

### 4.1 Current Architecture

The UE5 backend currently handles stdlib calls in `codegen_ue5.rs`:

```rust
// crates/ue5/src/codegen_ue5.rs:2692
fn gen_call(&self, fn_name: &str, args: &[Expr]) -> String {
    // Map math functions to FMath:: / UE5 equivalents
    let ue5_fn_name = match fn_name {
        "abs" => "FMath::Abs",
        "sqrt" => "FMath::Sqrt",
        "sin" => "FMath::Sin",
        // ... etc
        _ => fn_name,  // Fallback to user function
    };
    
    // Generate call
    format!("{}({})", ue5_fn_name, args_str)
}
```

**Problems with this approach:**
1. ❌ Hardcoded in codegen (not extensible)
2. ❌ No type checking (assumes correct types)
3. ❌ No error messages for unsupported functions
4. ❌ Duplicated logic across backends

### 4.2 Proposed Architecture: StdLibResolver

Create a dedicated `StdLibResolver` that:
1. ✅ Centralizes stdlib mappings
2. ✅ Provides type-aware code generation
3. ✅ Gives clear error messages
4. ✅ Supports multiple backends

```rust
// crates/ue5/src/ue5/stdlib_resolver.rs (NEW FILE)

pub struct StdLibResolver {
    mappings: HashMap<String, StdLibMapping>,
}

pub struct StdLibMapping {
    pub kain_name: String,
    pub ue5_template: String,
    pub param_types: Vec<Type>,
    pub return_type: Type,
    pub requires_include: Option<String>,
}

impl StdLibResolver {
    pub fn new() -> Self {
        let mut resolver = Self { mappings: HashMap::new() };
        
        // Math functions
        resolver.add("abs", StdLibMapping {
            kain_name: "abs".into(),
            ue5_template: "FMath::Abs($0)".into(),
            param_types: vec![Type::Float],
            return_type: Type::Float,
            requires_include: None,
        });
        
        // ... register all 80+ functions
        
        resolver
    }
    
    pub fn resolve(&self, fn_name: &str, args: &[String]) -> Result<String, String> {
        let mapping = self.mappings.get(fn_name)
            .ok_or_else(|| format!("Unknown stdlib function: {}", fn_name))?;
        
        // Substitute $0, $1, ... with actual args
        let mut result = mapping.ue5_template.clone();
        for (i, arg) in args.iter().enumerate() {
            result = result.replace(&format!("${}", i), arg);
        }
        
        Ok(result)
    }
}
```

### 4.3 Integration with Codegen

```rust
// crates/ue5/src/codegen_ue5.rs

impl Ue5Codegen {
    fn gen_call(&self, fn_name: &str, args: &[Expr]) -> String {
        // Generate arg strings
        let arg_strs: Vec<String> = args.iter()
            .map(|arg| self.gen_expr(arg))
            .collect();
        
        // Try stdlib resolver first
        if let Ok(ue5_code) = self.stdlib_resolver.resolve(fn_name, &arg_strs) {
            return ue5_code;
        }
        
        // Fallback to user-defined function
        format!("{}({})", fn_name, arg_strs.join(", "))
    }
}
```

### 4.4 Benefits

1. ✅ **Centralized:** All stdlib mappings in one place
2. ✅ **Type-safe:** Can validate arg types before codegen
3. ✅ **Extensible:** Easy to add new functions
4. ✅ **Multi-backend:** Same resolver pattern for WASM, LLVM, Rust
5. ✅ **Error messages:** Clear feedback when function not supported
6. ✅ **Include tracking:** Automatically add required headers

---

## 5. UE5 Deep Dive

Detailed implementation guide for the top 20 most important stdlib functions in UE5.

### 5.1 Math Functions

#### `abs(x: Float) -> Float`

```cpp
// Generated C++
FMath::Abs(x)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Abs(-5.0f);  // 5.0f
```

#### `sqrt(x: Float) -> Float`

```cpp
// Generated C++
FMath::Sqrt(x)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Sqrt(16.0f);  // 4.0f
```

#### `min(a: Float, b: Float) -> Float`

```cpp
// Generated C++
FMath::Min(a, b)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Min(3.0f, 7.0f);  // 3.0f
```

#### `max(a: Float, b: Float) -> Float`

```cpp
// Generated C++
FMath::Max(a, b)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Max(3.0f, 7.0f);  // 7.0f
```

#### `clamp(x: Float, lo: Float, hi: Float) -> Float`

```cpp
// Generated C++
FMath::Clamp(x, lo, hi)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Clamp(15.0f, 0.0f, 10.0f);  // 10.0f
```

#### `lerp(a: Float, b: Float, t: Float) -> Float`

```cpp
// Generated C++
FMath::Lerp(a, b, t)

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::Lerp(0.0f, 100.0f, 0.5f);  // 50.0f
```

#### `random() -> Float`

```cpp
// Generated C++
FMath::FRand()

// Include required
#include "Math/UnrealMathUtility.h"

// Example
float result = FMath::FRand();  // Random float [0, 1)
```

### 5.2 Vector Functions

#### `vec3(x: Float, y: Float, z: Float) -> Vec3`

```cpp
// Generated C++
FVector(x, y, z)

// Include required
#include "Math/Vector.h"

// Example
FVector position = FVector(10.0f, 20.0f, 30.0f);
```

#### `dot(a: Vec3, b: Vec3) -> Float`

```cpp
// Generated C++
FVector::DotProduct(a, b)

// Include required
#include "Math/Vector.h"

// Example
FVector a = FVector(1, 0, 0);
FVector b = FVector(0, 1, 0);
float result = FVector::DotProduct(a, b);  // 0.0f
```

#### `cross(a: Vec3, b: Vec3) -> Vec3`

```cpp
// Generated C++
FVector::CrossProduct(a, b)

// Include required
#include "Math/Vector.h"

// Example
FVector a = FVector(1, 0, 0);
FVector b = FVector(0, 1, 0);
FVector result = FVector::CrossProduct(a, b);  // (0, 0, 1)
```

#### `normalize(v: Vec3) -> Vec3`

```cpp
// Generated C++
v.GetSafeNormal()

// Include required
#include "Math/Vector.h"

// Example
FVector v = FVector(3, 4, 0);
FVector result = v.GetSafeNormal();  // (0.6, 0.8, 0)
```

#### `length(v: Vec3) -> Float`

```cpp
// Generated C++
v.Size()

// Include required
#include "Math/Vector.h"

// Example
FVector v = FVector(3, 4, 0);
float result = v.Size();  // 5.0f
```

#### `distance(a: Vec3, b: Vec3) -> Float`

```cpp
// Generated C++
FVector::Dist(a, b)

// Include required
#include "Math/Vector.h"

// Example
FVector a = FVector(0, 0, 0);
FVector b = FVector(3, 4, 0);
float result = FVector::Dist(a, b);  // 5.0f
```

### 5.3 Collection Functions

#### `len(arr: Array<T>) -> Int`

```cpp
// Generated C++
arr.Num()

// Include required
#include "Containers/Array.h"

// Example
TArray<int32> arr = {1, 2, 3};
int32 result = arr.Num();  // 3
```

#### `push(arr: Array<T>, value: T) -> Unit`

```cpp
// Generated C++
arr.Add(value)

// Include required
#include "Containers/Array.h"

// Example
TArray<int32> arr = {1, 2, 3};
arr.Add(4);  // arr is now {1, 2, 3, 4}
```

#### `pop(arr: Array<T>) -> T`

```cpp
// Generated C++
arr.Pop()

// Include required
#include "Containers/Array.h"

// Example
TArray<int32> arr = {1, 2, 3};
int32 value = arr.Pop();  // value = 3, arr is now {1, 2}
```

### 5.4 String Functions

#### `split(s: String, delimiter: String) -> Array<String>`

```cpp
// Generated C++
TArray<FString> parts;
s.ParseIntoArray(parts, *delimiter);
// Return parts

// Include required
#include "Containers/Array.h"

// Example
FString s = TEXT("hello,world,test");
TArray<FString> parts;
s.ParseIntoArray(parts, TEXT(","));
// parts = ["hello", "world", "test"]
```

#### `join(arr: Array<String>, delimiter: String) -> String`

```cpp
// Generated C++
FString::Join(arr, *delimiter)

// Include required
#include "Containers/Array.h"

// Example
TArray<FString> arr = {TEXT("hello"), TEXT("world")};
FString result = FString::Join(arr, TEXT(" "));
// result = "hello world"
```

#### `trim(s: String) -> String`

```cpp
// Generated C++
s.TrimStartAndEnd()

// Include required
None (FString built-in)

// Example
FString s = TEXT("  hello  ");
FString result = s.TrimStartAndEnd();
// result = "hello"
```

#### `to_upper(s: String) -> String`

```cpp
// Generated C++
s.ToUpper()

// Include required
None (FString built-in)

// Example
FString s = TEXT("hello");
FString result = s.ToUpper();
// result = "HELLO"
```

#### `contains(s: String, sub: String) -> Bool`

```cpp
// Generated C++
s.Contains(sub)

// Include required
None (FString built-in)

// Example
FString s = TEXT("hello world");
bool result = s.Contains(TEXT("world"));
// result = true
```

### 5.5 I/O Functions

#### `print(value: Any) -> Unit`

```cpp
// Generated C++
UE_LOG(LogTemp, Warning, TEXT("%s"), *value.ToString())

// Include required
#include "Logging/LogMacros.h"

// Example
UE_LOG(LogTemp, Warning, TEXT("%s"), TEXT("Hello World"));
```

#### `read_file(path: String) -> String`

```cpp
// Generated C++
FString content;
if (FFileHelper::LoadFileToString(content, *path)) {
    // Success - content contains file data
} else {
    // Error - file not found or read error
}

// Include required
#include "Misc/FileHelper.h"

// Example
FString content;
FFileHelper::LoadFileToString(content, TEXT("C:/data.txt"));
```

#### `write_file(path: String, content: String) -> Unit`

```cpp
// Generated C++
FFileHelper::SaveStringToFile(content, *path)

// Include required
#include "Misc/FileHelper.h"

// Example
FString content = TEXT("Hello World");
FFileHelper::SaveStringToFile(content, TEXT("C:/output.txt"));
```

#### `file_exists(path: String) -> Bool`

```cpp
// Generated C++
FPaths::FileExists(path)

// Include required
#include "Misc/Paths.h"

// Example
bool exists = FPaths::FileExists(TEXT("C:/data.txt"));
```

### 5.6 JSON Functions

#### `json_parse(s: String) -> JsonObject`

```cpp
// Generated C++
TSharedPtr<FJsonObject> JsonObject;
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(s);
if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
    // Success - JsonObject contains parsed data
} else {
    // Error - invalid JSON
}

// Include required
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"

// Example
FString json = TEXT("{\"name\":\"Player\",\"score\":100}");
TSharedPtr<FJsonObject> obj;
TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
FJsonSerializer::Deserialize(reader, obj);
FString name = obj->GetStringField(TEXT("name"));  // "Player"
int32 score = obj->GetIntegerField(TEXT("score"));  // 100
```

#### `json_string(obj: JsonObject) -> String`

```cpp
// Generated C++
FString OutputString;
TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
// OutputString contains JSON

// Include required
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Example
TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject);
obj->SetStringField(TEXT("name"), TEXT("Player"));
obj->SetNumberField(TEXT("score"), 100);
FString output;
TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&output);
FJsonSerializer::Serialize(obj.ToSharedRef(), writer);
// output = "{\"name\":\"Player\",\"score\":100}"
```

### 5.7 HTTP Functions

#### `http_get(url: String) -> String`

```cpp
// Generated C++ (Async pattern required)
TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
Request->SetURL(url);
Request->SetVerb(TEXT("GET"));
Request->OnProcessRequestComplete().BindLambda([](
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful
) {
    if (bWasSuccessful && Response.IsValid()) {
        FString content = Response->GetContentAsString();
        // Handle response
    }
});
Request->ProcessRequest();

// Include required
#include "Http.h"
#include "HttpModule.h"

// Note: HTTP in UE5 is inherently async
// KAIN needs to decide: block until complete, or return Future?
```

#### `http_post_json(url: String, json: String) -> String`

```cpp
// Generated C++ (Async pattern required)
TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
Request->SetURL(url);
Request->SetVerb(TEXT("POST"));
Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
Request->SetContentAsString(json);
Request->OnProcessRequestComplete().BindLambda([](
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful
) {
    if (bWasSuccessful && Response.IsValid()) {
        FString content = Response->GetContentAsString();
        // Handle response
    }
});
Request->ProcessRequest();

// Include required
#include "Http.h"
#include "HttpModule.h"
```

---

## 6. Runtime Linking Strategy

### 6.1 Inline vs. Runtime Library

**Inline Generation (Current):**
- ✅ No runtime dependencies
- ✅ Optimized by compiler
- ❌ Code bloat for complex functions
- ❌ Harder to update stdlib

**Runtime Library (Proposed):**
- ✅ Smaller generated code
- ✅ Easy to update stdlib
- ✅ Shared across all KAIN plugins
- ❌ Requires linking
- ❌ Slightly slower (function call overhead)

### 6.2 Hybrid Approach (Recommended)

**Inline:** Simple functions (math, type conversion)
**Runtime:** Complex functions (HTTP, JSON, collections)

```cpp
// Inline (generated directly)
float result = FMath::Sqrt(x);

// Runtime library (link against KainRuntime.lib)
FString result = KainRuntime::HttpGet(url);
```

### 6.3 KainRuntime Module Structure

```
KainRuntime/
├── Source/
│   ├── KainRuntime/
│   │   ├── Public/
│   │   │   ├── KainRuntime.h          # Main header
│   │   │   ├── KainCollections.h      # Array/Map helpers
│   │   │   ├── KainHttp.h             # HTTP wrappers
│   │   │   ├── KainJson.h             # JSON helpers
│   │   │   └── KainAsync.h            # Async utilities
│   │   └── Private/
│   │       ├── KainRuntime.cpp
│   │       ├── KainCollections.cpp
│   │       ├── KainHttp.cpp
│   │       ├── KainJson.cpp
│   │       └── KainAsync.cpp
│   └── KainRuntime.Build.cs
└── KainRuntime.uplugin
```

### 6.4 Example Runtime Functions

```cpp
// KainRuntime/Public/KainHttp.h
namespace KainRuntime {
    // Synchronous HTTP GET (blocks until complete)
    FString HttpGetSync(const FString& Url);
    
    // Async HTTP GET (returns immediately, callback on complete)
    void HttpGetAsync(const FString& Url, TFunction<void(FString)> Callback);
}

// KainRuntime/Public/KainJson.h
namespace KainRuntime {
    // Parse JSON string to TSharedPtr<FJsonObject>
    TSharedPtr<FJsonObject> JsonParse(const FString& JsonString);
    
    // Serialize TSharedPtr<FJsonObject> to string
    FString JsonStringify(TSharedPtr<FJsonObject> JsonObject);
}

// KainRuntime/Public/KainCollections.h
namespace KainRuntime {
    // Map function over TArray
    template<typename T, typename U>
    TArray<U> ArrayMap(const TArray<T>& Input, TFunction<U(T)> Func);
    
    // Filter function over TArray
    template<typename T>
    TArray<T> ArrayFilter(const TArray<T>& Input, TFunction<bool(T)> Predicate);
    
    // Reduce function over TArray
    template<typename T, typename U>
    U ArrayReduce(const TArray<T>& Input, U Initial, TFunction<U(U, T)> Func);
}
```

---

## 7. Testing Strategy

### 7.1 Test Categories

1. **Unit Tests:** Test each stdlib function in isolation
2. **Integration Tests:** Test stdlib functions in real KAIN programs
3. **Backend Tests:** Verify correct code generation for each backend
4. **Performance Tests:** Benchmark stdlib vs. hand-written code

### 7.2 Test File Structure

```
testing/stdlib/
├── math_test.kn              # Test all math functions
├── vector_test.kn            # Test all vector functions
├── collections_test.kn       # Test all collection functions
├── strings_test.kn           # Test all string functions
├── io_test.kn                # Test all I/O functions
├── http_test.kn              # Test HTTP functions
├── json_test.kn              # Test JSON functions
└── integration_test.kn       # Test multiple stdlib functions together
```

### 7.3 Example Test: Math Functions

```kain
// testing/stdlib/math_test.kn

fn test_abs():
    assert(abs(-5) == 5, "abs(-5) should be 5")
    assert(abs(5) == 5, "abs(5) should be 5")
    assert(abs(-3.14) == 3.14, "abs(-3.14) should be 3.14")
    println("✓ abs tests passed")

fn test_sqrt():
    assert(sqrt(4.0) == 2.0, "sqrt(4) should be 2")
    assert(sqrt(16.0) == 4.0, "sqrt(16) should be 4")
    assert(sqrt(0.0) == 0.0, "sqrt(0) should be 0")
    println("✓ sqrt tests passed")

fn test_min_max():
    assert(min(3, 7) == 3, "min(3, 7) should be 3")
    assert(max(3, 7) == 7, "max(3, 7) should be 7")
    assert(min(-5, -2) == -5, "min(-5, -2) should be -5")
    println("✓ min/max tests passed")

fn test_clamp():
    assert(clamp(5, 0, 10) == 5, "clamp(5, 0, 10) should be 5")
    assert(clamp(-5, 0, 10) == 0, "clamp(-5, 0, 10) should be 0")
    assert(clamp(15, 0, 10) == 10, "clamp(15, 0, 10) should be 10")
    println("✓ clamp tests passed")

fn main():
    println("Running math stdlib tests...")
    test_abs()
    test_sqrt()
    test_min_max()
    test_clamp()
    println("All math tests passed! ✓")
```

### 7.4 Backend-Specific Tests

```bash
# Test UE5 backend
cd testing/stdlib
kain build --ue5 math_test.kn
# Verify generated C++ uses FMath::

# Test WASM backend
kain build --wasm math_test.kn
# Verify WASM imports correct functions

# Test LLVM backend
kain build --llvm math_test.kn
# Verify LLVM IR uses correct intrinsics

# Test Rust backend
kain build --rust math_test.kn
# Verify Rust code uses std:: functions
```

### 7.5 Performance Benchmarks

```kain
// testing/stdlib/bench_math.kn

fn bench_sqrt():
    let start = now()
    var sum = 0.0
    for i in range(0, 1000000):
        sum = sum + sqrt(i as Float)
    let elapsed = now() - start
    println("sqrt benchmark: {elapsed}s")

fn bench_vector_ops():
    let start = now()
    var v = vec3(1.0, 2.0, 3.0)
    for i in range(0, 1000000):
        v = normalize(v)
    let elapsed = now() - start
    println("normalize benchmark: {elapsed}s")

fn main():
    println("Running stdlib benchmarks...")
    bench_sqrt()
    bench_vector_ops()
```

---

## 8. Implementation Roadmap

### Phase 1: Core Math & Vectors (Week 1)
- ✅ Implement `StdLibResolver` architecture
- ✅ Add all math functions (abs, sqrt, sin, cos, min, max, clamp, lerp)
- ✅ Add all vector functions (vec2, vec3, vec4, dot, cross, normalize, length, distance)
- ✅ Write unit tests
- ✅ Verify UE5 codegen

### Phase 2: Collections & Strings (Week 2)
- ✅ Add collection functions (len, push, pop, first, last, reverse)
- ✅ Add string functions (split, join, trim, upper, lower, contains, replace)
- ✅ Write unit tests
- ✅ Verify UE5 codegen

### Phase 3: I/O & JSON (Week 3)
- ✅ Add I/O functions (read_file, write_file, file_exists)
- ✅ Add JSON functions (json_parse, json_string)
- ✅ Write unit tests
- ✅ Verify UE5 codegen

### Phase 4: HTTP & Async (Week 4)
- ✅ Design async/await strategy for UE5
- ✅ Add HTTP functions (http_get, http_post_json)
- ✅ Create KainRuntime module
- ✅ Write integration tests

### Phase 5: Other Backends (Week 5+)
- ✅ Implement WASM stdlib resolver
- ✅ Implement LLVM stdlib resolver
- ✅ Implement Rust stdlib resolver
- ✅ Cross-backend testing

---

## 9. Open Questions

### 9.1 Async/Await in UE5

**Problem:** UE5 HTTP is inherently async, but KAIN stdlib expects synchronous return.

**Options:**
1. **Block until complete** (simple, but blocks game thread)
2. **Return Future** (complex, requires async/await in KAIN)
3. **Callback-based** (awkward in KAIN syntax)

**Recommendation:** Start with blocking for simplicity, add async/await later.

### 9.2 Error Handling

**Problem:** Many stdlib functions can fail (file not found, HTTP error, JSON parse error).

**Options:**
1. **Return Result<T, E>** (Rust-style, requires Result type in KAIN)
2. **Throw exceptions** (C++-style, requires exception handling in KAIN)
3. **Return None on error** (simple, but loses error info)

**Recommendation:** Use Result<T, E> pattern for consistency with Rust.

### 9.3 Type Overloading

**Problem:** Some functions work on multiple types (abs for Int and Float, len for Array and String).

**Options:**
1. **Separate functions** (abs_int, abs_float - verbose)
2. **Type-based dispatch** (codegen checks type, generates correct code)
3. **Generic functions** (requires generics in KAIN)

**Recommendation:** Type-based dispatch in codegen (already partially implemented).

---

## 10. Summary

### Current State
- ✅ 80+ stdlib functions implemented in interpreter
- ⚠️ Only 2 functions (print/println) mapped to UE5
- ❌ No stdlib support in other backends

### Proposed Solution
- ✅ Create `StdLibResolver` architecture
- ✅ Centralize all stdlib mappings
- ✅ Support multiple backends
- ✅ Provide clear error messages

### Next Steps
1. Implement `StdLibResolver` in `crates/ue5/src/ue5/stdlib_resolver.rs`
2. Add all P0 functions (math, vectors, collections, strings)
3. Write comprehensive tests
4. Extend to other backends (WASM, LLVM, Rust)

### Success Metrics
- ✅ All 80+ stdlib functions work in UE5
- ✅ Generated C++ is idiomatic and efficient
- ✅ Clear error messages for unsupported functions
- ✅ Comprehensive test coverage (>90%)
- ✅ Performance matches hand-written C++

---

**End of Document**
