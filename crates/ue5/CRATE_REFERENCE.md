# ue5 — UE5 Runtime Codegen Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — the largest single crate in the workspace. Generates UE5 C++ actors, components, subsystems, replication, networking, async tasks, and state machines.

---

## Purpose

The primary UE5 backend. Takes a `TypedProgram` and generates production-quality UE5 C++ source — `.h` header + `.cpp` implementation files — for plugin compilation.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `codegen_ue5.rs` | 290KB | Main codegen — all actor/component/struct/enum/subsystem/impl generation |
| `ue5/oracle.rs` | 70KB | UE5 semantic validator ("The Oracle") — runs before codegen |
| `ue5/stdlib_resolver.rs` | 48KB | Maps KAIN stdlib calls to UE5 equivalents |
| `ue5/engine_knowledge.rs` | 46KB | `EngineKnowledge` — loads 14 JSON metadata files |
| `ue5/types.rs` | 29KB | `TypeMapper` — KAIN types → UE5 C++ types |
| `ue5/uht_rules.rs` | 26KB | `UhtRules` — loads UHT JSON rules |
| `ue5/validation_rules.rs` | 14KB | `ValidationRules` + custom rule schema |
| `ue5/metadata_validation.rs` | 34KB | Schema validation for all 14 JSON metadata files |
| `ue5/module_graph.rs` | 19KB | Module dependency graph + DependencyResolver |
| `ue5/naming.rs` | 18KB | Naming system (prefixes, reserved name collision detection) |
| `ue5/widget_registry.rs` | 21KB | `WidgetRegistry` for Slate/UMG widgets |
| `ue5/virtual_obligations.rs` | 22KB | `VirtualObligations` — tracks required UInterface method overrides |
| `ue5/editor_attributes.rs` | 14KB | Editor attribute parsing for Detail panels, sliders, etc. |
| `ue5/context.rs` | 18KB | `Ue5Context` — full compilation context holder |
| `ue5/syntax.rs` | 5KB | UE5 C++ syntax helpers |
| `ue5/traits.rs` | 6KB | UE5 trait → interface mapping |
| `ue5/kain_markers.rs` | 4KB | `C_UNION_ATTR`, `C_BITFIELD_ATTR` constants |
| `ue5/metadata_hotreload.rs` | 12KB | Hot-reload of JSON metadata during development |
| `ue5/project.rs` | 6KB | Project root detection |
| `ue5/resolver.rs` | 4KB | Type resolution helpers |
| `ue5/logging.rs` | 3KB | UE_LOG + category codegen |
| `async_task_codegen.rs` | 25KB | `@async_task` → `FRunnable` |
| `async_task_ir.rs` | 15KB | Async task IR structs |
| `blueprint_codegen.rs` | 23KB | Blueprint node `UK2Node` codegen |
| `blueprint_ir.rs` | 12KB | Blueprint node IR structs |
| `network_sync_codegen.rs` | 19KB | Advanced network state sync codegen |
| `network_sync_ir.rs` | 19KB | Network sync IR |
| `state_machine_codegen.rs` | 21KB | `@state_machine` → state enum + `FStateMachine` |
| `state_machine_ir.rs` | 11KB | State machine IR |

---

## Public API (`lib.rs`)

```rust
pub fn generate(program: &TypedProgram) -> KainResult<GeneratedFiles>
pub fn generate_with_context(program: &TypedProgram, ctx: &Ue5Context) -> KainResult<GeneratedFiles>
pub fn validate_program(program: &TypedProgram, ...) -> KainResult<()>
```

`GeneratedFiles` is a map of filename → content for all generated `.h` and `.cpp` files.

---

## The Oracle (`ue5/oracle.rs`, 70KB)

Semantic validator that runs **before** C++ codegen. Catches UHT errors in ~10ms versus 2+ minute compile cycle.

**4 validation entry points:**

| Function | Use case |
|---|---|
| `validate_program(program, span_mapper, filename)` | Basic validation (loads EngineKnowledge) |
| `validate_program_with_knowledge(program, kb, ...)` | When you already have an `EngineKnowledge` |
| `validate_program_full(program, kb, uht, ...)` | Full validation with UHT rules |
| `validate_program_with_custom_rules(program, kb, uht, custom_rules, ...)` | + custom `validation_rules.json` |

**What The Oracle validates:**

| Check | Validates |
|---|---|
| RPC naming | `Server_*` / `Client_*` / `Multicast_*` prefix enforcement |
| Replication rules | `@replicated` fields on non-replicating actors flagged |
| Blueprint rules | `@blueprint_event` / `@blueprint_callable` compatibility |
| Engine name collisions | KAIN name vs. 500+ UE5 built-in type names (from `engine_knowledge.json`) |
| UFUNCTION specifiers | Conflicting function specifiers flagged |
| Struct property rules | No `UPROPERTY` on inner anonymous structs |
| Component validation | Component fields require proper attachment roots |
| Custom rules | 7 categories, 7 condition types from JSON (no recompile) |
| UHT Phase 2 | Data-driven type collision, forbidden types, missing attributes |

`FunctionFlags` struct mirrors UE5's `EFunctionFlags` — tracks BlueprintCallable, BlueprintPure, BlueprintNativeEvent, Server, Client, NetMulticast, etc.

---

## Naming System (`ue5/naming.rs`, 18KB)

Data-driven, enforced through `OnceLock<HashSet<String>>` caches (loaded once from JSON):

| Function | Output | Example |
|---|---|---|
| `to_actor_name(name)` | `A` prefix | `Player` → `APlayer` |
| `to_struct_name(name)` | `F` prefix | `Transform` → `FTransform` |
| `to_enum_name(name)` | `E` prefix | `Direction` → `EDirection` |
| `to_uobject_name(name)` | `U` prefix | `Component` → `UComponent` |

Anti-double-prefix: if the name already starts with the required prefix letter it is not re-prefixed.

Reserved name collision check: `check_engine_name_collision(engine_name, kain_name)` loads `RESERVED_NAMES` from metadata JSON + extends from `engine_knowledge.json` — raises descriptive errors for conflicts.

Also validates against: C++ keywords list (60+ keywords) + UE5 macro names (`UCLASS`, `USTRUCT`, `UENUM`, `UFUNCTION`, `UPROPERTY`, `UMETA`, `GENERATED_BODY`, `UPARAM`, `UDELEGATE`, `TEXT`, `LOCTEXT`, `NSLOCTEXT`).

---

## Engine Knowledge (`ue5/engine_knowledge.rs`)

Loads and caches 14 JSON metadata files:

| File | Content |
|---|---|
| `engine_knowledge.json` | 500+ engine types with modules, headers, categories |
| `widget_registry.json` | Slate/UMG widget definitions |
| `shader_knowledge.json` | Shader parameter types and semantics |
| `uht_rules.json` | UHT validation rules |
| `module_graph.json` | Engine module dependency graph |
| `validation_rules.json` + schema | Custom validation rule definitions |

Hot-reload capable via `metadata_hotreload.rs` — monitoring file system changes during development.

Multi-UE5-version support: 5.4 through 5.7. Multi-drive installation detection on Windows (C:\ through Z:\).

---

## TypeMapper (`ue5/types.rs`, 29KB)

Converts KAIN types to UE5 C++ types:

| KAIN | UE5 C++ |
|---|---|
| `Int` | `int32` |
| `Float` | `float` |
| `Bool` | `bool` |
| `String` | `FString` |
| `Array<T>` | `TArray<T>` |
| `Option<T>` | `TOptional<T>` |
| `Actor T` | `AT*` |
| `struct T` | `FT` |
| Pointer types | Raw pointer with optional `TWeakObjectPtr` |

Pointer detection: recognizes UObject-derived types and uses pointer semantics automatically.

---

## Codegen Areas (`codegen_ue5.rs`, 290KB)

The main file handles all UE5 construct generation:

| KAIN construct | Generated UE5 C++ |
|---|---|
| `actor Name` | `AName : public AActor` with `UCLASS`, `GENERATED_BODY`, `BeginPlay`, `Tick` |
| `@component struct Name` | `UNameComponent : public UActorComponent` |
| `@subsystem struct Name` | `UNameSubsystem : public UWorldSubsystem` |
| `@datatable struct Name` | `FName : public FTableRowBase` |
| `struct Name` | `USTRUCT(BlueprintType) struct FName` |
| `enum Name` | `UENUM(BlueprintType) enum class EName : uint8` |
| `on Server_X(args)` | `UFUNCTION(Server, Reliable) void Server_X(args); void Server_X_Implementation(); bool Server_X_Validate();` |
| `@replicated field` | `UPROPERTY(Replicated)` + `GetLifetimeReplicatedProps` + `DOREPLIFETIME` |
| `@blueprint_callable fn` | `UFUNCTION(BlueprintCallable)` |
| `@blueprint_event fn` | `UFUNCTION(BlueprintNativeEvent)` + `_Implementation` |
| `@async_task struct` | `FRunnable` + `FRunnableThread` + game-thread callback |
| `@state_machine struct` | State enum + state class hierarchy + `FStateMachine` |

### Post-Processing Fixes (5 passes)

After initial codegen, 5 fix passes clean the output:

| Fix | Purpose |
|---|---|
| `ReplicationFix` | Injects missing `GetLifetimeReplicatedProps` + `DOREPLIFETIME` |
| `ShaderInitFix` | Injects shader resource init in `BeginPlay` |
| `ForwardDeclFix` | Missing forward declarations in correct header order |
| `IncludeOrderFix` | `CoreMinimal.h` first, then Engine, then Project |
| `FormattingFix` | Tabs, single blank lines, LF line endings |

---

## Module Dependency Resolver (`ue5/module_graph.rs`)

`DependencyResolver` auto-detects required UE5 build modules from generated includes:

- `RenderCore` / `RHI` / `Renderer` — shader crates
- `Slate` / `SlateCore` — widget crates
- `PropertyEditor` — details panel
- `UnrealEd` / `AssetTools` / `AssetRegistry` — editor tools
- `GameplayAbilities` — GAS
- `AIModule` — AI subsystems

Used to auto-populate `Build.cs` `PublicDependencyModuleNames`.
