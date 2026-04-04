# 04 Editor UI And Tools

The `ue5-editor` lane is one of the strongest differentiators in the current Kain UE5 stack.

It covers:

- Slate widgets
- Details panel customizations
- viewports
- toolbars
- asset editors
- editor modules
- reactive editor bindings

## Slate Widgets

Kain can author editor-facing UI that lowers to Slate C++.

Key authored entry points:

- `@slate struct Name`
- `@panel struct Name`

Supported generated widget families include:

- text
- buttons
- checkboxes
- spin boxes
- list views
- tree views
- tabs
- splitters
- boxes and layout containers
- overlays
- borders
- scroll boxes
- images
- combo boxes
- editable text

Example:

```kain
@slate
struct TestDashboard:
    title: String = "Test Dashboard"

    fn Compose() -> Widget:
        return VerticalBox()
            .Add(
                TextBlock()
                    .Text(title)
            )
```

## Details Panels

Use `@details` to author editor property experiences without hand-writing `IDetailCustomization`.

```kain
@details
struct CharacterDetails:
    @category("Stats")
    @slider(0.0, 100.0)
    health: Float = 100.0

    @color_picker
    team_color: Color = color("red")
```

Current generated patterns include:

- `IPropertyHandle`
- spin boxes with min and max ranges
- color pickers
- custom row generation
- lambda-based property updates

## Viewports

Use `@viewport struct Name` when you want a custom editor viewport surface.

Current support includes:

- `SEditorViewport`
- `FEditorViewportClient`
- preview scene wiring
- optional authored camera and scene patterns

## Toolbars And Editor Modules

The editor module lane supports:

- `@editor_module`
- `@menu_entry`
- `@toolbar_button`
- `@toolbar_widget`

These lower to normal Unreal editor module patterns such as:

- `IModuleInterface`
- startup and shutdown hooks
- Level Editor menu registration
- toolbar extension hooks

## Asset Editors

Use `@asset_editor struct Name` when you want a full custom asset editor workflow.

Current output includes:

- `FAssetEditorToolkit`
- tab registration
- viewport/details layout wiring
- docking layout setup

## Reactive Bindings

The editor runtime also supports generated reactive attribute bindings for dynamic editor state.

That includes patterns like:

- enabled state bindings
- visibility bindings
- live property-driven Slate updates

## Recommended Authoring Split

For serious editor plugins, separate files by responsibility:

```text
src/editor/
├── editor_module.kn
├── slate.kn
├── details.kn
├── viewport.kn
└── asset_editor.kn
```

## Best Example Sources

If you want real examples, inspect:

- `unreal_plugins/Example_Slate`
- `unreal_plugins/Example_Comprehensive`
- `unreal_plugins/Cinema4DMograph`
- `unreal_plugins/TemporalBlueprint`
- `unreal_plugins/MetaFitter`
