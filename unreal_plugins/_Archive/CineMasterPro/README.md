# CineMaster Pro - Virtual Production & Multi-Cam Director Studio

**Price:** $899-1,299 (Premium)  
**Target:** Virtual production directors, cinematographers, pre-vis artists  
**UE5 Version:** 5.4+

## Overview

CineMaster Pro is the ultimate virtual production tool for Unreal Engine 5, providing professional-grade multi-camera direction capabilities with physical lens simulation. Direct up to 16 cameras simultaneously with real-time preview, instant switching, and Level Sequence recording.

## Key Features

### 🎥 Multi-Camera System
- **16 Simultaneous Cameras** - View all cameras at once in a 4x4 grid
- **Instant Camera Switching** - Zero-hitch transitions between cameras
- **Live Preview** - Real-time rendering of all camera feeds
- **Flexible Grid Layouts** - 4x4, 3x3, 2x2, or single camera view
- **Camera Types** - Wide, Medium, Close-Up, POV, Aerial, Tracking, Dolly, Crane, Steadicam, and more

### 🔍 Physical Lens Simulation
- **Real-World Lens Data** - Panavision, Cooke, Zeiss, Canon, ARRI, Angenieux, Sigma, Leica
- **T-Stop Accurate DOF** - Physically accurate depth of field based on transmission stops
- **Anamorphic Support** - 2.39:1 aspect ratio with squeeze factor simulation
- **Bokeh Simulation** - Adjustable blade count (5-16 blades)
- **Lens Aberrations** - Chromatic aberration, vignette, distortion
- **Focus Control** - Precise focus distance control (10cm - 100m)
- **Focal Length Range** - 8mm (fisheye) to 600mm (super telephoto)

### 📹 Recording & Sequencing
- **Level Sequence Integration** - Records camera cuts directly to UE5 sequences
- **Timecode Display** - Industry-standard HH:MM:SS:FF format
- **Multiple Frame Rates** - 23.976, 24, 25, 29.97, 30, 60, 120 fps
- **Transition Types** - Cut, Dissolve, Fade, Wipe, Push
- **Cut History** - Complete timeline of all camera switches
- **Export Support** - FBX, Alembic, USD formats

### 🎨 Cinematic Post-Processing
- **Exposure Control** - EV compensation, ISO, shutter speed
- **Color Grading** - Temperature, tint, contrast, saturation
- **Film Grain** - Adjustable grain intensity
- **LUT Support** - 3D LUT color grading
- **Per-Camera Settings** - Individual post-processing per camera

### 🖥️ Professional UI
- **Director's Control Board** - Instant "Take Cam X" buttons for all 16 cameras
- **Camera Grid View** - Live preview of all cameras with overlays
- **Lens Control Panel** - Real-time lens parameter adjustment
- **Timeline Widget** - Visual representation of cuts and transitions
- **Recording Status** - Live timecode, cut count, disk space monitoring
- **Camera Info Overlays** - On-screen display of camera settings

## Installation

1. **Build the Plugin:**

   ```bash
   cd Factory/CineMasterPro
   Build5.4.bat
   ```

2. **Copy to UE5 Project:**
   ```
   YourProject/Plugins/CineMasterPro/
   ```

3. **Regenerate Project Files:**
   - Right-click your .uproject file
   - Select "Generate Visual Studio project files"

4. **Compile in UE5:**
   - Open your project in UE5
   - The plugin will compile automatically

5. **Enable Plugin:**
   - Edit → Plugins
   - Search for "CineMaster Pro"
   - Enable and restart

## Quick Start

### Setting Up Your First Virtual Production Session

1. **Open Director Studio:**
   - Tools → CineMaster Pro → Open Director Studio

2. **Place Virtual Cameras:**
   - Drag 16 VirtualCamera actors into your scene
   - Position them for different shot angles
   - Assign camera types (Wide, Medium, Close, etc.)

3. **Configure Lenses:**
   - Select each camera
   - Choose lens manufacturer (Panavision, Cooke, Zeiss)
   - Set focal length (24mm, 50mm, 85mm, etc.)
   - Adjust T-stop for depth of field

4. **Start Recording:**
   - Click "Record" button (Ctrl+R)
   - Use "Take Cam X" buttons to switch cameras
   - All cuts are recorded to Level Sequence

5. **Export Sequence:**
   - Click "Stop" when finished
   - Tools → CineMaster Pro → Export Sequence
   - Choose format (FBX, Alembic, USD)

## Lens Presets

CineMaster Pro includes authentic lens data from major manufacturers:

### Panavision
- **Primo Primes** - 14mm, 17.5mm, 21mm, 27mm, 35mm, 50mm, 75mm, 100mm, 150mm
- **Anamorphic C-Series** - 40mm, 50mm, 75mm, 100mm (2x squeeze)
- **T-Stops** - T1.9 to T22

### Cooke
- **S4/i Primes** - 18mm, 25mm, 32mm, 50mm, 75mm, 100mm, 135mm
- **Anamorphic/i** - 25mm, 32mm, 40mm, 50mm, 75mm, 100mm, 135mm, 180mm
- **T-Stops** - T2.0 to T22

### Zeiss
- **Master Primes** - 12mm, 16mm, 18mm, 21mm, 25mm, 35mm, 50mm, 85mm, 100mm, 135mm
- **Supreme Primes** - 15mm, 18mm, 21mm, 25mm, 29mm, 35mm, 50mm, 85mm, 100mm, 125mm, 150mm, 200mm
- **T-Stops** - T1.3 to T22

### Canon
- **CN-E Primes** - 14mm, 20mm, 24mm, 35mm, 50mm, 85mm, 135mm
- **T-Stops** - T1.5 to T22

### ARRI/Fujinon
- **Signature Primes** - 12mm, 16mm, 18mm, 21mm, 25mm, 29mm, 35mm, 40mm, 47mm, 58mm, 75mm, 95mm, 125mm, 150mm, 280mm
- **T-Stops** - T1.8 to T22

## Camera Types

### Standard Shots
- **Wide** - Establishing shots, environment
- **Medium** - Standard dialogue, action
- **Close** - Character focus, emotion
- **Extreme** - Detail shots, intensity

### Specialized Shots
- **Over-the-Shoulder** - Conversation, POV
- **POV** - First-person perspective
- **Aerial** - Drone shots, bird's eye view
- **Tracking** - Following action
- **Dolly** - Push in/pull out
- **Crane** - Sweeping movements
- **Handheld** - Documentary style
- **Steadicam** - Smooth tracking

## Technical Specifications

### Performance
- **16 Viewports** - 30-60 FPS depending on quality setting
- **GPU Accelerated** - All lens effects run on compute shaders
- **Memory Efficient** - Viewport batching reduces VRAM usage
- **Zero Hitches** - Camera switching uses pre-rendered buffers

### Lens Simulation
- **Physical Accuracy** - Thin lens equation for DOF
- **Bokeh Quality** - 8-32 samples per pixel (quality dependent)
- **Chromatic Aberration** - RGB channel separation
- **Vignette** - Radial falloff based on aperture
- **Distortion** - Barrel/pincushion distortion
- **Anamorphic** - Horizontal squeeze with oval bokeh

### Recording
- **Sequence Format** - UE5 Level Sequence
- **Cut Precision** - Frame-accurate switching
- **Metadata** - Camera settings, lens data, timecode
- **Export Formats** - FBX, Alembic, USD, XML

## Keyboard Shortcuts

### Recording
- **Ctrl+R** - Start Recording
- **Ctrl+S** - Stop Recording
- **Ctrl+P** - Pause Recording

### Camera Switching
- **F1-F12** - Take Camera 1-12 live
- **Shift+F1-F4** - Take Camera 13-16 live

### Lens Control
- **1** - Wide angle (24mm)
- **2** - Normal (50mm)
- **3** - Telephoto (85mm)
- **4** - Super telephoto (135mm)

### View Control
- **G** - Toggle grid overlay
- **O** - Toggle camera overlays
- **S** - Toggle safe frames
- **T** - Toggle timeline

## Workflow Examples

### Multi-Camera Interview Setup
1. Place 3 cameras: Wide (establishing), Medium (host), Close (guest)
2. Set all to 50mm Cooke S4/i at T2.8
3. Record interview
4. Switch between cameras during conversation
5. Export to FBX for editing

### Virtual Production Scene
1. Place 8 cameras around action area
2. Mix focal lengths: 24mm, 35mm, 50mm, 85mm
3. Set different T-stops for depth variation
4. Record scene with live switching
5. Review timeline and adjust cuts
6. Export to Alembic for VFX

### Anamorphic Cinematic Sequence
1. Use Panavision Anamorphic C-Series lenses
2. Set 2.0x squeeze factor
3. Configure 2.39:1 aspect ratio
4. Add film grain and color grading
5. Record with dissolve transitions
6. Export to USD for post-production

## Troubleshooting

### Low Frame Rate
- Reduce viewport quality (Preview mode)
- Decrease grid size (2x2 instead of 4x4)
- Lower update frequency (15-30 Hz)
- Disable high-quality bokeh

### Camera Switching Lag
- Enable viewport pre-rendering
- Increase VRAM allocation
- Use viewport batching
- Reduce post-processing effects

### Lens Effects Not Visible
- Ensure "Enable Physical Simulation" is checked
- Verify T-stop is below 5.6 (wider aperture)
- Check focus distance is set correctly
- Enable high-quality bokeh for better results

## Support

- **Documentation:** Tools → CineMaster Pro → Documentation
- **Email:** support@cinemasterpro.com
- **Discord:** discord.gg/cinemasterpro
- **YouTube:** youtube.com/cinemasterpro

## License

CineMaster Pro is a commercial plugin for Unreal Engine 5.
Single-seat license: $899
Studio license (5 seats): $3,999
Enterprise license: Contact sales

## Credits

Developed with KAIN language and compiler.
Lens data courtesy of respective manufacturers.
Built for virtual production professionals.

---

**CineMaster Pro v1.0** - The Future of Virtual Production
