# KAIN Generics Implementation - Parallel Subagent Execution Plan

**Date:** February 20, 2026  
**Status:** Phase 1 Complete, Ready for 10 Parallel Subagents  
**Total Remaining Time:** 12-16 hours with parallel execution

---

## PHASE 1 STATUS: ✅ COMPLETE

**Completed by:** Kiro AI  
**Date:** February 20, 2026  
**Time:** ~2 hours

**What Works:**
- ✅ Monomorphization wired into all 12 backends
- ✅ 5 unit tests passing
- ✅ Release build succeeds
- ✅ Generic functions work: `identity<T>`, `max<T>`, `pair<T,U>`
- ✅ Name mangling works: `identity_Int`, `identity_Float`

**What Doesn't Work Yet:**
- ❌ Generic structs (`struct Box<T>`)
- ❌ Generic methods (`impl<T> Box<T>`)
- ❌ Explicit type args (`identity<Int>(42)`)
- ❌ Backend integration testing
- ❌ Optimization (inline hints, @specialize)
- ❌ Documentation

---

## 10 SUBAGENT ASSIGNMENT

### Wave 1: Parallel Execution (8 Subagents)

| Subagent | Phase | Task | Time | Files | Dependencies |
|----------|-------|------|------|-------|--------------|
| **Subagent 1** | 2+3 | Generic structs + methods | 8-12h | `monomorphize.rs` | Phase 1 ✅ |
| **Subagent 2** | 4 | Optimization | 4-8h | `monomorphize.rs`, codegen files | Phase 1 ✅ |
| **Subagent 3** | 2 | UE5 testing + plugins | 3-5h | `ue5/tests/`, `testing/generics/` | Phase 1 ✅ |
| **Subagent 4** | 2 | Web backends testing | 3-5h | `web/tests/` | Phase 1 ✅ |
| **Subagent 5** | 2 | GPU backends testing | 3-5h | `gpu/tests/` | Phase 1 ✅ |
| **Subagent 6** | 2 | Sys backends testing | 3-5h | `sys/tests/` | Phase 1 ✅ |
| **Subagent 7** | 2 | Editor backend testing | 3-5h | `ue5-editor/tests/` | Phase 1 ✅ |
| **Subagent 9** | 2 | Unit test expansion | 2-4h | `kain-core/tests/` | Phase 1 ✅ |
| **Subagent 10** | 2 | E2E test plugins | 4-6h | `testing/generics/` | Phase 1 ✅ |

### Wave 2: After Wave 1 (1 Subagent)

| Subagent | Phase | Task | Time | Files | Dependencies |
|----------|-------|------|------|-------|--------------|
| **Subagent 8** | 5 | Documentation | 8-12h | `docs/` | Phases 1-3 |

---

## DETAILED TASK BREAKDOWN

### Subagent 1: Generic Structs + Methods (8-12 hours)

**Priority:** HIGH (blocks other features)

**Tasks:**
1. Extend `MonoContext` to track generic struct definitions
2. Add struct instantiation logic (Box_Int, Box_Float)
3. Implement generic method support in impl blocks
4. Update type resolver for generic struct references
5. Add 7 new unit tests (tests 6-13)
6. Test with all backends

**Files to Modify:**
- `crates/kain-core/src/monomorphize.rs`
- `crates/kain-core/tests/monomorphize_test.rs`

**Acceptance Criteria:**
- Generic structs compile: `struct Box<T>`
- Generic methods work: `impl<T> Box<T>`
- 12/12 unit tests pass
- All backends handle generic structs

**Deliverables:**
- Modified monomorphize.rs with struct support
- 7 new passing tests
- Documentation of changes

---

### Subagent 2: Optimization (4-8 hours)

**Priority:** MEDIUM (independent of other work)

**Tasks:**
1. Add inline hints for small functions (< 5 statements)
2. Implement @specialize attribute parsing and handling
3. Create MonoConfig system with CLI flags
4. Add benchmarks for compilation time
5. Profile unification algorithm
6. Document optimization strategies

**Files to Modify:**
- `crates/kain-core/src/monomorphize.rs` (MonoConfig)
- `crates/kain-core/src/ast.rs` (@specialize attribute)
- `crates/kain-core/src/parser.rs` (parse @specialize)
- `crates/ue5/src/codegen_ue5.rs` (FORCEINLINE)
- `crates/sys/src/codegen_rust.rs` (#[inline])
- `crates/cli/src/main.rs` (CLI flags)

**Acceptance Criteria:**
- Inline hints appear in generated code
- @specialize attribute works
- MonoConfig system functional
- Compilation time increase < 10%
- Benchmarks show improvement

**Deliverables:**
- MonoConfig implementation
- @specialize attribute support
- Benchmark results
- Performance report

---

### Subagent 3: UE5 Backend Testing (3-5 hours)

**Priority:** HIGH (validates core functionality)

**Tasks:**
1. Create `crates/ue5/tests/generic_codegen_tests.rs`
2. Write 10+ integration tests for UE5 backend
3. Create `testing/generics/GenericMath.kn` plugin
4. Build plugin with `kain build --ue5`
5. Verify C++ output is valid
6. Test in actual UE5 project (if available)
7. Test Blueprint integration

**Files to Create:**
- `crates/ue5/tests/generic_codegen_tests.rs`
- `testing/generics/GenericMath.kn`
- `testing/generics/KAIN.toml`

**Acceptance Criteria:**
- All UE5 integration tests pass
- GenericMath.kn compiles without errors
- Generated C++ is valid
- Blueprint functions work with generics
- No UE5 compilation errors

**Deliverables:**
- UE5 integration test suite
- GenericMath.kn test plugin
- Test results report

---

### Subagent 4: Web Backends Testing (3-5 hours)

**Priority:** MEDIUM

**Tasks:**
1. Create `crates/web/tests/generic_codegen_tests.rs`
2. Write tests for WASM bytecode generation
3. Write tests for JavaScript output
4. Write tests for Hybrid mode
5. Create web test page
6. Verify runtime behavior in browser

**Files to Create:**
- `crates/web/tests/generic_codegen_tests.rs`
- `testing/generics/web_test.html`

**Acceptance Criteria:**
- All web integration tests pass
- WASM bytecode is valid
- JavaScript output is correct
- Hybrid mode works
- Browser tests pass

**Deliverables:**
- Web integration test suite
- Browser test page
- Test results report

---

### Subagent 5: GPU Backends Testing (3-5 hours)

**Priority:** MEDIUM

**Tasks:**
1. Create `crates/gpu/tests/generic_codegen_tests.rs`
2. Write tests for HLSL shader compilation
3. Write tests for SPIR-V generation
4. Create shader test plugin
5. Compile shaders in UE5 (if available)
6. Verify rendering works

**Files to Create:**
- `crates/gpu/tests/generic_codegen_tests.rs`
- `testing/generics/GenericShaders.kn`

**Acceptance Criteria:**
- All GPU integration tests pass
- HLSL output is valid
- SPIR-V bytecode is valid
- Shaders compile in UE5
- Rendering works correctly

**Deliverables:**
- GPU integration test suite
- GenericShaders.kn test plugin
- Test results report

---

### Subagent 6: Sys Backends Testing (3-5 hours)

**Priority:** MEDIUM

**Tasks:**
1. Create `crates/sys/tests/generic_codegen_tests.rs`
2. Write tests for LLVM IR generation
3. Write tests for Rust output (consider preserving generics)
4. Write tests for C++ output
5. Create sys test programs
6. Compile and run test programs

**Files to Create:**
- `crates/sys/tests/generic_codegen_tests.rs`
- `testing/generics/sys_test.kn`

**Acceptance Criteria:**
- All sys integration tests pass
- LLVM IR is valid
- Rust output compiles
- C++ output compiles
- Test programs run correctly

**Deliverables:**
- Sys integration test suite
- Test programs
- Test results report

---

### Subagent 7: Editor Backend Testing (3-5 hours)

**Priority:** LOW (editor features less critical)

**Tasks:**
1. Create `crates/ue5-editor/tests/generic_codegen_tests.rs`
2. Write tests for Slate widgets with generic properties
3. Write tests for Details panels with generic types
4. Create editor test plugin
5. Build in UE5 (if available)
6. Verify editor UI works

**Files to Create:**
- `crates/ue5-editor/tests/generic_codegen_tests.rs`
- `testing/generics/GenericEditor.kn`

**Acceptance Criteria:**
- All editor integration tests pass
- Slate widgets compile
- Details panels compile
- Editor UI works correctly

**Deliverables:**
- Editor integration test suite
- GenericEditor.kn test plugin
- Test results report

---

### Subagent 8: Documentation (8-12 hours)

**Priority:** MEDIUM (can start after Wave 1)

**Tasks:**
1. Write language guide for generics
2. Create cookbook with generic examples
3. Document monomorphization internals
4. Write migration guide
5. Update API documentation
6. Create troubleshooting guide
7. Add examples to README

**Files to Create:**
- `docs/language_guide/generics.md`
- `docs/cookbook/generic_examples.md`
- `docs/internals/monomorphization.md`
- `docs/migration/adopting_generics.md`
- Update `README.md`

**Acceptance Criteria:**
- Users can write generic code from docs alone
- Contributors understand internals
- Migration path is clear
- Examples are comprehensive

**Deliverables:**
- Complete documentation set
- Updated README
- Example code

---

### Subagent 9: Unit Test Expansion (2-4 hours)

**Priority:** HIGH (validates core functionality)

**Tasks:**
1. Add test 6: Generic function with struct types
2. Add test 7: Generic function with trait bounds
3. Add test 8: Generic method calls (after Subagent 1)
4. Add test 9: Generic structs (after Subagent 1)
5. Add test 10: Higher-kinded types (Option, Result, Array)
6. Add test 11: Error - unbound type parameter
7. Add test 12: Error - trait bound not satisfied
8. Add test 13: Error - type mismatch in generic call

**Files to Modify:**
- `crates/kain-core/tests/monomorphize_test.rs`

**Acceptance Criteria:**
- 13/13 tests pass
- Error cases handled correctly
- Edge cases covered

**Deliverables:**
- 8 new passing tests
- Test coverage report

---

### Subagent 10: E2E Test Plugins (4-6 hours)

**Priority:** HIGH (validates real-world usage)

**Tasks:**
1. Create `testing/generics/GenericMath.kn`
2. Create `testing/generics/GenericCollections.kn` (after Subagent 1)
3. Create `testing/generics/GenericActors.kn` (after Subagent 1)
4. Build all plugins with `kain build --ue5`
5. Verify C++ output is valid
6. Test in actual UE5 project (if available)

**Files to Create:**
- `testing/generics/GenericMath.kn`
- `testing/generics/GenericCollections.kn`
- `testing/generics/GenericActors.kn`
- `testing/generics/KAIN.toml`

**Acceptance Criteria:**
- All 3 plugins compile without errors
- Generated C++ is valid
- Plugins run in UE5
- Blueprint integration works

**Deliverables:**
- 3 complete test plugins
- Build results
- Test report

---

## COORDINATION & DEPENDENCIES

### Critical Path
1. **Subagent 1** must complete before Subagents 9 and 10 can finish (generic structs/methods needed)
2. **Subagent 8** should start after Wave 1 completes (needs features to document)

### Independent Work (Can Run in Parallel)
- Subagents 2, 3, 4, 5, 6, 7 are fully independent
- Subagent 9 can start immediately (tests 6-7, 10-13 don't need Subagent 1)
- Subagent 10 can start GenericMath.kn immediately

### Communication
- Each subagent should report completion status
- Share test results in central location
- Coordinate on shared files (monomorphize.rs)

---

## SUCCESS CRITERIA

**Phase 2 Complete When:**
- [ ] Generic structs work (Subagent 1)
- [ ] Generic methods work (Subagent 1)
- [ ] All 13 unit tests pass (Subagent 9)
- [ ] All backend tests pass (Subagents 3-7)
- [ ] All 3 E2E plugins work (Subagent 10)

**Phase 3 Complete When:**
- [ ] Optimization implemented (Subagent 2)
- [ ] Benchmarks show <10% compile time increase
- [ ] @specialize attribute works

**Phase 5 Complete When:**
- [ ] All documentation written (Subagent 8)
- [ ] Users can write generic code from docs
- [ ] Migration guide is clear

**Overall Success:**
- [ ] All 13 unit tests pass
- [ ] All backend integration tests pass
- [ ] All 3 E2E plugins compile and run
- [ ] Documentation is complete
- [ ] Performance is acceptable

---

## TIMELINE

**With 10 Parallel Subagents:**
- **Wave 1:** 8-12 hours (Subagents 1-7, 9-10 in parallel)
- **Wave 2:** 8-12 hours (Subagent 8 after Wave 1)
- **Total:** 12-16 hours wall-clock time

**Sequential (for comparison):**
- Would take 48-72 hours

**Speedup:** 4-6x faster with parallel execution

---

## GETTING STARTED

1. **Assign subagents** to tasks based on expertise
2. **Create branches** for each subagent (optional)
3. **Start Wave 1** (8 subagents)
4. **Monitor progress** and coordinate on shared files
5. **Start Wave 2** after Wave 1 completes
6. **Merge and test** all changes together
7. **Celebrate** 🎉

---

**Ready to execute. Let's complete the remaining 75% of generic programming support!**
