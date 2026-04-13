# Atlas Backend Boundary Matrix

## Purpose

This matrix defines what belongs to compiler-owned semantics, what belongs to the shared runtime, and what each backend adapter may customize without becoming the source of truth.

## Boundary Matrix

| Concern | Compiler-owned contract | `kain-ui` runtime ownership | Backend adapter allowance | Backend adapter must not do | Current leak to remove |
| --- | --- | --- | --- | --- | --- |
| Widget identity | Semantic widget family, schema id, authored props | Node identity, patch targeting | Map semantic widget to egui/DOM/Slate realization | Invent new product widgets as the only truth | Native host still changes behavior by host mode for inspector/tree |
| Event routing | Typed event routes and handler targets | Dispatch routing, transaction application | Convert host input to semantic event route invocations | Keep event meaning as renderer-local callbacks | `kain-core` still serializes event props as `[event:name]` strings |
| Commands | Command ids, intent, payload schema, authored surfaces | Command buffer and execution lifecycle | Render menus, toolbars, palettes, shortcuts | Invent backend-only product commands | Driver runtime snapshot injects `Reload Runtime` as default shell command |
| Focus and selection | Scope declarations and authored defaults | Active scope state, transfer on reload, invalidation | Map host focus/selection to semantic scopes | Keep hidden backend-only selection truth | Current runtime mostly derives scopes from node fields or previous reload state |
| Docking and tabs | Dock intent, persistent ids, tab grouping semantics | Layout state, snapshot/apply, active tab state | Use native docking primitives if available | Replace semantic tab/dock model with backend-local layout model | Native projection and host behavior are still carrying convenience logic |
| `UiNativeProjection` (compatibility) | None (explicitly non-canonical) | Optional compatibility generation only | Native adapter may use it to bridge legacy consumers | Treat it as the semantic IR for web/Slate/UE or use it to invent meaning | `UiRuntimeBundle` embeds `native_projection` and parity fixtures lock its serialized tags for non-Rust consumers |
| Paint system | Semantic paints, gradients, masks, images, blur, layers | Resolved paint values, animation playback state | Translate paint semantics into renderer-specific draw calls | Require smoke-local rendering hacks for authored visuals | Native host currently resolves many visuals from theme lookup without a richer compiler contract |
| Motion | Authored tracks, triggers, easing, transition semantics | Playback state, scheduler integration, patch timing | Use backend animation APIs or manual interpolation | Hide motion state entirely inside backend widgets | Runtime currently seeds generic mount animations from surface inference |
| Surfaces and viewports | Surface ids, shader refs, scene refs, composition intent | Surface registry, capability negotiation, fallback selection | Realize via egui/wgpu/Slate/custom viewport APIs | Invent surface meaning independent of bundle truth | Native host already has a strong renderer path, but contract depth is still uneven |
| Realtime render surface bundle (`RealtimeAppBundle`) | Realtime surface and scene requirements, stable surface ids | Runtime provides surface registry and capability negotiation | Backend loads it to configure viewports/materials/shader canvases | Use it as a dumping ground for UI commands/state/chrome | Schema depends on `output.systems.surfaces` (often inferred) plus prop scanning, so identity and requirements are not yet fully compiler-owned truth |
| Capability and fallback | Shared capability ids and fallback categories | Capability resolution and unsupported-state reporting | Publish backend capability table and realize declared fallback | Silently drop unsupported semantics | Current backend fallbacks are implicit and scattered |
| Devtools | Separate opt-in devtools contract | Runtime inspection data, patch log, diagnostics exposure | Render inspector windows or panels when enabled | Ship devtools surfaces as default product chrome | Native topbar and inspector still live in the default shell path |
| Packaging metadata | Bundle paths, runtime capabilities, sidecar references | None beyond runtime load hooks | Read packaged sidecars | Invent app-shell structure or runtime workspace identity | Driver emits synthetic runtime snapshot panels/providers/sessions that backends treat as product meaning |
| Native runtime snapshot sidecar (devtools only) | None for product mode | Runtime may expose devtools channels when enabled | Backend can render opt-in runtime/devtools UI using it | Use it to decide product posture, default chrome, or app identity | `build_native_app_runtime_snapshot` injects default panels/commands/providers/sessions/workspaces into every native app today |
| Slate / UE specifics | None beyond backend-neutral semantic ids | None beyond generic runtime graph state | Use `crates/ue5` metadata for Slate mapping, editor attributes, and module requirements | Push UE-only widget semantics back into compiler authoring | UE metadata exists today but is not yet wired as an adapter consumer |

## Backend-Specific Notes

### `kain-ui-native`

- Allowed:
- egui/wgpu rendering choices
- native windowing
- viewport execution details
- capability publication
- opt-in devtools surfaces

- Not allowed:
- default topbar or inspector in product mode (today `show_runtime_topbar` defaults visible)
- root/component badges in authored shells
- host-generated product command surfaces

### future `kain-ui-web`

- Allowed:
- DOM, canvas, and WebGPU realization
- accessibility tree mapping
- browser event plumbing

- Not allowed:
- browser-specific semantics leaking back into authoring contracts

### future `kain-ui-slate`

- Allowed:
- Slate widget mapping
- UE editor docking integration
- use of `WidgetRegistry` and `EditorAttributesRegistry`

- Not allowed:
- redefining semantic widget families around Slate class names
- treating extracted UE metadata as the UI source of truth

## Adapter Readiness Requirement

Any backend is considered ready only when it can consume:

- the same semantic widget identities
- the same event and command contracts
- the same docking/tab semantics
- the same surface and capability model

without requiring a backend-specific rewrite of authored UI meaning.
