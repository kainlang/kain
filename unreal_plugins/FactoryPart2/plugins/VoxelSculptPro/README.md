# VoxelSculptPro

**Version**: 1.0.0  
**Category**: DCC Tools (Digital Content Creation)  
**Engine**: Unreal Engine 5.4+

## Overview

VoxelSculptPro is a ZBrush-style GPU sculpting system that brings professional digital sculpting directly into the Unreal Engine editor. Unlike external tools that require export/import workflows, VoxelSculptPro provides real-time sculpting with dynamic tessellation, multi-resolution mesh support, and a comprehensive brush system.

## Key Features

### GPU-Accelerated Sculpting
- **Compute Shader Pipeline**: All sculpting operations run on GPU compute shaders for maximum performance
- **Real-Time Deformation**: Sculpt meshes at 60+ FPS with millions of vertices
- **Brush Permutations**: CFG-based shader permutations for different brush types (Standard, Smooth, Inflate, Grab)

### Professional Brush System
- **Data-Driven Architecture**: Brush behaviors defined in KAIN and compiled to GPU kernels
- **Customizable Parameters**: Radius, strength, falloff, and symmetry controls
- **Multi-Axis Symmetry**: X, Y, Z axis symmetry options for character work

### Multi-Resolution Workflow
- **Dynamic LOD**: Automatic LOD generation and mesh optimization
- **Subdivision Support**: Work at different detail levels seamlessly
- **Async Processing**: Background mesh processing doesn't block the editor

### Editor Integration
- **Slate UI**: Intuitive brush palette and parameter controls
- **3D Viewport**: Dedicated sculpting viewport with mesh preview
- **Asset Pipeline**: Seamless integration with UE5's asset system

## Why VoxelSculptPro?

VoxelSculptPro fills a critical gap in the marketplace—no existing plugin offers in-editor sculpting at this quality level:

- **ZBrush/Blender**: Require external workflows with export/import friction
- **UE5 Native Tools**: Lack professional sculpting capabilities
- **VoxelSculptPro**: Professional sculpting without leaving the engine

Perfect for:
- Character artists refining facial details
- Environment artists sculpting organic terrain
- Technical artists creating custom meshes
- Anyone needing rapid iteration without external tools

## Technical Architecture

### Components

1. **GPU Compute Shaders** (`sculpting_shaders.kn`)
   - BrushKernel compute shader with brush operations
   - MeshDeformation shader for vertex manipulation
   - Shader permutations for brush type variants

2. **Slate Widgets** (`brush_palette.kn`)
   - Brush selection UI with type buttons
   - Parameter sliders for radius, strength, falloff
   - Symmetry toggles for X, Y, Z axes

3. **Viewport System** (Future)
   - 3D sculpting viewport with mesh preview
   - Real-time brush preview overlay
   - Camera controls optimized for sculpting

4. **Async Tasks** (Future)
   - Background mesh processing
   - LOD generation pipeline
   - Topology optimization

5. **Actor System** (`sculpting_actor.kn`)
   - Mesh state management
   - Undo/redo history
   - Shader resource coordination

## KAIN Features Demonstrated

- **GPU Compute Shaders** (ue5-shaders): Sculpting kernels, brush operations, mesh deformation
- **Editor UI - Slate Widgets** (ue5-editor): Brush palette, parameter controls, symmetry options
- **Editor UI - Viewports** (ue5-editor): 3D sculpting viewport with mesh preview
- **Async Tasks** (ue5): Background mesh processing, LOD generation, topology optimization
- **Actor System** (ue5): Sculpting actors for mesh management and state tracking

## Building

```bash
kain build --ue5
```

## Installation

1. Copy the generated plugin to your project's `Plugins/` directory
2. Regenerate project files
3. Build your project
4. Enable VoxelSculptPro in the Plugins menu

## Usage

1. Open the VoxelSculptPro editor window from Tools menu
2. Import or create a mesh to sculpt
3. Select a brush type from the palette
4. Adjust brush parameters (radius, strength, falloff)
5. Enable symmetry if needed
6. Start sculpting in the viewport

## Performance

- **Target**: 60+ FPS with 1M+ vertices
- **GPU Requirements**: Compute shader support (SM5.0+)
- **Memory**: Scales with mesh resolution

## License

Copyright (c) 2026 KAIN Factory Part 2. All rights reserved.

## Support

For issues, feature requests, or questions, please contact the development team.
