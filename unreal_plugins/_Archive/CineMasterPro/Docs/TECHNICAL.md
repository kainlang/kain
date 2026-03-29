# CineMaster Pro - Technical Documentation

## Architecture Overview

CineMaster Pro is built using the KAIN language, which compiles to production-ready UE5 C++ code. The plugin consists of multiple interconnected systems that work together to provide professional virtual production capabilities.

## System Components

### 1. Virtual Camera System

**Actor:** `VirtualCamera`  
**Component:** `VirtualCameraComponent`  
**Shaders:** `LensSimulation`, `CinematicGrading`

The virtual camera system manages individual camera instances with full lens simulation and post-processing.

#### Key Features:
- **Networked Replication** - All camera state is replicated across clients
- **GPU Lens Simulation** - Compute shaders handle DOF, bokeh, aberrations
- **Per-Camera Post-Processing** - Individual color grading per camera
- **Performance Tracking** - Frame time monitoring for optimization

#### RPC Methods:
- `Server_SetActive(Bool)` - Activate/deactivate camera
- `Server_SetLive(Bool)` - Set camera as live feed
- `Server_SetLensParameters(Float, Float, Float)` - Update lens settings
- `Server_SetExposure(Float, Float, Float)` - Update exposure settings
- `Server_StartRecording()` - Begin recording from this camera
- `Server_StopRecording()` - Stop recording

#### Blueprint Functions:
- `GetCameraID() -> Int` - Get unique camera identifier
- `GetCameraName() -> String` - Get camera display name
- `GetFocalLength() -> Float` - Get current focal length in mm
- `GetTStop() -> Float` - Get current T-stop value
- `IsActive() -> Bool` - Check if camera is active
- `IsLive() -> Bool` - Check if camera is live
- `IsRecording() -> Bool` - Check if camera is recording

### 2. Director Control System

**Actor:** `DirectorController`  
**Component:** `SequenceRecorderComponent`, `MultiViewportComponent`

The director control system orchestrates camera switching, sequence recording, and viewport management.

#### Key Features:
- **Multi-Camera Switching** - Instant switching between 16 cameras
- **Level Sequence Recording** - Records all cuts with timecode
- **Transition Support** - Cut, Dissolve, Fade, Wipe, Push
- **Timeline Management** - Complete cut history with metadata

#### RPC Methods:
- `Server_StartRecording()` - Begin sequence recording
- `Server_PauseRecording()` - Pause recording
- `Server_ResumeRecording()` - Resume recording
- `Server_StopRecording()` - Stop and finalize recording
- `Server_SwitchCamera(Int, CameraSwitchMode, Float)` - Switch active camera
- `Server_SetViewportQuality(ViewportQuality)` - Adjust viewport quality
- `Server_SetGridLayout(String)` - Change grid layout
- `Server_ExportSequence(String)` - Export recorded sequence

#### Blueprint Functions:
- `GetRecordingState() -> RecordingState` - Get current recording state
- `GetCurrentTimecode() -> Float` - Get current timecode in seconds
- `GetActiveCameraID() -> Int` - Get active camera ID
- `GetCutCount() -> Int` - Get number of recorded cuts
- `FormatTimecode() -> String` - Get formatted timecode (HH:MM:SS:FF)

### 3. Lens Simulation System

**Component:** `LensSimulationComponent`  
**Shader:** `LensSimulation` (Compute)

The lens simulation system provides physically accurate lens effects using GPU compute shaders.

#### Lens Parameters:
- **Focal Length** - 8mm to 600mm
- **T-Stop** - 1.0 to 22.0 (transmission stop, not f-stop)
- **Focus Distance** - 10cm to 100m
- **Bokeh Blade Count** - 5 to 16 blades
- **Chromatic Aberration** - 0.0 to 1.0
- **Vignette Intensity** - 0.0 to 1.0
- **Distortion Amount** - -1.0 to 1.0
- **Anamorphic Squeeze** - 1.0 to 2.0

#### Physical Calculations:

**Circle of Confusion (CoC):**
```
CoC = |aperture_diameter * focal_length * (subject_distance - focus_distance) / (subject_distance * (focus_distance - focal_length))|
```

**Hyperfocal Distance:**
```
H = (focal_length^2) / (t_stop * CoC) + focal_length
```

**Field of View:**
```
FOV = 2 * atan(sensor_width / (2 * focal_length))
```

**Exposure Value:**
```
EV = log2(N^2 / t)
where N = T-stop, t = shutter speed
```

#### Shader Permutations:
- `CFG_HIGH_QUALITY_BOKEH` - 32 samples vs 8 samples
- `CFG_CHROMATIC_ABERRATION` - Enable RGB channel separation
- `ENABLE_VIGNETTE` - Enable radial falloff
- `ENABLE_DISTORTION` - Enable barrel/pincushion distortion

### 4. Multi-Viewport System

**Component:** `MultiViewportComponent`  
**Shader:** `ViewportBatcher` (Compute)

The multi-viewport system manages rendering of up to 16 simultaneous camera feeds.

#### Viewport Management:
- **Grid Layouts** - 4x4 (16 cams), 3x3 (9 cams), 2x2 (4 cams), 1x1 (1 cam)
- **Quality Levels** - Preview, Standard, High, Cinematic
- **Update Frequency** - 15Hz to 120Hz
- **Viewport Batching** - Reduces draw calls by rendering to texture atlas

#### Performance Optimization:
- **Texture Atlas** - All viewports render to single large texture
- **Culling** - Off-screen viewports are not updated
- **LOD System** - Distant viewports use lower quality
- **Async Rendering** - Viewports render in parallel on GPU

### 5. Sequence Recording System

**Component:** `SequenceRecorderComponent`  
**Data:** `DirectorCutData`

The sequence recording system captures camera cuts and exports to UE5 Level Sequences.

#### Recording Data:
- **Timecode** - Frame-accurate timestamp
- **Source Camera** - Camera ID before cut
- **Target Camera** - Camera ID after cut
- **Transition Type** - Cut, Dissolve, Fade, Wipe, Push
- **Transition Duration** - Length of transition in seconds
- **Notes** - Optional director notes

#### Export Formats:
- **Level Sequence** - Native UE5 format
- **FBX** - Autodesk FBX with camera animation
- **Alembic** - Open-source interchange format
- **USD** - Universal Scene Description
- **XML** - Edit Decision List (EDL)

### 6. Cinematic Post-Processing

**Shader:** `CinematicGrading` (Compute)

The cinematic grading system provides per-camera color grading and film effects.

#### Grading Parameters:
- **Exposure** - -5.0 to +5.0 EV
- **Contrast** - 0.0 to 2.0
- **Saturation** - 0.0 to 2.0
- **Temperature** - 2000K to 10000K
- **Tint** - -1.0 to +1.0 (green/magenta)
- **Film Grain** - 0.0 to 1.0 intensity

#### Shader Permutations:
- `CFG_ENABLE_LUT` - Enable 3D LUT color grading
- `CFG_FILM_GRAIN` - Enable film grain simulation

## Data Structures

### Enums

```kain
enum CameraType:
    Wide, Medium, Close, Extreme, Establishing,
    OverShoulder, POV, Aerial, Tracking, Dolly,
    Crane, Handheld, Steadicam, Static, Custom

enum LensManufacturer:
    Panavision, Cooke, Zeiss, Canon, Arri,
    Angenieux, Sigma, Leica, Anamorphic, Vintage, Custom

enum LensType:
    Prime, Zoom, Anamorphic, Tilt, Macro, Fisheye

enum RecordingState:
    Idle, Recording, Paused, Reviewing, Exporting

enum CameraSwitchMode:
    Cut, Dissolve, Fade, Wipe, Push

enum ViewportQuality:
    Preview, Standard, High, Cinematic
```

### DataTables

**LensPreset:**
- Stores real-world lens data from manufacturers
- Includes focal length, T-stop range, focus distances
- Contains optical characteristics (aberration, vignette, distortion)
- Importable from CSV files

**CameraPreset:**
- Stores camera configurations
- Includes position, rotation, FOV
- Contains post-processing settings
- Allows quick camera setup

**DirectorCutData:**
- Stores recorded camera cuts
- Includes timecode, source/target cameras
- Contains transition type and duration
- Exportable to EDL format

## UI Architecture

### Slate Widgets

**DirectorControlBoard:**
- Main control interface for director
- 16 "Take Cam X" buttons for instant switching
- Record/Stop/Pause controls
- Timecode display
- Cut counter

**CameraGridWidget:**
- 4x4 grid of live camera viewports
- Click to select camera
- Double-click to take live
- Visual indicators for active/live/recording state

**LensControlPanel:**
- Real-time lens parameter adjustment
- Focal length slider (8-600mm)
- T-stop slider (1.0-22.0)
- Focus distance slider (10cm-100m)
- Lens preset selector

**TimelineWidget:**
- Visual timeline of recorded sequence
- Cut markers with camera IDs
- Playhead scrubbing
- Zoom controls
- Cut editing (trim, delete, reorder)

**RecordingStatusWidget:**
- Live recording status indicator
- Timecode display (HH:MM:SS:FF)
- Sequence name
- Cut count
- Disk space monitoring

**CameraInfoOverlay:**
- On-screen camera information
- Camera ID and name
- Focal length and T-stop
- Live/Recording indicators
- Frame rate display

**ExposureControlWidget:**
- Exposure compensation slider
- Color temperature slider (2000K-10000K)
- Tint slider (green/magenta)
- ISO display
- Shutter speed display

### Details Panels

**VirtualCameraDetails:**
- Complete camera configuration
- Lens parameters with sliders
- Exposure settings
- Post-processing controls
- Action buttons (Set Active, Take Live, Load Preset)

**DirectorControllerDetails:**
- Recording configuration
- Sequence settings (name, FPS)
- Viewport quality settings
- Grid layout selection
- Export options

### Viewports

**CameraPreviewViewport:**
- Individual camera preview
- 3D scene rendering
- Camera transform visualization
- Live/Preview mode toggle

**MultiCameraViewport:**
- Multiple camera preview
- Grid layout rendering
- Synchronized updates
- Performance optimized

**DirectorMainViewport:**
- Main director view
- Scene overview
- Camera placement visualization
- Orbit controls

### Toolbars

**DirectorToolbar:**
- Quick access to recording controls
- Camera switching shortcuts (F1-F12)
- View toggles (Grid, Overlays, Safe Frames)
- Quality and layout dropdowns
- Export button

**LensToolbar:**
- Quick lens presets (Wide, Normal, Tele)
- Quick T-stop presets (T2.8, T4, T5.6)
- Load/Save preset buttons

## Performance Considerations

### GPU Optimization

**Shader Dispatch:**
- All lens effects run on GPU compute shaders
- Parallel processing of all 16 viewports
- Viewport batching reduces draw calls
- Async compute for non-blocking execution

**Memory Management:**
- Texture atlas for viewport rendering
- Shared shader resources across cameras
- Culling of off-screen viewports
- LOD system for distant viewports

### CPU Optimization

**Networking:**
- Delta compression for replicated state
- Relevancy filtering for camera updates
- Batched RPC calls for camera switching
- Lazy evaluation of non-visible cameras

**UI Updates:**
- Throttled Slate widget updates (30Hz)
- Cached text formatting
- Deferred property updates
- Minimal redraws

### Scalability

**Quality Presets:**
- **Preview** - 8 samples, 15Hz, low resolution
- **Standard** - 16 samples, 30Hz, medium resolution
- **High** - 24 samples, 60Hz, high resolution
- **Cinematic** - 32 samples, 60Hz, full resolution

**Grid Layouts:**
- **4x4** - 16 cameras, highest load
- **3x3** - 9 cameras, balanced
- **2x2** - 4 cameras, performance
- **1x1** - 1 camera, maximum quality

## Integration Guide

### Blueprint Integration

```cpp
// Get director controller
ADirectorController* Director = GetWorld()->SpawnActor<ADirectorController>();

// Start recording
Director->Server_StartRecording();

// Switch to camera 3 with dissolve transition
Director->Server_SwitchCamera(3, CameraSwitchMode::Dissolve, 1.0f);

// Stop recording
Director->Server_StopRecording();

// Export sequence
Director->Server_ExportSequence("/Game/Sequences/MySequence");
```

### C++ Integration

```cpp
// Create virtual camera
AVirtualCamera* Camera = GetWorld()->SpawnActor<AVirtualCamera>();
Camera->camera_id = 1;
Camera->camera_name = "Hero Camera";
Camera->camera_type = CameraType::Medium;

// Configure lens
Camera->Server_SetLensParameters(50.0f, 2.8f, 300.0f);

// Set exposure
Camera->Server_SetExposure(0.0f, 6500.0f, 0.0f);

// Activate camera
Camera->Server_SetActive(true);
Camera->Server_SetLive(true);
```

### Level Sequence Integration

```cpp
// Create Level Sequence
ULevelSequence* Sequence = NewObject<ULevelSequence>();

// Add camera cut track
UMovieSceneCameraCutTrack* CutTrack = Sequence->GetMovieScene()->AddMasterTrack<UMovieSceneCameraCutTrack>();

// Add camera cut section
UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(CutTrack->CreateNewSection());
CutSection->SetRange(TRange<FFrameNumber>(0, 100));
CutSection->SetCameraBindingID(CameraBinding);
```

## Troubleshooting

### Common Issues

**Issue:** Low frame rate with 16 viewports  
**Solution:** Reduce viewport quality to Preview, decrease grid size to 2x2, lower update frequency to 15Hz

**Issue:** Camera switching has visible lag  
**Solution:** Enable viewport pre-rendering, increase VRAM allocation, use viewport batching

**Issue:** Lens effects not visible  
**Solution:** Ensure T-stop is below 5.6, check focus distance is set correctly, enable high-quality bokeh

**Issue:** Recording sequence is empty  
**Solution:** Verify recording state is "Recording", check active camera ID is valid, ensure Level Sequence is created

**Issue:** Export fails  
**Solution:** Check export path is valid, verify disk space is available, ensure sequence has recorded cuts

### Debug Commands

```
CineMaster.ShowDebugInfo 1  // Show debug overlay
CineMaster.ShowCameraInfo 1  // Show camera information
CineMaster.ShowLensInfo 1  // Show lens parameters
CineMaster.ShowPerformance 1  // Show performance stats
CineMaster.DumpSequence  // Dump sequence data to log
```

## Future Enhancements

### Planned Features
- **Motion Capture Integration** - Live actor tracking
- **Virtual Camera App** - iPad/iPhone control
- **AI Camera Operator** - Automated camera movement
- **Multi-User Collaboration** - Multiple directors
- **Cloud Recording** - Remote sequence storage
- **Real-Time Compositing** - Live green screen keying
- **HDR Support** - High dynamic range rendering
- **Ray Tracing** - Real-time ray-traced reflections

---

**CineMaster Pro v1.0** - Technical Documentation  
Last Updated: 2024
