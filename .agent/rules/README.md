# KAIN Steering Docs - Quick Reference

## PROJECT CONTEXT

**CRITICAL**: This is a SOLO DEV project. One person built everything. When docs mention "other devs", that means OTHER LLMs helping with development, NOT human developers. This is a private tool that will never be public.

**Your Role**: You are an LLM assistant helping the solo dev iterate quickly. Focus on speed and what works NOW, not enterprise best practices for teams.

## Architecture Overview

```
.kn source → Rust Compiler (95%) → Python Post-Processing (5%) → Production UE5 C++
```

**Rust**: Core compilation (parsing, type checking, AST merging, code generation)
**Python**: Edge case auto-fixes (missing includes, duplicates, formatting)
**Integration**: Built into `kain build --ue5` command

## File Guide

### 1. `kain.md` - Language Patterns
- KAIN syntax and patterns
- Common code examples
- Naming conventions
- Attribute reference
- Quick reference guide

**Use when**: Writing KAIN code, need syntax examples

### 2. `llm-first-development.md` - Philosophy
- LLM-optimized design principles
- Pipeline architecture (Rust + Python)
- Validation layers
- Token efficiency
- Production quality guarantees

**Use when**: Understanding overall system design, optimization strategies

### 3. `marketplacedomination.md` - Strategy
- Marketplace domination plan
- Plugin categories and priorities
- Pricing strategy
- Revenue projections
- Execution rules

**Use when**: Planning plugin development, marketplace strategy

### 4. `pipeline.md` - Compilation Details
- Code generation details
- Type mappings
- Performance characteristics
- Common patterns
- Troubleshooting

**Use when**: Understanding compilation process, debugging issues

### 5. `ue5-pluginbuilder.md` - Build System
- Automated build system
- KAIN.toml configuration
- Multi-file compilation
- Python post-processing
- Development workflow

**Use when**: Building plugins, configuring projects, troubleshooting builds

## Quick Commands

### Build Plugin
```bash
cd YourPlugin/
kain build --ue5
```

### Rebuild Rust Compiler (after modifying Rust code)
```bash
cd kain/
./cargo-build-install.ps1 --release
```

### Fix Stuck Changes
```bash
# Delete Source/ folder, then rebuild
rm -rf Source/
kain build --ue5
```

## Key Files in Project

### Rust Compiler
- `kain/crates/cli/src/packager.rs` - Main build orchestration
- `kain/crates/ue5/src/codegen_ue5.rs` - UE5 code generation
- `kain/crates/ue5-editor/src/editor/slate.rs` - Slate widget generation
- `kain/crates/ue5/src/ue5/oracle.rs` - Validation system
- `kain/crates/ue5/src/ue5/oracle_enhanced.rs` - Enhanced validation with Python hooks

### Python Post-Processing
- `kain/python/validation_rules.py` - Validation rules (extensible)
- `kain/python/post_process.py` - Main entry point (TODO)

### Build Scripts
- `kain/cargo-build-install.ps1` - Rebuild and install Rust compiler

## Common Patterns

### Actor with RPCs
```kn
actor GameMode:
    state score: Int = 0
    
    on Server_StartMatch():
        score = 0
    
    on Client_UpdateScore(new_score: Int):
        println("Score: {new_score}")
```

### Component (no defaults!)
```kn
@component
struct HealthComponent:
    @replicated
    current: Float
    max: Float
```

### DataTable
```kn
@datatable
struct ItemData:
    id: Int
    name: String
    value: Int
```

### Blueprint Function
```kn
@blueprint
fn CalculateDamage(base: Float, mult: Float) -> Float:
    return base * mult
```

### Shader
```kn
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
```

## Critical Rules

1. **Components CANNOT have default values** - Only actors can
2. **Delegates use simple syntax** - `type MyDelegate = delegate(Float, Float)` (no param names)
3. **Shaders need return types** - `shader compute Name(param: Type) -> ReturnType:`
4. **Always use `kain build --ue5`** - Don't compile files individually
5. **Rebuild Rust after changes** - Run `cargo-build-install.ps1 --release`
6. **Delete Source/ if stuck** - Sometimes needed for changes to take effect

## Troubleshooting

### Build fails with cryptic errors
- Check KAIN syntax first
- Verify all files in KAIN.toml exist
- Ensure components don't have default values
- Check delegate syntax (no parameter names)

### Changes not taking effect
- Rebuild Rust compiler: `cargo-build-install.ps1 --release`
- Delete Source/ folder and rebuild
- Regenerate UE5 project files

### UE5 compilation errors
- Regenerate project files
- Clean and rebuild solution
- Check .Build.cs dependencies
- Verify shader virtual paths

## Philosophy

**Speed over perfection**: Ship fast, iterate based on real usage
**LLM-first**: Designed for AI code generation, not human coding
**Zero manual fixes**: If it compiles, it's production-ready
**Python safety net**: Auto-fixes edge cases so LLMs don't have to worry

## Success Metrics

- **Build time**: < 2 seconds for typical plugin
- **Error clarity**: File:line:column with fix suggestions
- **Manual fixes**: Zero (Python handles edge cases)
- **Production ready**: If `kain build --ue5` succeeds, ship it

---

**Remember**: You're helping a solo dev iterate fast. No corporate patterns, just make it work. Focus on speed and what works NOW.
