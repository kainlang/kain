# OmniCam - The Global Camera Matrix

**Version:** 1.0.0  
**Price:** $24.99  
**Category:** Editor Tools  
**Engine:** Unreal Engine 5.4+

---

## Overview

OmniCam is a comprehensive camera management system for Unreal Engine 5 that solves the pain of managing dozens of cameras across large levels. No more hunting through the World Outliner or possessing cameras one by one to check their views.

### Key Features

✅ **Auto-Index All Cameras** - Automatically discovers all CineCameraActors and standard cameras in your level  
✅ **Live Picture-in-Picture** - Preview any camera's view without possessing it  
✅ **Bulk Property Editing** - Modify focal length, aperture, focus distance, and sensor size across multiple cameras at once  
✅ **Smart Search & Filtering** - Find cameras by name, type, location, or FOV  
✅ **Dockable Tab** - Persistent editor window that stays open across sessions  
✅ **Frustum Visualization** - Toggle camera frustums and name labels in the viewport  
✅ **Real-Time Updates** - Changes to cameras are reflected immediately in the preview

---

## Target Users

- **Cinematic Artists** - Managing complex camera sequences with 30+ cameras
- **ArchViz Developers** - Setting up multiple camera angles for presentations
- **Level Designers** - Quickly reviewing and adjusting camera placements
- **Technical Artists** - Bulk-editing camera properties for consistency

---

## Installation

1. Copy the `OmniCam` folder to your project's `Plugins` directory
2. Right-click your `.uproject` file and select **Generate Visual Studio project files**
3. Open the solution in Visual Studio
4. Build the project (**Development Editor** configuration)
5. Launch Unreal Editor
6. Enable the **OmniCam** plugin in **Edit → Plugins**
7. Restart the editor

---

## Usage

### Opening the Camera Manager

**Method 1:** Menu bar → **Tools → OmniCam → Open Camera Manager**  
**Method 2:** Toolbar → Click the camera icon in the Content section  
**Method 3:** Command palette → Search for "OmniCam"

### Camera List Panel (Left)

- **Search Bar** - Filter cameras by name
- **Sort Dropdown** - Sort by Name, Type, Location, or FOV
- **Camera List** - Click to select, Ctrl+Click for multi-select
- **Right-Click Menu** - Quick actions (Focus, Isolate, Delete)

### Preview Panel (Right)

- **Live PiP Viewport** - Real-time preview of selected camera
- **Quality Dropdown** - Low/Medium/High/Ultra rendering quality
- **Camera Info** - Name, type, location, rotation, FOV

### Bulk Edit Panel (Bottom)

- **Focal Length Slider** - 10mm to 200mm
- **Aperture Slider** - f/1.4 to f/22
- **Focus Distance Slider** - 0cm to 10000cm
- **Sensor Width Slider** - 12mm to 36mm
- **Debug Color Picker** - Set frustum visualization color
- **Apply to Selected** - Apply changes to all selected cameras
- **Reset to Defaults** - Reset selected cameras to default values

### Toolbar Actions

- **Refresh Index** - Rescan level for cameras
- **Select All** - Select all cameras in the list
- **Deselect All** - Clear selection
- **Focus Selected** - Frame selected camera in main viewport
- **Show Frustums** - Toggle frustum visualization
- **Show Names** - Toggle camera name labels
- **Auto-Update** - Automatically refresh camera list when cameras are added/removed

---

## Blueprint Functions

OmniCam exposes several Blueprint-callable functions for automation:

### `GetAllCamerasInLevel() -> Array<CameraInfo>`
Returns an array of all cameras in the current level.

### `SelectCameraByName(name: String) -> CameraInfo`
Finds and returns a camera by name.

### `BulkSetFocalLength(cameras: Array<CameraInfo>, focal_length: Float) -> Bool`
Sets the focal length for multiple cameras at once.

### `BulkSetAperture(cameras: Array<CameraInfo>, aperture: Float) -> Bool`
Sets the aperture for multiple cameras at once.

### `FocusCameraInViewport(camera: CameraInfo) -> Bool`
Frames the specified camera in the main viewport.

---

## Performance

- **Lightweight** - Minimal runtime overhead, editor-only plugin
- **Efficient Indexing** - Camera list updates only when needed
- **Optimized PiP** - Uses `SceneCaptureComponent2D` with quality presets
- **Scalable** - Tested with 100+ cameras in a single level

---

## Troubleshooting

### Camera list is empty
- Click **Refresh Index** in the toolbar
- Ensure you have cameras in the level (CineCameraActor or CameraActor)
- Check that **Auto-Update** is enabled

### PiP preview is black
- Select a camera from the list
- Increase **Quality** setting in the preview panel
- Ensure the camera has a valid view (not inside geometry)

### Bulk edit not working
- Ensure cameras are selected in the list (highlighted)
- Click **Apply to Selected** after adjusting sliders
- Check that cameras are not locked in the World Outliner

---

## Support

For bug reports, feature requests, or support:
- Email: support@kainfactory.com
- Discord: [KAIN Factory Community](https://discord.gg/kainfactory)
- Documentation: [docs.kainfactory.com/omnicam](https://docs.kainfactory.com/omnicam)

---

## Changelog

### Version 1.0.0 (Initial Release)
- Auto-indexing of all cameras in level
- Live PiP preview viewport
- Bulk property editing (focal length, aperture, focus, sensor)
- Search and filtering
- Frustum visualization
- Dockable editor tab
- Blueprint function library

---

## License

This plugin is licensed for use in Unreal Engine projects only. Redistribution of source code or compiled binaries is prohibited without written permission from KAIN Factory.

---

**Made with ❤️ by KAIN Factory**
