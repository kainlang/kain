# 08 Examples Feature Matrix And Limits

This document is the practical truth layer for the UE5 docs set.

It answers:

- which examples prove what
- which features are strong today
- which features are partial or staged
- which limits you should know before promising the moon

## Best Example Plugins

### `Example_Comprehensive`

Best all-around feature tour.

Shows:

- enums
- structs
- datatables
- delegates
- components
- subsystems
- actors
- RPCs
- Blueprint functions
- shaders
- Slate widgets
- details panels
- async tasks
- state machines

### `Example_Blueprint`

Best for Blueprint-focused generation and related authoring surfaces.

### `Example_Material`

Best for material graph authoring and material asset generation.

### `Example_Shader`

Best for focused shader authoring.

### `Example_Slate`

Best for editor UI authoring patterns.

### `Example_Graph`

Best for graph editor and graph runtime authoring.

### `Example_GAS`

Best for understanding the GAS surface, with an important caveat:

- it demonstrates the GAS language shape well
- not every GAS phase is fully wired into the main CLI build path yet

### `FluidFlow`

Best stress test for the shader-heavy, systems-heavy, domain-heavy plugin story.

## Current Feature Matrix

| Area | Status | Notes |
|---|---|---|
| Actors, components, structs, enums | Strong | Core runtime UE5 lane |
| Subsystems | Strong | Supported in main backend |
| Replication and RPC patterns | Strong with caveats | Validation exists; some misuse cases still matter |
| Blueprint-callable and Blueprint-event functions | Strong | Core UE5 backend support |
| Blueprint asset generation | Strong | Dual-path blueprint lane |
| Slate and details panels | Strong | Editor lane is substantial |
| Editor modules, toolbars, asset editors, viewports | Strong | Real editor tooling support |
| Shader generation | Strong | Major differentiator |
| Material graph generation | Strong | Material asset lane exists |
| Graph editor and graph runtime | Strong | `ue5-graphs` is production-ready |
| Config / developer settings | Strong | Good plugin fit |
| GAS tags and attribute sets | Strong | Production phases |
| Full GAS abilities/effects/cues/tasks | Partial | IR and codegen exist, CLI wiring still staged |
| Import lanes | Advanced | Useful, but not the product headline |
| Injection into existing plugins | Strong | Very helpful adoption path |

## Current Known Limits

### Full UE5 Validation Can Be Environment-Blocked

Some prior pipeline work reports partial blockage from Unreal-side file locks, even when Kain codegen itself succeeded.

### Name Collision Handling Still Has Improvement Room

Engine type collisions are validated, but automatic prefixing and collision smoothing can still improve.

### Struct Literal And Loop Ergonomics Still Want Growth

The plugin compilation work repeatedly called out:

- lack of native `for` loop comfort
- lack of ergonomic struct literals

### Shader Discovery In Large Plugins May Still Need Care

At least one large proof plugin documents manual shader registration friction in complex projects.

### GAS Is Not Uniformly Mature Across All Phases

Safe current sales language:

- GameplayTags and AttributeSets are solid
- broader authored GAS support exists but some phases remain partially integrated

## Bottom Line

The UE5 pipeline is broad enough to document and sell as its own product.

The honest framing is:

- production-strength core UE5 codegen
- unusually broad plugin and editor coverage
- several advanced lanes already implemented
- a few staged or rough edges that are visible and manageable
