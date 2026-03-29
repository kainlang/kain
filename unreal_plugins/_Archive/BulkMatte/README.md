# BulkMatte - Material Instance Parameter Bulk Editor

**Version:** 1.0.0  
**Price:** $19.99  
**Target:** Technical artists, material artists, asset pack creators

## Overview

BulkMatte is a professional spreadsheet-style bulk editor for Unreal Engine 5 Material Instances. Edit 200+ materials simultaneously with a powerful grid interface, master controls, and CSV import/export.

## The Problem

Editing hundreds of Material Instances is tedious:
- Open each material individually
- Check parameter override boxes
- Change values one by one
- Save and repeat 200+ times
- Takes hours for simple adjustments

## The Solution

BulkMatte provides:
- **Spreadsheet View** - See all material parameters in one grid
- **Bulk Operations** - Edit multiple materials simultaneously
- **Master Controls** - Apply values to all selected materials
- **Smart Filtering** - Find materials by name, parent, or parameter
- **CSV Export/Import** - Share parameter sets between projects
- **Real-time Preview** - See changes instantly
- **Undo/Redo** - Safe bulk editing with full history

## Key Features

### 1. Material Scanning
- Scan entire project or specific folders
- Index all Material Instances automatically
- Filter by parent material
- Show only modified materials

### 2. Parameter Grid
- Spreadsheet-style parameter view
- Sort by name, type, value, or parent
- Show/hide overridden parameters
- Color-coded modified values
- Multi-select for bulk operations

### 3. Bulk Editing
- Set absolute values across materials
- Add/multiply relative adjustments
- Clamp values to safe ranges
- Reset to default values
- Apply master values to all

### 4. Master Controls
- Master Roughness slider (0-1)
- Master Metallic slider (0-1)
- Master Base Color picker
- Master Normal Intensity slider (0-2)
- One-click apply to all selected

### 5. CSV Import/Export
- Export parameters to CSV
- Import parameters from CSV
- Share presets between projects
- Batch process with external tools

### 6. Context Menu Integration
- Right-click folder → "Audit Materials"
- Quick access from Content Browser
- Scan and edit in one workflow

## Workflow Example

### Before BulkMatte (2 hours):
1. Open Material Instance 1
2. Check "Roughness" override box
3. Change value from 0.2 to 0.8
4. Save
5. Repeat 200 times
6. ☕☕☕ (many coffee breaks)

### With BulkMatte (30 seconds):
1. Right-click "Materials/Environment" folder
2. Select "Audit Materials"
3. See all 200 materials in grid
4. Type "0.8" in Master Roughness
5. Click "Apply to All"
6. Click "Save All"
7. ✅ Done!

## Use Cases

### Asset Pack Creation
- Standardize roughness across 500 materials
- Ensure consistent metallic values
- Batch adjust normal intensity
- Export parameter sets for documentation

### Project Cleanup
- Find materials with extreme values
- Reset unused overrides
- Standardize naming conventions
- Audit parameter usage

### Art Direction Changes
- "Make everything less glossy" → 30 seconds
- "Increase normal intensity" → 30 seconds
- "Adjust base colors" → 30 seconds
- "Reset all to defaults" → 30 seconds

### Cross-Project Sharing
- Export material presets to CSV
- Import presets into new project
- Share parameter sets with team
- Version control material settings

## Installation

1. Copy `BulkMatte` folder to `YourProject/Plugins/`
2. Right-click `.uproject` → "Generate Visual Studio project files"
3. Open solution in Visual Studio
4. Build (Development Editor)
5. Launch Unreal Editor
6. Enable BulkMatte in Edit → Plugins
7. Restart editor

## Usage

### Opening BulkMatte
- **Menu:** Tools → BulkMatte → Open Material Editor
- **Toolbar:** Click BulkMatte icon in Content toolbar
- **Context Menu:** Right-click folder → "Audit Materials"

### Scanning Materials
1. Click "Scan Materials" button
2. Select folders to scan (or scan entire project)
3. Wait for indexing to complete
4. Materials appear in grid

### Bulk Editing
1. Select materials in left panel (Ctrl+Click for multi-select)
2. Adjust master controls (Roughness, Metallic, etc.)
3. Click "Apply to All" or "Apply to Selected"
4. Changes apply instantly
5. Use Ctrl+Z to undo if needed

### Filtering
- **Search Box:** Type parameter name to filter
- **Filter Dropdown:** All / Modified / Unmodified / Overridden
- **Sort Dropdown:** Name / Type / Value / Parent

### CSV Export/Import
- **Export:** Select materials → Click "Export CSV" → Choose file location
- **Import:** Click "Import CSV" → Select file → Confirm changes

## Technical Details

### Supported Parameter Types
- Scalar (Float)
- Vector (Color/Vector3)
- Texture (Texture2D)
- Static Switch (Bool)

### Performance
- Scans 1000+ materials in seconds
- Real-time parameter updates
- Efficient undo/redo system
- Minimal memory footprint

### Compatibility
- Unreal Engine 5.4+
- Windows, Linux, Mac
- Works with any Material Instance
- Compatible with all material types

## Tips & Tricks

### Tip 1: Use Filters
Filter to "Modified Only" to see which materials have overrides. Great for cleanup!

### Tip 2: Master Controls
Set master values before scanning. New materials will use these defaults.

### Tip 3: CSV Presets
Create CSV presets for common material types (Metal, Wood, Plastic, etc.)

### Tip 4: Undo is Your Friend
Bulk operations are powerful but risky. Use Ctrl+Z liberally!

### Tip 5: Preview Before Apply
Select one material, adjust values, preview, then apply to all.

## Support

- **Documentation:** See TECHNICAL.md for implementation details
- **Issues:** Report bugs via support email
- **Updates:** Check for updates regularly

## License

Commercial use allowed. See LICENSE.txt for details.

## Credits

Built with KAIN - The LLM-first UE5 plugin compiler.
