Kaintana vs egui vs dear imgui — Complete Feature Assessment
What Kaintana Already Does Well (Strengths)
Unique Kain-level advantages (no other GUI framework has these):
- World + Entangle reactivity — KaintanaReactivity.signal <-> KaintanaReactivityMirror.signal_copy with resonate gives you declarative state propagation across worlds/graphs. egui and dear imgui have no equivalent — they'd need a separate reactive data layer.
- Resonate (dampened event stream) — auto-updates layout_revision on signal changes. This is compile-time observable reactivity, not a runtime observer pattern.
- Patch (transactional mutation) — guaranteed side-effect enforcement.
- Axiom (capability gating) — compile-time when target("llvm") and when capability("ui.retained") gates the entire framework surface. egui/dear imgui can't do this at compile time.
- Component JSX — component KaintanaReactivityPanel(): render <panel title="..." /> — compile-time verified templates.
- Defer-based cleanup — defer kaintana_begin_frame_cleanup(session_id) — guaranteed finalization in Kain's effect system.
- Builder pattern + trait-polymorphic build_* functions — build_button<T: KaintanaRenderable>(ctx, builder) where T: KaintanaUiCompatible is a level of generic UI composition neither egui nor dear imgui provides.
- Hot-reload integration — kaintana_begin_frame(session_id, revision_key, delta_ms) with reload_begin/commit. This is first-class in the framework, not a library add-on.
Production features Kaintana already has:
- IME (text input method editor) for CJK input
- Clipboard (copy/paste)
- Keyboard action binding (declarative key→action map)
- Agent intent injection (push events from AI agent)
- Dialog system (request/respond/poll)
- Menu system (create, add items, open at position)
- Popover system
- Focus management (focus, focused node, focus ring rendering)
- Screenshot + report writing (frame reports, host reports, BMP capture)
- Snapshot + input trace (regression testing harness)
- Retained + immediate widget styles (both reconciliation modes)
- 4 color themes (solar-broadcast, marine-terminal, kawaii-voltage, oxide-dcc)
- Layered arch: platform adapter → reconciliation engine → render commands → widget API
- 3 platform backends: Desktop (GDI/GDI+), Winit, Vulkan
Major Gaps (vs egui and dear imgui)
These are ranked by production impact — what you'd need to build real applications:
Critical (users will notice immediately):
Gap	Kaintana	egui	dear imgui
No auto-layout	All positions/sizes manual (kaintana_rect(x, y, w, h) everywhere)	ui.horizontal(), ui.vertical(), Layout::top_down(Align::Center)	Auto vertical stacking, SameLine(), BeginTable
No scroll container	No ScrollArea — if content > rect, it clips	ScrollArea (animated, auto-expand)	BeginChild with scroll flags, SetScrollHereY
No collapsing header	Can't collapse a section	CollapsingHeader	CollapsingHeader, TreeNode
No combo box / dropdown	Must build from scratch	ComboBox	BeginCombo/EndCombo
No tooltip	No hover tooltips	Tooltip container	BeginTooltip/EndTooltip, SetItemTooltip
No table/grid data view	Only single bar chart	Grid (auto-column-width)	BeginTable/EndTable (sort, resize, reorder, freeze, hide, stretch, row bg, context menu)
No tree / outline	No hierarchical data browser	No built-in	TreeNode, TreeNodeEx, TreePush/TreePop
No tab bar	Can't do tabbed interfaces	No built-in	BeginTabBar/EndTabBar (reorderable, scrollable, closeable)
No progress bar / spinner	No loading indicators	ProgressBar, Spinner	ProgressBar
Important (complex apps need these):
Gap	Impact
No window management — no move/drag/resize/minimize/close on panels	Can't build multi-document apps
No resize handles / splitters	Panels are fixed size after layout
No drag & drop	Can't reorder lists, drag files
No image widget	Can't display bitmaps/textures
No radio button	Single-select patterns require manual impl
No color picker	No color editing UI
No drag value / spinner	No numeric increment/decrement
No menu bar (only programmatic menu)	No F10/menu-bar UX
No text selection in labels/text	Can't select/copy displayed text
No style editor	Can't tweak theme at runtime
No multi-window	Single native window only (per session)
Rendering:
Gap	Kaintana	egui	dear imgui
Draw primitives	Only fill rect + text	Lines, circles, arcs, bezier, triangles, polygons, images, rounded rects, gradients, frames, 9-slice	ImDrawList AddRect, AddCircle, AddLine, AddText, AddImage, AddTriangle, AddBezierCubic, AddNgon, AddEllipse, AddRectFilledMultiColor (gradient), PathStroke, PathFillConvex, etc.
Image rendering	None	Image, ImageButton with ImageSource, ImageFit, animated GIF/WebP	Image, ImageButton
Rounded rects	None (only sharp rects)	CornerRadius	DrawList::AddRectRounded, DrawList::AddNgon
Anti-aliasing	None at Kaintana level	Via epaint tessellation	style.AntiAliasedLines, style.AntiAliasedFill
Gradients	None	Stroke with gradient?	AddRectFilledMultiColor (horizontal/vertical gradients)
Animation:
Gap	Kaintana	egui	dear imgui
Animation system	None	animate_bool(), animate_value(), animate_continuous(), easing functions	None built-in (manual GetTime())
Smooth transitions	None	Boolean→float, value lerp, position animation	Manual
Collapse animation	N/A (no collapse widget)	Vertical collapse animation	None
Accessibility:
Gap	Kaintana	egui	dear imgui
Screen reader	None	AccessKit integration (native Windows/macOS)	Explicitly not supported
Keyboard nav	Basic focus + action binding	FocusDirection directional nav	Full keyboard/gamepad nav system with scoring
Where Kaintana Is Actually Ahead (vs egui & dear imgui)
What	Kaintana's advantage
Reactivity	world/entangle/resonate — compiler-owned state graph with observable propagation. egui/dear imgui have zero built-in reactivity.
Hot reload	reload_begin/reload_commit at the framework level — real-time code reload is a first-class feature, not a hacked-on debug tool
Agent intent injection	kaintana_action_push_agent_intent — AI agent can directly drive UI events. This is forward-looking in a way no other GUI framework has.
Builder pattern + generics	build_button<T: KaintanaRenderable>(ctx, builder) where T: KaintanaUiCompatible — type-safe polymorphic widget construction. egui has a Widget trait but it's simpler.
Stable-key reconciliation	Kaintana's kaintana_reconcile_node matches nodes by stable key across frames — this is closer to React's keyed reconciliation than either egui or dear imgui's ID model
Compile-time capability gating	axiom gates compile the framework for specific targets — eliminates dead code for unsupported platforms at compile time
Hot-reload harness	kaintana_write_harness_artifacts + snapshot/input-trace is built for testing with regression captures.
Shape verification	command_checksum + draw_count gives you a compile-time "did the right things render" proof
C FFI bridge	Direct GDI/GDI+ backend via companion .c file — you can write OS-level rendering that's not stuck behind a graphics API abstraction
Capsule / amalgamation	kaintana-capsule produces a single-file portable bundle. egui needs full Rust toolchain. dear imgui needs a C++ compiler.
The Verdict
Kaintana is NOT "god mode UI" yet — but it has a fundamentally better foundation.
What makes it special isn't the widget count (which is ~12 vs egui's 35+ vs dear imgui's 50+). It's the architecture:
egui and dear imgui are render-time frameworks — they draw pixels, you handle state manually. Kaintana is a compile-time reactive framework — world/entangle/patch/resonate/axiom/component give you guarantees about state propagation and capability that no render-time framework can match.
The gap is in widgets, layout, and render primitives — not architecture.
Priority Order for "God Mode" (what to add next)
Tier 1 — Surface-level (high impact, moderate effort):
1. Scroll container — std::ui probably already supports scroll areas; just wrap it
2. Tooltip — trivial hover state → popup text
3. Collapsing header / accordion — essential for inspector panels
4. Combo box / dropdown — kaintana_immediate_combo() using menu system
5. Progress bar — simple fill rect with ratio
Tier 2 — Layout (enables everything else):
6. Auto-layout group — kaintana_begin_horizontal() / kaintana_begin_vertical() with auto-sizing children (removes manual rect positioning for most cases)
7. Resizable splitter — interactive panel divider (egui Resize, dear imgui ImGuiChildFlags_ResizeX)
Tier 3 — Data views (for tools/apps):
8. Table widget — column headers, sort, virtual scroll
9. Tab bar — tabbed document interface
10. Tree view — hierarchical data browser
Tier 4 — Polish:
11. Rounded rect render primitive — kaintana_primitive_rounded_rect
12. Image widget — load + display bitmap/texture resources
13. Animation helpers — animate_bool, animate_value with easing
14. Style editor / theme customization — per-widget color overrides
Tier 5 — Bold (Kain-unique):
15. World-driven layout — use entangle to propagate layout constraints across worlds (egui/dear imgui can't do this)
16. Converge lanes for render backends — pick GDI vs Vulkan vs software at runtime via CPUID checking
17. Component-based widget library — <Button label="..." /> JSX syntax for all widgets, not just panels