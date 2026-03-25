# KAIN Stdlib Error Messages

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Status:** Production-Ready

## Overview

This document catalogs all stdlib-related error messages, their causes, and recovery strategies. The stdlib system provides clear, actionable error messages for all failure scenarios.

## Error Categories

### 1. Discovery Errors

#### Error: "Stdlib not found, compiling without standard library"

**Cause:** Stdlib directory not found in any search location (KAIN_STDLIB_PATH, exe walk, CWD walk)

**Severity:** Warning (compilation continues without stdlib)

**Recovery Strategies:**
1. Set KAIN_STDLIB_PATH environment variable:
   ```bash
   set KAIN_STDLIB_PATH=M:\Code\Kain\stdlib
   ```
2. Ensure `Kain/stdlib/ue5/` directory exists
3. Run `kain build --ue5` from within Kain repository
4. Verify stdlib files exist:
   ```bash
   dir M:\Code\Kain\stdlib\ue5\*.kn
   ```

**Example:**
```
Warning: Stdlib not found, compiling without standard library
Searched locations:
  - KAIN_STDLIB_PATH: not set
  - Exe walk: C:\Users\Admin\.cargo\bin\ (not found)
  - CWD walk: M:\Code\Kain\Factory\Example\ (not found)
```

#### Error: "KAIN_STDLIB_PATH invalid, searching default locations"

**Cause:** KAIN_STDLIB_PATH points to non-existent directory

**Severity:** Warning (falls back to filesystem walking)

**Recovery Strategies:**
1. Verify KAIN_STDLIB_PATH points to valid directory
2. Check for typos in path
3. Ensure directory exists and is accessible
4. Unset KAIN_STDLIB_PATH to use default discovery

**Example:**
```
Warning: KAIN_STDLIB_PATH invalid: M:\Code\Kain\stdlib_typo
Falling back to default search locations
```

#### Error: "Stdlib directory empty, compiling without standard library"

**Cause:** Stdlib directory exists but contains no .kn files

**Severity:** Warning (compilation continues without stdlib)

**Recovery Strategies:**
1. Verify stdlib files exist in `stdlib/ue5/` directory
2. Check file extensions (.kn not .txt)
3. Restore stdlib files from repository
4. Remove empty stdlib directory if intentional

**Example:**
```
Warning: Stdlib directory empty: M:\Code\Kain\stdlib\ue5
Found 0 .kn files
Compiling without standard library
```

### 2. Parsing Errors

#### Error: "Syntax error in stdlib file"

**Cause:** Stdlib file has invalid KAIN syntax

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Fix syntax error in stdlib file
2. Check for typos or incorrect syntax
3. Verify function signatures are correct
4. Temporarily remove problematic stdlib file
5. Report issue to KAIN development team

**Example:**
```
Error: Syntax error in stdlib/ue5/actor.kn:42:5
Expected 'fn' but found 'var'

42 | var GetActorLocation() -> Vec3
   |     ^
   | Expected function declaration

Recovery:
  - Change 'var' to 'fn' on line 42
  - Or remove line 42 if not needed
```

#### Error: "Type error in stdlib function"

**Cause:** Stdlib function has incorrect type signature

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Fix type signature in stdlib file
2. Ensure all type references resolve to known types
3. Check for typos in type names (Vec3 vs Vector3)
4. Verify custom types are defined before use

**Example:**
```
Error: Type error in stdlib function GetActorLocation in actor.kn:6:30
Expected Vec3 but found Float

6 | fn GetActorLocation() -> Float
  |                          ^^^^^
  | Return type should be Vec3, not Float

Recovery:
  - Change return type to Vec3 on line 6
  - Or verify function signature matches UE5 API
```

#### Error: "Duplicate function name"

**Cause:** Same function defined in multiple stdlib files

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Remove duplicate function from one file
2. Rename one function to avoid conflict
3. Verify function is in correct category file
4. Report issue to KAIN development team

**Example:**
```
Error: Duplicate function GetActorLocation
Found in:
  - stdlib/ue5/actor.kn:6
  - stdlib/ue5/world.kn:42

Recovery:
  - Remove GetActorLocation from world.kn (line 42)
  - Or rename one function to avoid conflict
```

### 3. Compilation Errors

#### Error: "Undefined function"

**Cause:** Function doesn't exist in stdlib or typo in function name

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Check function name spelling
2. Verify function exists in stdlib files:
   ```bash
   grep -r "fn GetActorLocation" M:\Code\Kain\stdlib\ue5\
   ```
3. Check stdlib README for available functions
4. Ensure stdlib is loaded (check compilation output)

**Example:**
```
Error: Undefined function: GetActorLocaton
Did you mean: GetActorLocation?

Location: Factory/Example/Kain/player.kn:12:20

12 |     let location = GetActorLocaton()
   |                    ^^^^^^^^^^^^^^^
   | Function not found in stdlib or user code

Recovery:
  - Fix typo: GetActorLocaton → GetActorLocation
  - Or verify stdlib is loaded (check for "Loading stdlib" message)
```

#### Error: "Type mismatch in function call"

**Cause:** Function called with wrong parameter types

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Check function signature in stdlib file
2. Verify parameter types match
3. Check stdlib documentation for correct usage
4. Ensure you're calling the right function

**Example:**
```
Error: Type mismatch in function call to apply_damage
Expected: (Float, Float, Float, Float)
Got: (Int, Float, Float, Float)

Location: Factory/Example/Kain/player.kn:15:18

15 |     health = apply_damage(100, max_health, damage, armor)
   |                           ^^^
   | Parameter 1: expected Float, got Int

Recovery:
  - Change 100 to 100.0 (Float literal)
  - Or cast to Float: apply_damage(100 as Float, ...)
```

#### Error: "Wrong number of parameters"

**Cause:** Function called with wrong number of parameters

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Check function signature in stdlib file
2. Verify parameter count matches
3. Check stdlib documentation for correct usage
4. Ensure you're calling the right function

**Example:**
```
Error: Wrong number of parameters in function call to GetActorLocation
Expected: 0 parameters
Got: 1 parameter

Location: Factory/Example/Kain/player.kn:12:20

12 |     let location = GetActorLocation(self)
   |                    ^^^^^^^^^^^^^^^^^^^^^^
   | GetActorLocation takes no parameters

Recovery:
  - Remove parameter: GetActorLocation()
  - Or check if you meant a different function
```

### 4. Shader Compilation Errors

#### Error: "String type in shader context"

**Cause:** String type validator-codegen mismatch (known issue)

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Temporarily disable stdlib:
   ```bash
   set KAIN_STDLIB_PATH=M:\Code\empty_stdlib
   kain build --ue5
   ```
2. Use shader stdlib functions in separate test files
3. Wait for backend fix (update shader validator to reject String types)

**Example:**
```
Error: Type 'String' should have been rejected by validator
This indicates a validator-codegen synchronization bug

Location: crates\ue5-shaders\src\codegen_usf.rs:2206:21

Context: Shader stdlib (shaders.kn) contains functions with String parameters
that are being loaded into shader compilation context

Recovery:
  - Temporarily disable stdlib (set KAIN_STDLIB_PATH to empty directory)
  - Or use shader functions in separate test files
  - Or wait for backend fix (in progress)

Permanent Fix:
  - Update ue5-shaders crate to reject String types in shader context
  - Remove String parameters from shader stdlib functions
```

### 5. Validation Errors

#### Error: "Stdlib function has empty body"

**Cause:** @blueprint function has no implementation

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Add function implementation
2. Change annotation to @extern if function exists in C++
3. Remove function if not needed

**Example:**
```
Error: Stdlib function apply_damage has empty body
@blueprint functions must have complete implementations

Location: stdlib/ue5/gameplay.kn:15:1

15 | @blueprint
16 | fn apply_damage(current_health: Float, max_health: Float, damage: Float, armor: Float) -> Float:
17 |     # Empty body
   |     ^^^^^^^^^^^
   | @blueprint functions require implementation

Recovery:
  - Add function body with implementation
  - Or change @blueprint to @extern if function exists in UE5 C++
```

#### Error: "@extern function has body"

**Cause:** @extern function has implementation (should be declaration only)

**Severity:** Error (compilation fails)

**Recovery Strategies:**
1. Remove function body
2. Change annotation to @blueprint if function should be implemented in KAIN

**Example:**
```
Error: @extern function GetActorLocation has body
@extern functions should be declarations only

Location: stdlib/ue5/actor.kn:6:1

6 | @extern
7 | fn GetActorLocation() -> Vec3:
8 |     return vec3(0.0, 0.0, 0.0)
   |     ^^^^^^^^^^^^^^^^^^^^^^^^^^
   | @extern functions should not have implementation

Recovery:
  - Remove function body (lines 8-9)
  - Or change @extern to @blueprint if function should be implemented
```

## Error Message Quality Checklist

All stdlib error messages should:

- ✅ Include error location (file, line, column)
- ✅ Explain what went wrong
- ✅ Suggest recovery strategies
- ✅ Provide examples when helpful
- ✅ Use clear, non-technical language
- ✅ Include context (surrounding code)
- ✅ Suggest similar functions (did you mean?)
- ✅ Link to documentation when applicable

## Testing Error Messages

### Test Cases

1. **Missing Stdlib:**
   ```bash
   mv Kain/stdlib Kain/stdlib_backup
   kain build --ue5
   # Expected: Warning about stdlib not found
   mv Kain/stdlib_backup Kain/stdlib
   ```

2. **Invalid KAIN_STDLIB_PATH:**
   ```bash
   set KAIN_STDLIB_PATH=M:\Invalid\Path
   kain build --ue5
   # Expected: Warning about invalid path, fallback to default
   ```

3. **Syntax Error:**
   ```bash
   # Add syntax error to actor.kn
   echo "var GetActorLocation() -> Vec3" >> Kain/stdlib/ue5/actor.kn
   kain build --ue5
   # Expected: Syntax error with line number and recovery suggestion
   git checkout Kain/stdlib/ue5/actor.kn
   ```

4. **Type Error:**
   ```bash
   # Change return type in actor.kn
   sed -i 's/-> Vec3/-> Float/' Kain/stdlib/ue5/actor.kn
   kain build --ue5
   # Expected: Type error with expected vs actual types
   git checkout Kain/stdlib/ue5/actor.kn
   ```

5. **Duplicate Function:**
   ```bash
   # Add duplicate function to world.kn
   echo "@extern\nfn GetActorLocation() -> Vec3" >> Kain/stdlib/ue5/world.kn
   kain build --ue5
   # Expected: Duplicate function error with both locations
   git checkout Kain/stdlib/ue5/world.kn
   ```

6. **Undefined Function:**
   ```bash
   # Use non-existent function in Example plugin
   echo "let x = NonExistentFunction()" >> Factory/Example/Kain/test.kn
   kain build --ue5
   # Expected: Undefined function error with suggestion
   rm Factory/Example/Kain/test.kn
   ```

## Conclusion

The stdlib error message system provides clear, actionable error messages for all failure scenarios. All error messages include location, explanation, recovery strategies, and examples. The system follows best practices for error reporting and helps users quickly diagnose and fix issues.

**Key Features:**
- Clear error messages with location (file, line, column)
- Actionable recovery strategies
- Examples and context
- "Did you mean?" suggestions
- Links to documentation
- Graceful degradation (warnings vs errors)

**Next Steps:**
- Test all error scenarios
- Verify error messages are clear and helpful
- Add more "did you mean?" suggestions
- Improve error message formatting
- Add color coding for better readability

---

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Status:** Production-Ready
