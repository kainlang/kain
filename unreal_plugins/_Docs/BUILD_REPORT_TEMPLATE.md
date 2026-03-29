# [Plugin Name] — Build Report

**Date:** [Date]  
**Plugin:** [Plugin Name] ([Description])  
**Source:** [N] KAIN files → [M] generated files  
**Final Status:** [✅ Clean build / ❌ Failed]

---

## Plugin Overview

### File Structure
| KAIN Source | Lines | Role |
|-------------|-------|------|
| [file.kn] | [count] | [description] |

### Generated Output
- **C++ Files:** [count]
- **Header Files:** [count]
- **Shader Files:** [count]
- **Blueprint Files:** [count]
- **Material Files:** [count]
- **Total Generated:** [count] files

---

## Compilation Journey

### Phase 1: Source-Level Fixes

#### Fix Pattern 1: [Pattern Name]
**Problem:** [Description of the issue]  
**Frequency:** [How many times this appeared]  
**Fix Applied:** [Description of the fix]

**Example:**
```kain
# Before (Invalid)
[invalid code]

# After (Valid)
[valid code]
```

#### Fix Pattern 2: [Pattern Name]
[Continue for all patterns...]

---

### Phase 2: Backend Fixes

#### Backend Fix 1: [Fix Name]
**File Modified:** `[path/to/rust/file.rs]`  
**Problem:** [Description of the codegen issue]  
**Solution:** [Description of the backend change]  
**Impact:** [Which plugins benefit from this fix]

**Code Changes:**
```rust
// Before
[old code]

// After
[new code]
```

---

## Error Patterns Encountered

### Parse Errors
| Error Message | Root Cause | Fix Applied | Frequency |
|---------------|------------|-------------|-----------|
| [error] | [cause] | [fix] | [count] |

### Type Errors
| Error Message | Root Cause | Fix Applied | Frequency |
|---------------|------------|-------------|-----------|
| [error] | [cause] | [fix] | [count] |

### Oracle Errors
| Error Message | Root Cause | Fix Applied | Frequency |
|---------------|------------|-------------|-----------|
| [error] | [cause] | [fix] | [count] |

### Codegen Errors
| Error Message | Root Cause | Fix Applied | Frequency |
|---------------|------------|-------------|-----------|
| [error] | [cause] | [fix] | [count] |

### UE5 C++ Compilation Errors
| Error Message | Root Cause | Fix Applied | Frequency |
|---------------|------------|-------------|-----------|
| [error] | [cause] | [fix] | [count] |

---

## Functional Completeness Verification

### Features Implemented
- [x] [Feature 1]
- [x] [Feature 2]
- [x] [Feature 3]

### Code Quality Checks
- [x] All actors have UCLASS() macros
- [x] All properties have UPROPERTY() macros
- [x] All functions have UFUNCTION() macros where appropriate
- [x] All shaders have proper #include directives
- [x] All modules registered in .uplugin
- [x] No features simplified or removed

---

## Lessons Learned

### What Worked Well
1. [Lesson 1]
2. [Lesson 2]

### Challenges Encountered
1. [Challenge 1]
2. [Challenge 2]

### Recommendations
1. [Recommendation 1]
2. [Recommendation 2]

---

## Cross-Plugin Patterns

### Patterns Applicable to Other Plugins
1. **[Pattern Name]:** [Description and applicability]
2. **[Pattern Name]:** [Description and applicability]

### Backend Improvements Needed
1. **[Improvement]:** [Description and rationale]
2. **[Improvement]:** [Description and rationale]

---

## Build Statistics

- **Total Compilation Time:** [time]
- **KAIN Compilation Time:** [time]
- **UE5 Compilation Time:** [time]
- **Source Lines of KAIN:** [count]
- **Generated Lines of C++:** [count]
- **Compression Ratio:** [ratio] (1 line KAIN → X lines C++)
- **Iterations Required:** [count]
- **Source-Level Fixes:** [count]
- **Backend Fixes:** [count]

---

## Appendix: Complete Fix List

### All Source-Level Fixes Applied
1. [File:Line] - [Fix description]
2. [File:Line] - [Fix description]

### All Backend Fixes Applied
1. [Crate:File] - [Fix description]
2. [Crate:File] - [Fix description]
