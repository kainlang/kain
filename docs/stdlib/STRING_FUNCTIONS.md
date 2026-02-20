# KAIN String Functions

## Overview

KAIN provides 15 essential string manipulation functions that map directly to UE5's `FString` operations. All functions are available through the `StdLibResolver` and generate efficient C++ code.

**Total Functions:** 15  
**UE5 Type:** `FString`  
**Include Required:** None (FString is always available)

---

## Function Reference

### 1. `trim(str: String) -> String`

Remove whitespace from the start and end of a string.

**KAIN:**
```kain
let cleaned = trim("  hello  ")
```

**UE5 C++:**
```cpp
FString cleaned = "  hello  ".TrimStartAndEnd();
```

**Use Cases:**
- User input sanitization
- Configuration file parsing
- Whitespace cleanup

---

### 2. `upper(str: String) -> String`

Convert string to uppercase.

**KAIN:**
```kain
let loud = upper("hello")
```

**UE5 C++:**
```cpp
FString loud = "hello".ToUpper();
```

**Use Cases:**
- Case-insensitive comparison
- Display formatting
- Key normalization

---

### 3. `lower(str: String) -> String`

Convert string to lowercase.

**KAIN:**
```kain
let quiet = lower("HELLO")
```

**UE5 C++:**
```cpp
FString quiet = "HELLO".ToLower();
```

**Use Cases:**
- Case-insensitive comparison
- Filename normalization
- Search operations

---

### 4. `str_contains(str: String, substring: String) -> Bool`

Check if string contains a substring.

**KAIN:**
```kain
let has_world = str_contains("Hello World", "World")
```

**UE5 C++:**
```cpp
bool has_world = "Hello World".Contains("World");
```

**Use Cases:**
- Search functionality
- Validation
- Filtering

**Note:** Named `str_contains` to avoid conflict with array `contains()`.

---

### 5. `starts_with(str: String, prefix: String) -> Bool`

Check if string starts with a prefix.

**KAIN:**
```kain
let is_greeting = starts_with("Hello World", "Hello")
```

**UE5 C++:**
```cpp
bool is_greeting = "Hello World".StartsWith("Hello");
```

**Use Cases:**
- Protocol detection
- Path validation
- Command parsing

---

### 6. `ends_with(str: String, suffix: String) -> Bool`

Check if string ends with a suffix.

**KAIN:**
```kain
let is_cpp = ends_with("main.cpp", ".cpp")
```

**UE5 C++:**
```cpp
bool is_cpp = "main.cpp".EndsWith(".cpp");
```

**Use Cases:**
- File extension checking
- URL validation
- Pattern matching

---

### 7. `replace(str: String, from: String, to: String) -> String`

Replace all occurrences of a substring.

**KAIN:**
```kain
let fixed = replace("Hello World", "World", "KAIN")
```

**UE5 C++:**
```cpp
FString fixed = "Hello World".Replace(*"World", *"KAIN");
```

**Use Cases:**
- Text substitution
- Template processing
- Data sanitization

**Note:** Uses `*` dereference for FString parameters.

---

### 8. `substring(str: String, start: Int, length: Int) -> String`

Extract a substring starting at index with specified length.

**KAIN:**
```kain
let sub = substring("Hello World", 0, 5)  // "Hello"
```

**UE5 C++:**
```cpp
FString sub = "Hello World".Mid(0, 5);
```

**Use Cases:**
- Text extraction
- Parsing
- Truncation

**Note:** Uses UE5's `Mid()` function (start, length).

---

### 9. `char_at(str: String, index: Int) -> String`

Get character at index as a string.

**KAIN:**
```kain
let first = char_at("Hello", 0)  // "H"
```

**UE5 C++:**
```cpp
FString first = FString(1, &"Hello"[0]);
```

**Use Cases:**
- Character inspection
- Parsing
- Validation

**Note:** Returns a single-character FString, not a char.

---

### 10. `to_int(str: String) -> Int`

Convert string to integer.

**KAIN:**
```kain
let num = to_int("42")  // 42
```

**UE5 C++:**
```cpp
int32 num = FCString::Atoi(*"42");
```

**Use Cases:**
- Configuration parsing
- User input conversion
- Data deserialization

**Note:** Returns 0 if conversion fails.

---

### 11. `to_float(str: String) -> Float`

Convert string to float.

**KAIN:**
```kain
let pi = to_float("3.14159")  // 3.14159
```

**UE5 C++:**
```cpp
float pi = FCString::Atof(*"3.14159");
```

**Use Cases:**
- Configuration parsing
- Numeric input
- Data conversion

**Note:** Returns 0.0 if conversion fails.

---

### 12. `str_is_empty(str: String) -> Bool`

Check if string is empty.

**KAIN:**
```kain
let empty = str_is_empty("")  // true
```

**UE5 C++:**
```cpp
bool empty = "".IsEmpty();
```

**Use Cases:**
- Validation
- Conditional logic
- Error checking

**Note:** Named `str_is_empty` to avoid conflict with array `is_empty()`.

---

### 13. `join(arr: Array<String>, delimiter: String) -> String`

Join array of strings with delimiter.

**KAIN:**
```kain
let csv = join(["apple", "banana", "cherry"], ",")
// "apple,banana,cherry"
```

**UE5 C++:**
```cpp
FString csv = FString::Join(TArray<FString>{"apple", "banana", "cherry"}, *",");
```

**Use Cases:**
- CSV generation
- Path construction
- List formatting

---

### 14. `str_len(str: String) -> Int`

Get string length.

**KAIN:**
```kain
let length = str_len("Hello")  // 5
```

**UE5 C++:**
```cpp
int32 length = "Hello".Len();
```

**Use Cases:**
- Validation
- Buffer sizing
- Truncation logic

**Note:** Named `str_len` to avoid conflict with array `len()`.

---

### 15. `split(str: String, delimiter: String) -> Array<String>`

Split string by delimiter into array.

**KAIN:**
```kain
let parts = split("apple,banana,cherry", ",")
// ["apple", "banana", "cherry"]
```

**UE5 C++:**
```cpp
TArray<FString> parts = [&](){ 
    TArray<FString> Parts; 
    "apple,banana,cherry".ParseIntoArray(Parts, *","); 
    return Parts; 
}();
```

**Use Cases:**
- CSV parsing
- Path splitting
- Tokenization

**Note:** Uses lambda to return array from `ParseIntoArray()`.

---

## Naming Conventions

Some functions have prefixes to avoid conflicts with array functions:

| String Function | Array Function | Reason |
|----------------|----------------|--------|
| `str_contains()` | `contains()` | Both check membership |
| `str_is_empty()` | `is_empty()` | Both check emptiness |
| `str_len()` | `len()` | Both get length |

**Design Decision:** Explicit prefixes make code clearer and avoid ambiguity.

---

## Usage Examples

### Example 1: Input Validation

```kain
@blueprint
fn validate_username(username: String) -> Bool:
    // Check if empty
    if str_is_empty(username):
        return false
    
    // Trim whitespace
    let cleaned = trim(username)
    
    // Check length
    if str_len(cleaned) < 3:
        return false
    
    // Check for invalid characters
    if str_contains(cleaned, " "):
        return false
    
    return true
```

### Example 2: CSV Parsing

```kain
@blueprint
fn parse_csv_row(row: String) -> Array<String>:
    // Split by comma
    let fields = split(row, ",")
    
    // Trim each field
    var cleaned: Array<String> = []
    for field in fields:
        push(cleaned, trim(field))
    
    return cleaned
```

### Example 3: File Extension Check

```kain
@blueprint
fn is_image_file(filename: String) -> Bool:
    let lower_name = lower(filename)
    
    if ends_with(lower_name, ".png"):
        return true
    if ends_with(lower_name, ".jpg"):
        return true
    if ends_with(lower_name, ".jpeg"):
        return true
    
    return false
```

### Example 4: String Formatting

```kain
@blueprint
fn format_player_name(first: String, last: String) -> String:
    // Trim and lowercase
    let first_clean = lower(trim(first))
    let last_clean = lower(trim(last))
    
    // Uppercase first letter (simplified)
    let first_cap = upper(first_clean)
    let last_cap = upper(last_clean)
    
    // Join with space
    return join([first_cap, last_cap], " ")
```

### Example 5: Configuration Parsing

```kain
@blueprint
fn parse_config_line(line: String) -> Bool:
    // Trim whitespace
    let cleaned = trim(line)
    
    // Skip empty lines
    if str_is_empty(cleaned):
        return false
    
    // Skip comments
    if starts_with(cleaned, "#"):
        return false
    
    // Parse key=value
    if str_contains(cleaned, "="):
        let parts = split(cleaned, "=")
        let key = trim(first(parts))
        let value = trim(last(parts))
        
        // Process key-value pair
        println(f"Config: {key} = {value}")
        return true
    
    return false
```

---

## Special Cases

### `split()` Implementation

The `split()` function uses a lambda to work around UE5's `ParseIntoArray()` API:

```cpp
// UE5's ParseIntoArray modifies array in-place
TArray<FString> Parts;
str.ParseIntoArray(Parts, *delimiter);

// KAIN wraps in lambda to return array
[&](){ 
    TArray<FString> Parts; 
    str.ParseIntoArray(Parts, *delimiter); 
    return Parts; 
}()
```

This allows `split()` to be used as an expression:
```kain
let parts = split(csv, ",")  // Works!
```

### `replace()` Pointer Dereference

The `replace()` function uses `*` to dereference FString parameters:

```cpp
str.Replace(*from, *to)
```

This is required by UE5's `FString::Replace()` API which expects `const TCHAR*`.

---

## Performance Notes

1. **String Immutability:** Most operations return new strings (UE5 FString is copy-on-write)
2. **Allocation:** `split()` allocates a new array
3. **In-Place:** Some operations like `trim()` may modify in-place if possible
4. **Caching:** Store results of expensive operations (e.g., `upper()`, `lower()`)

---

## Testing

See `testing/stdlib/StringTest.kn` for comprehensive test coverage of all 15 functions.

**Test Coverage:**
- ✅ All 15 functions tested
- ✅ Edge cases (empty strings, special characters)
- ✅ Chaining operations
- ✅ Integration with arrays
- ✅ Blueprint-callable wrappers

---

## Future Enhancements

Potential additions for future versions:

1. **`format(fmt, ...)` - Variadic string formatting**
   - Challenge: Variadic args in KAIN
   - Workaround: Use f-strings for now

2. **`pad_left(str, width, char)` - Left padding**
   - UE5: `str.LeftPad(width)`

3. **`pad_right(str, width, char)` - Right padding**
   - UE5: `str.RightPad(width)`

4. **`reverse(str)` - Reverse string**
   - UE5: `Algo::Reverse(str)`

5. **`repeat(str, count)` - Repeat string**
   - Custom implementation needed

6. **`index_of(str, sub)` - Find substring index**
   - UE5: `str.Find(sub)`

7. **`last_index_of(str, sub)` - Find last occurrence**
   - Custom implementation needed

8. **`trim_start(str)` - Trim left only**
   - UE5: `str.TrimStart()`

9. **`trim_end(str)` - Trim right only**
   - UE5: `str.TrimEnd()`

10. **`is_numeric(str)` - Check if string is numeric**
    - UE5: `str.IsNumeric()`

---

## Summary

The KAIN string stdlib provides 15 essential functions for text processing:

| Category | Functions | Count |
|----------|-----------|-------|
| **Case Conversion** | `upper`, `lower` | 2 |
| **Whitespace** | `trim` | 1 |
| **Search** | `str_contains`, `starts_with`, `ends_with` | 3 |
| **Manipulation** | `replace`, `substring`, `char_at` | 3 |
| **Conversion** | `to_int`, `to_float` | 2 |
| **Validation** | `str_is_empty`, `str_len` | 2 |
| **Array Operations** | `split`, `join` | 2 |

**Total:** 15 functions

All functions map directly to UE5 FString operations with zero overhead.
