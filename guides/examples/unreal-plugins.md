# Unreal Plugins

`unreal_plugins/` is the UE5 example and reference surface.

## Notable Plugin Families

| Family | Examples |
| --- | --- |
| Shader and material | `Example_Shader`, `Example_Material`, `ToonShaderz`, `MetaHumanVAT`, `UESculpt`, `UPaint` |
| Graph and editor | `Example_Graph`, `NarrativeGraph`, `TitanGraph`, `TemporalBlueprint`, `VoxelForgePro` |
| Gameplay Ability System | `Example_GAS`, `TacticalRaidGAS`, `CrowdFlowDirector` |
| Slate / UI | `Example_Slate`, `OmniCam`, `Cinema4DMograph` |
| Low-level runtime | `LowLevelAbiSmoke`, `LowLevelRuntimeSmoke`, `SM64SubsetSmoke` |
| Config / platform | `ConfigSmokeTest`, `FluidFlow`, `MetaFitter` |
| Legacy archive | `_Archive/` |

## What These Examples Prove

- UE5 plugin materialization
- shader and material lowering
- graph and editor integration
- gameplay-system item mapping
- low-level ABI/runtime alignment

## Documentation Pattern

Many plugin folders include their own docs or Kain source. Use the local docs
first when a plugin has them; they usually explain the specific lane better than
the umbrella README can.
