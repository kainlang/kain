# FlexPartition - Smart Grid & Data Layer Automator

**Version:** 1.0.0  
**Price:** $29.99  
**Target:** Open-world developers, level designers, technical artists

## Overview

FlexPartition automates the tedious process of configuring UE5 World Partition settings. It analyzes your level actors, categorizes them by size, and automatically assigns them to optimal Data Layers with calculated grid cell sizes.

## Problem Solved

UE5 World Partition is powerful but requires manual configuration:
- Manually assigning thousands of actors to Data Layers
- Calculating optimal grid cell sizes
- Determining loading ranges
- Balancing performance vs quality

FlexPartition does this automatically in seconds.

## Features

### 1. Automatic Actor Analysis
- Scans all actors in your level
- Categorizes by size: Tiny, Small, Medium, Large, Massive
- Calculates bounding sphere radius
- Estimates memory footprint
- Counts triangles (optional)

### 2. Smart Grid Optimization
- Calculates optimal grid cell sizes based on actor density
- Three grid types: Distant, Main, Detail
- Configurable loading ranges
- Performance/Balanced/Quality presets

### 3. Data Layer Management
- Auto-creates Data Layers if missing
- Assigns actors to appropriate layers
- Bulk operations on thousands of actors
- Undo/redo support

### 4. Visual Dashboard
- Real-time statistics
- Actor distribution charts
- Grid layout preview (top-down view)
- Memory usage estimates

### 5. Optimization Presets
- **Performance:** Larger cells, aggressive culling
- **Balanced:** Medium cells, moderate culling
- **Quality:** Smaller cells, minimal culling
- **Custom:** Full manual control

## Quick Start

### Installation
1. Copy `FlexPartition` folder to `YourProject/Plugins/`
2. Restart Unreal Engine
3. Enable plugin in Edit → Plugins → FlexPartition

### Basic Usage
1. Open your World Partition level
2. Go to **Tools → FlexPartition → Open Dashboard**
3. Click **Analyze Level** to scan actors
4. Review statistics and distribution
5. Select optimization preset (Performance/Balanced/Quality)
6. Click **Apply Optimization**
7. Done! Your World Partition is optimized

### Advanced Usage
1. Open **Tools → FlexPartition → Analyze World Partition**
2. Adjust thresholds in Details panel:
   - Tiny: 0-1000 units
   - Small: 1000-5000 units
   - Medium: 5000-20000 units
   - Large: 20000-100000 units
   - Massive: >100000 units
3. Configure grid cell sizes:
   - Distant Grid: 50000-500000 units
   - Main Grid: 10000-100000 units
   - Detail Grid: 1000-20000 units
4. Set loading ranges for each grid type
5. Click **Preview Changes** to see before/after
6. Click **Apply Optimization** to commit
7. Use **Revert to Defaults** if needed

## Algorithm

```
1. Scan all actors in level
2. For each actor:
   - Calculate bounding sphere radius
   - Categorize by size
3. Assign to grids:
   - Massive → Distant Grid (never unloads)
   - Large → Main Grid (loads at 50000 units)
   - Medium → Main Grid (loads at 50000 units)
   - Small → Detail Grid (loads at 10000 units)
   - Tiny → Detail Grid (loads at 5000 units)
4. Create Data Layers if missing
5. Assign actors to layers
6. Update World Partition settings
```

## Optimization Presets

### Performance
- **Goal:** Maximum FPS, aggressive culling
- **Distant Grid:** 200000 units, loads at 500000
- **Main Grid:** 50000 units, loads at 100000
- **Detail Grid:** 10000 units, loads at 20000
- **Best for:** Large open worlds, lower-end hardware

### Balanced
- **Goal:** Balance between quality and performance
- **Distant Grid:** 150000 units, loads at 300000
- **Main Grid:** 30000 units, loads at 60000
- **Detail Grid:** 5000 units, loads at 10000
- **Best for:** Most projects, recommended default

### Quality
- **Goal:** Maximum detail, minimal culling
- **Distant Grid:** 100000 units, loads at 200000
- **Main Grid:** 20000 units, loads at 40000
- **Detail Grid:** 2000 units, loads at 5000
- **Best for:** High-end showcases, cinematics

## Use Cases

### Open World Games
- Automatically partition massive landscapes
- Optimize streaming for seamless exploration
- Balance detail vs performance

### Architectural Visualization
- Handle thousands of detailed props
- Optimize loading for walkthroughs
- Maintain visual quality

### Level Design Iteration
- Quickly test different grid configurations
- Analyze actor distribution
- Identify performance bottlenecks

## Tips & Best Practices

1. **Run analysis after major level changes** - Actor distribution changes as you build
2. **Start with Balanced preset** - Then tweak if needed
3. **Use Preview before Apply** - See changes before committing
4. **Check memory estimates** - Ensure you're within budget
5. **Test in-game** - Verify streaming performance

## Troubleshooting

**Q: Analysis shows 0 actors**  
A: Ensure you're in a World Partition level, not a regular level

**Q: Grid optimization doesn't apply**  
A: Check that World Partition is enabled in World Settings

**Q: Memory estimates seem wrong**  
A: Estimates are approximate, use UE5 profiler for exact numbers

**Q: Some actors not assigned**  
A: Check actor mobility (static vs dynamic) and World Partition settings

## Support

- Documentation: See TECHNICAL.md for implementation details
- Issues: Report bugs via GitHub/Marketplace
- Updates: Check for plugin updates regularly

## License

Commercial use allowed. See LICENSE.txt for details.

## Credits

Built with KAIN compiler - Python-like language for UE5 plugin development.
