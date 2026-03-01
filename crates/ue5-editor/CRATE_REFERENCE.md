# ue5-editor — UE5 Editor UI Codegen Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — Slate, Details, Viewport, Toolbar, Asset Editors, Editor Modules all implemented. slate.rs is the largest file at 120KB.

---

## Purpose

Generates UE5 editor extension C++ from KAIN editor constructs. Covers the full range of UE5 editor customization from simple Details panel customizations to full custom Asset Editors with toolbars and viewports.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `editor/slate.rs` | 120KB | `SCompoundWidget`, `SPanel`, all Slate widget codegen |
| `editor/details.rs` | 48KB | `IDetailCustomization` + `IPropertyHandle` binding |
| `editor/codegen.rs` | 55KB | Main editor orchestrator |
| `editor/assets.rs` | 16KB | Asset editor factory + toolkit |
| `editor/asset_editor_ir.rs` | 22KB | Asset editor IR |
| `editor/editor_module_codegen.rs` | 21KB | `IModuleInterface` + menus/toolbars |
| `editor/editor_module_ir.rs` | 9.2KB | Editor module IR |
| `editor/viewport.rs` | 12KB | `SEditorViewport` + viewport client |
| `editor/reactive.rs` | 12KB | Reactive binding helpers for Properties |
| `editor/style.rs` | 14KB | `FSlateStyleSet` / `FAppStyle` codegen |
| `data_asset_writer.rs` | 16KB | `UDataAsset` binary writer (shared with `ue5-asset-utils`) |

---

## Public API (`lib.rs`)

```rust
pub fn generate_editor(program: &TypedProgram) -> KainResult<GeneratedFiles>
```

---

## Slate Widgets (`editor/slate.rs`, 120KB)

### Supported Widget Types

| KAIN attribute | Generated Slate | C++ class |
|---|---|---|
| `@slate struct Name` | Custom widget | `SName : public SCompoundWidget` |
| `@panel struct Name` | Layout panel | `SName : public SPanel` |
| (auto from context) | Text block | `STextBlock` |
| (auto) | Button | `SButton` |
| (auto) | Check box | `SCheckBox` |
| (auto) | Spinner | `SSpinBox<float>` |
| (auto) | List view | `SListView<UObject*>` |
| (auto) | Tree view | `STreeView<UObject*>` |
| (auto) | Tab stack | `SDockTab` + `FTabManager` |
| (auto) | Splitter | `SSplitter` |
| (auto) | Box / HBox / VBox | `SBox` / `SHorizontalBox` / `SVerticalBox` |
| (auto) | Overlay | `SOverlay` |
| (auto) | Border | `SBorder` |
| (auto) | ScrollBox | `SScrollBox` |
| (auto) | Image | `SImage` |
| (auto) | ComboBox | `SComboBox<TSharedPtr<FString>>` |
| (auto) | Edit Text | `SEditableTextBox` |

### Slate Build Pattern

Generated code uses the chained fluent Slate API:
```cpp
SNew(SVerticalBox)
+ SVerticalBox::Slot()
  .AutoHeight()
  .Padding(4.0f)
[
    SNew(STextBlock)
    .Text(LOCTEXT("Label", "My Widget"))
]
```

### `SLATE_BEGIN_ARGS` Generation

For `@slate struct` types:
```cpp
SLATE_BEGIN_ARGS(SMyWidget) {}
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_EVENT(FSimpleDelegate, OnClicked)
SLATE_END_ARGS()
```

---

## Details Panel (`editor/details.rs`, 48KB)

| KAIN attribute | Generated Details |
|---|---|
| `@details struct Name` → field | `IPropertyHandle` + `SSpinBox` / `SColorBlock` |
| `@slider(min, max)` | `SSpinBox<float>` with `MinValue`/`MaxValue` |
| `@color_picker` | `SColorBlock` + color picker modal |
| `@button(label)` | `SButton` with click delegate |
| (object ref fields) | `SObjectPropertyEntryBox` |

Uses `GET_MEMBER_NAME_CHECKED` macro for type-safe property name binding. Auto-generates `Value_Lambda` and `OnValueChanged_Lambda` for each property binding.

```cpp
DetailBuilder.EditCategory("Combat")
    .AddCustomRow(LOCTEXT("Health", "Health"))
    .ValueContent()
    [
        SNew(SSpinBox<float>)
        .MinValue(0.0f)
        .MaxValue(1000.0f)
        .Value_Lambda([this]() { return GetHealthValue(); })
        .OnValueChanged_Lambda([this](float Val) { SetHealthValue(Val); })
    ];
```

---

## Viewport (`editor/viewport.rs`, 12KB)

`@viewport struct Name` generates:
- `SName : public SEditorViewport` — the Slate viewport widget
- `FNameViewportClient : public FEditorViewportClient` — viewport client with scene management
- Optional `@scene_actor` mesh objects added to preview scene
- Optional `@camera` for camera position/rotation setup

---

## Toolbar (`codegen.rs`)

| KAIN attribute | Generated toolbar item |
|---|---|
| `@button(label)` | `AddToolBarButton(FUIAction(...))` |
| `@toggle` | `AddToolBarButton` with IsChecked binding |
| `@separator` | `AddSeparator()` |
| `@dropdown` | `AddComboButton(...)` |

Uses `FToolBarBuilder` with `EToolBarPaletteOverride`.

---

## Asset Editor (`editor/assets.rs` + `asset_editor_ir.rs`)

`@asset_editor struct Name` generates a full `FAssetEditorToolkit`:
- `OpenAsset(UObject* InAsset)` override
- Tab spawner registration via `FTabManager::FLayout`
- Default docking layout combining Viewport + Details + Toolbar
- `RegisterTabSpawners` / `UnregisterTabSpawners`

---

## Editor Module (`editor/editor_module_codegen.rs`, 21KB)

`@editor_module struct Name` generates `IModuleInterface`:
```cpp
class FNameEditorModule : public IModuleInterface {
    void StartupModule() override;
    void ShutdownModule() override;
};
IMPLEMENT_MODULE(FNameEditorModule, Name)
```

Supports:
- `@menu_entry` — adds items to Level Editor menus
- `@toolbar_button` — adds buttons to Level Editor toolbar
- `FTSTicker` registration for periodic editor tick callbacks

---

## Reactive Bindings (`editor/reactive.rs`)

Generates attribute bindings for live data:
```cpp
.IsEnabled(TAttribute<bool>::Create([this]() { return bIsEnabled; }))
.Visibility(TAttribute<EVisibility>::Create([this]() { ... }))
```

Used in Slate widgets and Details panels for data that changes at runtime.
