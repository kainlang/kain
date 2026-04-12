# Unreal Plugins

`unreal_plugins/` is the UE5 example and reference surface. It is the best place to see how Kain turns into real plugin-shaped outputs, validation lanes, and engine-facing integration.

## How To Read It

Start with the official docs folder, then move into the concrete plugin families that match the surface you are trying to understand. The plugin-local docs and source are usually more specific than the umbrella README, so prefer those when they exist.

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

- UE5 plugin materialization and injection workflows
- shader, material, and graph lowering into UE5-friendly outputs
- editor and Slate integration for tool-facing surfaces
- gameplay-system item mapping and engine-facing runtime bridges
- low-level ABI and runtime alignment against the current C runtime contract
- config, platform, and packaging behavior that has to survive real engine integration

## Reader Path

If you are learning the UE5 lane, read the current UE5 guide pages first, then use these plugin families to verify the generated files, engine module layout, and validation output. If you are debugging a specific plugin family, go straight to that folder's local README or source before coming back to the umbrella docs.

## Documentation Pattern

Many plugin folders include their own docs or Kain source. Use the local docs first when a plugin has them; they usually explain the specific lane better than the umbrella README can.
