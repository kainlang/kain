# Feature Checklist: {PLUGIN_NAME}

## Overview

This document maps KAIN features to their implementation locations in {PLUGIN_NAME}. Use this checklist to verify all assigned features are properly demonstrated.

## KAIN Feature Mapping

### Feature 1: {FEATURE_1}

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

- [ ] File: `src/{FILE_2}.kn` (if applicable)
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Private/{FILE_2}.cpp`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 2: {FEATURE_2}

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

- [ ] File: `src/{FILE_2}.kn` (if applicable)
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Private/{FILE_2}.cpp`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 3: {FEATURE_3}

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 4: {FEATURE_4}

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 5: {FEATURE_5}

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 6: {FEATURE_6} (if applicable)

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 7: {FEATURE_7} (if applicable)

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

### Feature 8: {FEATURE_8} (if applicable)

**Description**: {Feature description from feature_matrix.md}

**Implementation Locations**:
- [ ] File: `src/{FILE_1}.kn`
  - Lines: {START_LINE}-{END_LINE}
  - Usage: {How feature is used}
  - Generated C++: `Source/{MODULE}/Public/{FILE_1}.h`

**Verification**:
- [ ] Feature syntax correct
- [ ] Generated C++ correct
- [ ] UE5 integration working
- [ ] Documented in README.md

## UE5 System Integration

### {UE5_SYSTEM_1}

**Integration Points**:
- [ ] File: `src/{FILE}.kn`
  - Component: {COMPONENT_NAME}
  - Integration: {How integrated}
  - Generated C++: {Generated files}

**Verification**:
- [ ] Integration working
- [ ] UE5 lifecycle correct
- [ ] Memory management correct

### {UE5_SYSTEM_2}

**Integration Points**:
- [ ] File: `src/{FILE}.kn`
  - Component: {COMPONENT_NAME}
  - Integration: {How integrated}
  - Generated C++: {Generated files}

**Verification**:
- [ ] Integration working
- [ ] UE5 lifecycle correct
- [ ] Memory management correct

### {UE5_SYSTEM_3}

**Integration Points**:
- [ ] File: `src/{FILE}.kn`
  - Component: {COMPONENT_NAME}
  - Integration: {How integrated}
  - Generated C++: {Generated files}

**Verification**:
- [ ] Integration working
- [ ] UE5 lifecycle correct
- [ ] Memory management correct

## Generated File Mapping

### Actors

| KAIN File | KAIN Actor | Generated C++ | UE5 Base Class |
|-----------|------------|---------------|----------------|
| `src/{FILE}.kn` | `actor {NAME}` | `A{NAME}.h/.cpp` | `AActor` |
| `src/{FILE}.kn` | `actor {NAME}` | `A{NAME}.h/.cpp` | `{BASE_CLASS}` |

### Components

| KAIN File | KAIN Component | Generated C++ | UE5 Base Class |
|-----------|----------------|---------------|----------------|
| `src/{FILE}.kn` | `@component struct {NAME}` | `U{NAME}.h/.cpp` | `UActorComponent` |
| `src/{FILE}.kn` | `@component struct {NAME}` | `U{NAME}.h/.cpp` | `USceneComponent` |

### Structs

| KAIN File | KAIN Struct | Generated C++ | UE5 Macro |
|-----------|-------------|---------------|-----------|
| `src/{FILE}.kn` | `struct {NAME}` | `F{NAME}` | `USTRUCT(BlueprintType)` |
| `src/{FILE}.kn` | `@datatable struct {NAME}` | `F{NAME}` | `USTRUCT(BlueprintType)` + `FTableRowBase` |

### Enums

| KAIN File | KAIN Enum | Generated C++ | UE5 Macro |
|-----------|-----------|---------------|-----------|
| `src/{FILE}.kn` | `enum {NAME}` | `E{NAME}` | `UENUM(BlueprintType)` |

### Shaders (if applicable)

| KAIN File | KAIN Shader | Generated USF | Generated C++ |
|-----------|-------------|---------------|---------------|
| `src/shaders/{FILE}.kn` | `shader compute {NAME}` | `{NAME}.usf` | `F{NAME}CS` |
| `src/shaders/{FILE}.kn` | `shader fragment {NAME}` | `{NAME}.usf` | `F{NAME}PS` |

### Materials (if applicable)

| KAIN File | KAIN Material | Generated Asset | Node Count |
|-----------|---------------|-----------------|------------|
| `src/materials/{FILE}.kn` | `material {NAME}` | `{NAME}.uasset` | {COUNT} |

### Editor UI (if applicable)

| KAIN File | KAIN Editor Component | Generated C++ | UE5 Base Class |
|-----------|----------------------|---------------|----------------|
| `src/editor/{FILE}.kn` | `@slate struct {NAME}` | `S{NAME}.h/.cpp` | `SCompoundWidget` |
| `src/editor/{FILE}.kn` | `@details struct {NAME}` | `F{NAME}Customization.h/.cpp` | `IDetailCustomization` |
| `src/editor/{FILE}.kn` | `@viewport struct {NAME}` | `S{NAME}.h/.cpp` | `SEditorViewport` |

### Graph Editors (if applicable)

| KAIN File | KAIN Graph Component | Generated C++ | UE5 Base Class |
|-----------|---------------------|---------------|----------------|
| `src/graph/{FILE}.kn` | `@graph_runtime graph {NAME}` | `U{NAME}Instance.h/.cpp` | `UObject` |
| `src/graph/{FILE}.kn` | `@node_data node {NAME}` | `UNodeData_{NAME}.h/.cpp` | `UObject` |
| `src/graph/{FILE}.kn` | `@graph_editor graph {NAME}` | `U{NAME}Node.h/.cpp` | `UEdGraphNode` |

## Stdlib Function Usage

### Category: {STDLIB_CATEGORY_1}

**Functions Used**:
- [ ] `{FUNCTION_1}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`
- [ ] `{FUNCTION_2}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`
- [ ] `{FUNCTION_3}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`

**Compression Contribution**: {RATIO} (estimated)

### Category: {STDLIB_CATEGORY_2}

**Functions Used**:
- [ ] `{FUNCTION_1}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`
- [ ] `{FUNCTION_2}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`

**Compression Contribution**: {RATIO} (estimated)

### Category: {STDLIB_CATEGORY_3}

**Functions Used**:
- [ ] `{FUNCTION_1}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`
- [ ] `{FUNCTION_2}({PARAMS})` - Used in `src/{FILE}.kn:{LINE}`

**Compression Contribution**: {RATIO} (estimated)

## Compression Ratio Analysis

### KAIN Source Lines
- `src/{FILE_1}.kn`: {LINES} lines
- `src/{FILE_2}.kn`: {LINES} lines
- `src/{FILE_3}.kn`: {LINES} lines
- `src/{FILE_4}.kn`: {LINES} lines (if applicable)
- `src/{FILE_5}.kn`: {LINES} lines (if applicable)
- **Total KAIN Lines**: {TOTAL_KAIN_LINES}

### Generated C++ Lines
- `Source/{MODULE}/Public/{FILE_1}.h`: {LINES} lines
- `Source/{MODULE}/Private/{FILE_1}.cpp`: {LINES} lines
- `Source/{MODULE}/Public/{FILE_2}.h`: {LINES} lines
- `Source/{MODULE}/Private/{FILE_2}.cpp`: {LINES} lines
- (... additional files ...)
- **Total C++ Lines**: {TOTAL_CPP_LINES}

### Compression Ratio
- **Base Ratio** (without stdlib): 1:{BASE_RATIO}
- **With Stdlib**: 1:{STDLIB_RATIO}
- **Target**: 1:15+
- **Status**: {PASS/FAIL}

## Verification Checklist

### Feature Coverage
- [ ] All assigned KAIN features demonstrated
- [ ] All features documented in README.md
- [ ] All features have code examples
- [ ] All features generate correct C++

### UE5 Integration
- [ ] All UE5 systems integrated correctly
- [ ] All UE5 lifecycle methods implemented
- [ ] All UE5 macros generated correctly
- [ ] All UE5 naming conventions followed

### Code Quality
- [ ] Zero TODO comments
- [ ] Zero placeholder implementations
- [ ] Zero simplifications or shortcuts
- [ ] Minimum {MIN_LOC} lines of KAIN code
- [ ] Compression ratio >= 1:15

### Compilation
- [ ] `kain build --ue5` succeeds
- [ ] All expected files generated
- [ ] No compilation errors
- [ ] No compilation warnings

### Documentation
- [ ] README.md complete
- [ ] Code examples provided
- [ ] Compilation instructions provided
- [ ] UE5 integration instructions provided

### Quality Gate
- [ ] All requirements implemented
- [ ] All correctness properties verified
- [ ] $1000+ marketplace quality achieved
- [ ] Plugin ready for Factory Part 2 catalog
