# Specification Generation Rules

## Overview

This document defines the rules for generating plugin specifications in Factory Part 2. These rules ensure consistency, testability, and quality across all 50 plugins.

## EARS Pattern Rules (Requirements)

### Rule 1: EARS Structure

**All acceptance criteria MUST follow EARS (Easy Approach to Requirements Syntax) patterns:**

1. **Ubiquitous Requirements** (always active):
   ```
   THE <system> SHALL <action>
   ```
   Example: `THE VoxelEngine SHALL support infinite terrain generation`

2. **Event-Driven Requirements** (triggered by events):
   ```
   WHEN <trigger>, THE <system> SHALL <action>
   ```
   Example: `WHEN user clicks sculpt button, THE BrushSystem SHALL apply deformation`

3. **State-Driven Requirements** (active in specific states):
   ```
   WHILE <state>, THE <system> SHALL <action>
   ```
   Example: `WHILE in edit mode, THE EditorUI SHALL display property panel`

4. **Optional Features** (conditional requirements):
   ```
   WHERE <feature enabled>, THE <system> SHALL <action>
   ```
   Example: `WHERE networking enabled, THE ActorSystem SHALL replicate state`

5. **Universal Quantification** (applies to all instances):
   ```
   FOR ALL <entity type>, THE <system> SHALL <property>
   ```
   Example: `FOR ALL voxel chunks, THE ChunkManager SHALL maintain LOD levels`

### Rule 2: Requirement Atomicity

**Each acceptance criterion MUST be atomic (single, testable condition):**

❌ **Bad** (multiple conditions):
```
THE System SHALL validate input, process data, and generate output
```

✅ **Good** (atomic):
```
1. THE System SHALL validate input
2. THE System SHALL process validated data
3. THE System SHALL generate output from processed data
```

### Rule 3: Requirement Testability

**Each acceptance criterion MUST be objectively testable:**

❌ **Bad** (subjective):
```
THE UI SHALL be user-friendly
```

✅ **Good** (objective):
```
THE UI SHALL respond to user input within 16ms
```

### Rule 4: Requirement Completeness

**Each requirement MUST include:**
- User story (As a... I want... so that...)
- 10-15 acceptance criteria
- Clear system boundaries
- Input/output specifications
- Error handling expectations

### Rule 5: Requirement Traceability

**Each acceptance criterion MUST:**
- Have a unique identifier (Requirement X, Acceptance Criterion Y)
- Be referenced in design.md correctness properties
- Be referenced in tasks.md implementation tasks
- Be verifiable in feature_checklist.md

## Correctness Property Rules (Design)

### Rule 6: Universal Quantification

**All correctness properties MUST use universal quantification:**

❌ **Bad** (specific instance):
```
Property: The voxel at (0,0,0) has density 1.0
```

✅ **Good** (universal):
```
Property: For all voxels V, density(V) is in range [0.0, 1.0]
```

### Rule 7: Requirement References

**Each correctness property MUST reference specific requirements:**

```
Property 1: Voxel Density Bounds
Statement: For all voxels V, 0.0 <= density(V) <= 1.0
Requirement Reference: Requirement 1, Acceptance Criterion 3
Testability: Property-based test with random voxel generation
```

### Rule 8: Property Categories

**Correctness properties MUST be categorized:**

1. **Universal Properties**: Hold for all instances and all inputs
2. **Round-Trip Properties**: Data integrity through transformations
3. **Invariant Properties**: Data structure invariants that always hold
4. **Idempotence Properties**: Operations that can be safely repeated
5. **Network Properties**: Correct network behavior (if applicable)
6. **Performance Properties**: Performance targets are met

### Rule 9: Property Testability

**Each correctness property MUST include:**
- Clear statement with universal quantification
- Requirement reference (Requirement X, Acceptance Criterion Y)
- Testability declaration (property-based test, integration test, benchmark)
- Test strategy (pseudocode or description)

Example:
```
Property 5: Serialization Round-Trip
Statement: For all VoxelChunk C, deserialize(serialize(C)) = C
Requirement Reference: Requirement 7, Acceptance Criterion 6
Testability: Property-based test with random VoxelChunk generation
Test Strategy:
  @property_test
  fn test_serialization_round_trip():
      for all chunk in generate_random_chunks(1000):
          let serialized = serialize(chunk)
          let deserialized = deserialize(serialized)
          assert deserialized == chunk
```

## Round-Trip Property Rules

### Rule 10: Parser Round-Trip

**All parsers MUST have round-trip properties:**

```
Property: For all valid input I, unparse(parse(I)) = I
```

This ensures:
- Parser correctly interprets input
- Unparser correctly reconstructs input
- No information loss during parsing

### Rule 11: Serializer Round-Trip

**All serializers MUST have round-trip properties:**

```
Property: For all data D, deserialize(serialize(D)) = D
```

This ensures:
- Serializer correctly encodes data
- Deserializer correctly decodes data
- No data corruption during serialization

### Rule 12: Encoder Round-Trip

**All encoders MUST have round-trip properties:**

```
Property: For all data D, decode(encode(D)) = D
```

This ensures:
- Encoder correctly transforms data
- Decoder correctly reverses transformation
- Lossless encoding/decoding

### Rule 13: Round-Trip Exceptions

**Round-trip properties MAY allow for normalization:**

```
Property: For all valid input I, parse(unparse(parse(I))) = parse(I)
```

This allows:
- Whitespace normalization
- Comment removal
- Canonical formatting

But the semantic content MUST be preserved.

## Invariant Property Rules

### Rule 14: Data Structure Invariants

**All data structures MUST define invariants:**

```
struct VoxelChunk:
    position: Vec3
    size: Int
    voxels: Array<Voxel>

Invariant 1: voxels.len() == size * size * size
Invariant 2: For all v in voxels, 0.0 <= v.density <= 1.0
Invariant 3: position.x % size == 0 && position.y % size == 0 && position.z % size == 0
```

### Rule 15: Invariant Checking

**Invariants MUST be checked:**
- After construction
- After every mutation operation
- Before serialization
- After deserialization

### Rule 16: Invariant Violation Handling

**Invariant violations MUST:**
- Be detected immediately
- Log detailed error information
- Prevent further operations
- Provide recovery suggestions

## Idempotence Property Rules

### Rule 17: Idempotent Operations

**Operations that should be idempotent MUST have properties:**

```
Property: For all X, operation(operation(X)) = operation(X)
```

Common idempotent operations:
- Normalization: `normalize(normalize(v)) = normalize(v)`
- Clamping: `clamp(clamp(x, min, max), min, max) = clamp(x, min, max)`
- Deduplication: `dedupe(dedupe(list)) = dedupe(list)`
- Initialization: `init(init(system)) = init(system)`

### Rule 18: Non-Idempotent Operations

**Operations that are NOT idempotent MUST be documented:**

```
// NOT idempotent - each call increments counter
fn increment(counter: Int) -> Int:
    return counter + 1

// increment(increment(5)) = 7, not 6
```

### Rule 19: Idempotence Testing

**Idempotent operations MUST be tested:**

```
@property_test
fn test_normalize_idempotence():
    for all v in generate_random_vectors(1000):
        let once = normalize(v)
        let twice = normalize(once)
        assert once == twice
```

## Task Structure Rules

### Rule 20: Maximum Nesting Depth

**Tasks MUST have maximum 2 levels of nesting:**

✅ **Good**:
```
## Phase 1: Core Systems
### 1.1 Implement Component A
- [ ] 1.1.1 Define data structure
- [ ] 1.1.2 Implement methods
- [ ] 1.1.3 Add error handling
```

❌ **Bad** (too deep):
```
## Phase 1
### 1.1 System A
#### 1.1.1 Component A
##### 1.1.1.1 Method A
###### 1.1.1.1.1 Validation
```

### Rule 21: Decimal Notation

**Tasks MUST use decimal notation:**

```
## Phase 1: Core Systems
### 1.1 Implement Component A
- [ ] 1.1.1 Define data structure
- [ ] 1.1.2 Implement methods
### 1.2 Implement Component B
- [ ] 1.2.1 Define data structure
- [ ] 1.2.2 Implement methods

## Phase 2: Integration
### 2.1 Integrate Components
- [ ] 2.1.1 Connect A to B
- [ ] 2.1.2 Test integration
```

### Rule 22: Task Atomicity

**Each task MUST be atomic (single, completable action):**

❌ **Bad** (multiple actions):
```
- [ ] Implement system, test it, and document it
```

✅ **Good** (atomic):
```
- [ ] Implement system
- [ ] Test system
- [ ] Document system
```

### Rule 23: Task Requirement References

**Each task group MUST reference requirements:**

```
### 1.1 Implement VoxelEngine
- [ ] Create VoxelEngine actor
- [ ] Implement chunk generation
- [ ] Implement LOD system
- _Requirements: 1.1, 1.2, 1.3_
```

### Rule 24: Checkpoint Tasks

**Each phase MUST end with verification tasks:**

```
### 1.5 Phase 1 Verification
- [ ] Verify all Phase 1 requirements implemented
- [ ] Verify all Phase 1 correctness properties hold
- [ ] Verify no TODO comments in Phase 1 code
- [ ] Run Phase 1 compilation test
```

### Rule 25: Checkpoints

**Tasks MUST define checkpoints:**

```
## Checkpoints

- **Checkpoint 1** (End of Phase 2): Core systems implemented, basic functionality working
- **Checkpoint 2** (End of Phase 4): Shaders implemented, GPU features working
- **Checkpoint 3** (End of Phase 6): Blueprint integration complete
- **Checkpoint 4** (End of Phase 11): Performance optimized, all features complete
- **Checkpoint 5** (End of Phase 15): Quality gate passed, plugin complete
```

## Quality Standards Rules

### Rule 26: Zero TODOs

**All specifications and implementations MUST have zero TODO comments:**

❌ **Forbidden**:
```
// TODO: Implement this later
// FIXME: This is broken
// HACK: Temporary workaround
// XXX: This needs work
```

✅ **Required**:
```
// Full implementation with proper error handling
```

### Rule 27: Zero Placeholders

**All implementations MUST have zero placeholder code:**

❌ **Forbidden**:
```
fn complex_algorithm(input: Data) -> Result:
    // Placeholder - implement actual algorithm
    return Ok(default_value)
```

✅ **Required**:
```
fn complex_algorithm(input: Data) -> Result:
    // Full algorithm implementation
    let validated = validate_input(input)?
    let processed = process_data(validated)?
    let result = generate_output(processed)?
    return Ok(result)
```

### Rule 28: Zero Simplifications

**All implementations MUST be production-ready:**

❌ **Forbidden**:
```
// Simplified version for now - add full features later
fn basic_version():
    simple_implementation()
```

✅ **Required**:
```
// Full production implementation with all features
fn complete_version():
    full_implementation_with_all_features()
```

### Rule 29: Minimum Line Count

**All plugins MUST have minimum 5000 lines of KAIN code:**

This ensures:
- Sufficient complexity to demonstrate features
- $1000+ marketplace quality
- Comprehensive functionality
- Production-ready implementation

### Rule 30: Compression Ratio

**All plugins MUST achieve compression ratio >= 1:15:**

This demonstrates:
- Effective use of KAIN stdlib (200+ functions)
- Concise KAIN syntax
- Significant productivity advantage over vanilla C++
- Proper feature utilization

## Documentation Rules

### Rule 31: README Requirements

**Each plugin README.md MUST include:**
- Plugin overview and description
- Feature showcase with examples
- KAIN code examples
- Generated C++ examples
- Compilation instructions (`kain build --ue5`)
- UE5 integration instructions
- Capabilities impossible in vanilla UE5
- Compression ratio analysis

### Rule 32: Code Examples

**All code examples MUST:**
- Be complete and runnable
- Include both KAIN and generated C++
- Demonstrate key features
- Show proper error handling
- Follow best practices

### Rule 33: Requirement Documentation

**Each requirement MUST be documented with:**
- Clear user story
- 10-15 atomic acceptance criteria
- EARS pattern compliance
- Testability verification
- Traceability to design and tasks

### Rule 34: Design Documentation

**Each design MUST be documented with:**
- Architecture overview with diagrams
- Component breakdown with interfaces
- Data model definitions
- Correctness properties (8-12 properties)
- Implementation strategy
- Risk mitigation

### Rule 35: Task Documentation

**Each task list MUST be documented with:**
- Phase-based structure (10-15 phases)
- Decimal notation (max 2 levels)
- Atomic tasks with checkboxes
- Requirement references
- Checkpoints (5 checkpoints)
- Estimated effort

## Validation Rules

### Rule 36: Requirement Validation

**Before implementation, validate:**
- [ ] All requirements follow EARS patterns
- [ ] All acceptance criteria are atomic
- [ ] All acceptance criteria are testable
- [ ] All requirements have user stories
- [ ] All requirements are traceable

### Rule 37: Design Validation

**Before implementation, validate:**
- [ ] All correctness properties use universal quantification
- [ ] All correctness properties reference requirements
- [ ] All correctness properties are testable
- [ ] All data structures have invariants
- [ ] All parsers/serializers have round-trip properties

### Rule 38: Task Validation

**Before implementation, validate:**
- [ ] All tasks use decimal notation
- [ ] All tasks are atomic
- [ ] All tasks reference requirements
- [ ] All phases have verification tasks
- [ ] All checkpoints are defined

### Rule 39: Implementation Validation

**During implementation, validate:**
- [ ] Zero TODO comments
- [ ] Zero placeholder implementations
- [ ] Zero simplifications or shortcuts
- [ ] Minimum 5000 lines of KAIN code
- [ ] All requirements implemented

### Rule 40: Quality Gate Validation

**After implementation, validate:**
- [ ] Compression ratio >= 1:15
- [ ] All correctness properties verified
- [ ] All requirements coverage 100%
- [ ] `kain build --ue5` succeeds
- [ ] All expected files generated
- [ ] $1000+ marketplace quality achieved

## Template Usage Rules

### Rule 41: Template Placeholders

**All templates use placeholders in format `{PLACEHOLDER_NAME}`:**

Examples:
- `{PLUGIN_NAME}` - Plugin name (e.g., "VoxelSculptPro")
- `{PLUGIN_CATEGORY}` - Plugin category (e.g., "DCC Tools")
- `{PLUGIN_DESCRIPTION}` - Brief description
- `{FEATURE_1}` - First KAIN feature demonstrated
- `{COMPONENT_1_NAME}` - First component name
- `{REQ_REFS}` - Requirement references (e.g., "1.1, 1.2, 1.3")

### Rule 42: Template Instantiation

**When instantiating templates:**
1. Copy template to plugin directory
2. Replace ALL placeholders with actual values
3. Remove inapplicable sections (marked "if applicable")
4. Verify no placeholders remain
5. Verify all sections are complete

### Rule 43: Template Customization

**Templates MAY be customized for plugin-specific needs:**
- Add additional requirements (beyond 10)
- Add additional correctness properties (beyond 8)
- Add additional phases (beyond 15)
- Add plugin-specific sections
- Adjust structure for complex plugins

But MUST maintain:
- EARS pattern compliance
- Universal quantification in properties
- Decimal notation in tasks
- Quality standards (zero TODOs, 5000+ LOC, 1:15+ ratio)

### Rule 44: Template Consistency

**All plugins MUST use consistent structure:**
- requirements.md follows same section order
- design.md follows same section order
- tasks.md follows same phase structure
- feature_checklist.md follows same mapping structure
- KAIN.toml follows same configuration structure

This ensures:
- Easy navigation across plugins
- Consistent quality standards
- Simplified review process
- Automated validation

### Rule 45: Template Evolution

**Templates MAY evolve based on learnings:**
- Document template improvements in this file
- Update all templates simultaneously
- Notify all subagents of changes
- Re-validate existing plugins against new rules

## Specification Generation Workflow

### Step 1: Select Plugin from Catalog
- Read plugin concept from plugin_catalog.md
- Extract: name, description, domain, features, estimated LOC, unique value

### Step 2: Instantiate Requirements Template
- Copy requirements_template.md to plugin directory
- Replace {PLUGIN_NAME}, {PLUGIN_CATEGORY}, {PLUGIN_DESCRIPTION}
- Replace {FEATURE_1} through {FEATURE_8} with assigned features
- Define 10 requirements with 10-15 acceptance criteria each
- Verify all EARS patterns correct
- Verify all requirements testable

### Step 3: Instantiate Design Template
- Copy design_template.md to plugin directory
- Replace all placeholders
- Define 5-8 components with interfaces
- Define 8-12 correctness properties
- Verify all properties use universal quantification
- Verify all properties reference requirements
- Verify all properties are testable

### Step 4: Instantiate Tasks Template
- Copy tasks_template.md to plugin directory
- Replace all placeholders
- Define 10-15 phases with implementation tasks
- Add requirement references to each task group
- Define 5 checkpoints
- Estimate effort for each phase

### Step 5: Instantiate Feature Checklist Template
- Copy feature_checklist_template.md to plugin directory
- Replace all placeholders
- Map each KAIN feature to implementation locations
- Map each UE5 integration to implementation locations
- Define verification checklist

### Step 6: Instantiate KAIN.toml Template
- Copy KAIN_toml_template.toml to plugin directory
- Replace {PLUGIN_NAME}, {PLUGIN_DESCRIPTION}
- Configure modules (Runtime, Editor, Developer)
- Specify module dependencies
- Configure build settings

### Step 7: Validate Specification
- Run validation checks (Rules 36-40)
- Verify all placeholders replaced
- Verify all sections complete
- Verify traceability (requirements → design → tasks)
- Verify quality standards

### Step 8: Review and Approve
- Review specification for completeness
- Review specification for correctness
- Review specification for consistency
- Approve specification for implementation
- Assign to subagent for implementation

## Success Criteria

A specification is complete when:
- [ ] All templates instantiated
- [ ] All placeholders replaced
- [ ] All EARS patterns correct
- [ ] All correctness properties defined
- [ ] All tasks structured correctly
- [ ] All validation checks passed
- [ ] All quality standards met
- [ ] Specification approved for implementation
