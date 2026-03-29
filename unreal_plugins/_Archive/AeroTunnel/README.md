# AeroTunnel - Professional Flight Physics & Wind Tunnel Editor

**Version:** 1.0.0  
**Price:** $499-699  
**Target:** Flight sim developers, aerospace engineers, vehicle game developers  
**Engine:** Unreal Engine 5.4+

---

## 🚀 Overview

AeroTunnel is a professional-grade UE5 plugin that brings **true aerodynamic physics** to your projects using **Blade Element Theory**. Whether you're building a flight simulator, aerospace visualization tool, or game with flying vehicles, AeroTunnel provides the accuracy and tools you need.

### Key Features

✅ **True 6-DOF Aerodynamic Physics** - Blade Element Theory implementation  
✅ **GPU-Accelerated** - 4 compute shaders for real-time force calculations  
✅ **Wind Tunnel Editor** - Interactive 3D visualization with real-time feedback  
✅ **Live Telemetry Dashboard** - G-Force, Mach number, stall warnings  
✅ **Works with Any Mesh** - Skeletal or Static Mesh support  
✅ **120Hz Physics** - Sub-stepping independent of framerate  
✅ **Real Airfoil Database** - NACA 0012, 2412, 4412, Clark Y, and more  
✅ **Debug Visualization** - Blue lift vectors, red drag vectors, pressure maps  
✅ **Flight Data Export** - CSV export for analysis  
✅ **Blueprint Integration** - Full Blueprint support for gameplay

---

## 📊 Technical Specifications

### Aerodynamic Model

- **Blade Element Theory** - Mesh divided into elements, forces calculated per element
- **Lift Coefficient:** `CL = CL_alpha * α` (with stall modeling)
- **Drag Coefficient:** `CD = CD_min + K * CL²` (induced drag)
- **Dynamic Pressure:** `q = 0.5 * ρ * V²`
- **Compressibility Corrections** - Prandtl-Glauert for subsonic/transonic
- **Reynolds Number Effects** - Viscous drag modeling
- **Stall Detection** - Real-time flow separation analysis

### Flight Regimes

- **Subsonic** - Mach < 0.8
- **Transonic** - Mach 0.8 - 1.2
- **Supersonic** - Mach 1.2 - 5.0
- **Hypersonic** - Mach > 5.0

### Airfoil Profiles

| Profile | Type | Max CL | Stall Angle | Use Case |
|---------|------|--------|-------------|----------|
| NACA 0012 | Symmetric | 1.2 | 15° | General purpose, aerobatics |
| NACA 2412 | Cambered | 1.6 | 16° | High lift, general aviation |
| NACA 4412 | High camber | 1.8 | 14° | Maximum lift, slow flight |
| Clark Y | Classic | 1.5 | 15° | Stable, forgiving |
| NACA 6409 | Laminar | 1.4 | 12° | Low drag, high speed |
| Flat Plate | Simple | 0.8 | 10° | Basic simulation |

### Performance

- **Physics Rate:** 120 Hz (sub-stepped)
- **Telemetry Rate:** 60 Hz
- **Max Blade Elements:** 10,000 per mesh
- **GPU Compute:** 4 shaders (BladeElementForces, VortexWake, Turbulence, StallDetection)

---

## 🎮 Quick Start

### Installation

1. **Build the plugin:**
   ```batch
   cd Factory/AeroTunnel
   Build5.4.bat
   ```

2. **Copy to your project:**
   ```
   YourProject/Plugins/AeroTunnel/
   ```

3. **Generate project files:**
   - Right-click your `.uproject` file
   - Select "Generate Visual Studio project files"

4. **Build in Visual Studio:**
   - Open the solution
   - Build (Development Editor configuration)

5. **Enable in Unreal:**
   - Launch Unreal Editor
   - Edit → Plugins → Search "AeroTunnel"
   - Enable and restart

### Basic Usage

1. **Open Wind Tunnel:**
   - Tools → AeroTunnel → Open Wind Tunnel

2. **Add Aerodynamic Component:**
   ```cpp
   // In Blueprint or C++
   AerodynamicComponent* Aero = CreateDefaultSubobject<UAerodynamicComponent>(TEXT("Aero"));
   ```

3. **Configure Airfoil:**
   - Select your aircraft actor
   - Details panel → Aerodynamic Component
   - Set Airfoil Profile (e.g., NACA 2412)
   - Set Wing Area, Span, Chord

4. **Start Simulation:**
   - Click "Start Simulation" in toolbar
   - Watch real-time force vectors
   - Monitor telemetry dashboard

---

## 🔧 Editor UI

### Wind Tunnel Viewport

- **3D Visualization** - Real-time aircraft preview
- **Force Vectors** - Blue (lift), Red (drag)
- **Pressure Map** - Color-coded surface pressure
- **Grid Overlay** - Reference grid
- **Camera Controls** - Orbit, zoom, pan

### Telemetry Dashboard

- **Airspeed** - m/s, km/h, knots
- **Altitude** - meters, feet
- **Mach Number** - Real-time calculation
- **Angle of Attack** - Degrees
- **G-Force** - Current load factor
- **Stall Warning** - Visual/audio alerts
- **Flight Regime** - Subsonic/Transonic/Supersonic/Hypersonic

### Telemetry Graphs

- **G-Force History** - Last 10 seconds
- **Mach History** - Speed over time
- **AoA History** - Angle of attack trends

### Details Panel

- **Mesh Analysis** - Blade element count, wing area, span, chord
- **Airfoil Configuration** - Profile, lift curve slope, drag coefficient
- **Mass Properties** - Mass, moment of inertia
- **Physics Settings** - Sub-step rate, air density, speed of sound
- **Visualization** - Debug modes, vector colors, scale

---

## 📐 Blueprint API

### Get Flight Data

```cpp
// Get current airspeed
float Airspeed = AerodynamicAircraft->GetAirspeed();

// Get Mach number
float Mach = AerodynamicAircraft->GetMachNumber();

// Get angle of attack
float AoA = AerodynamicAircraft->GetAngleOfAttack();

// Get G-force
float GForce = AerodynamicAircraft->GetGForce();

// Check if stalled
bool IsStalled = AerodynamicAircraft->IsStalled();
```

### Get Forces

```cpp
// Get lift force vector
FVector Lift = AerodynamicAircraft->GetLiftForce();

// Get drag force vector
FVector Drag = AerodynamicAircraft->GetDragForce();

// Get center of pressure
FVector CoP = AerodynamicAircraft->GetCenterOfPressure();
```

### Configure Airfoil

```cpp
// Set airfoil profile
AerodynamicAircraft->SetAirfoilProfile(EAirfoilProfile::NACA2412);

// Set visualization mode
AerodynamicAircraft->SetVisualizationMode(EDebugVisualizationMode::ForceVectors);

// Export flight data
FString FilePath = AerodynamicAircraft->ExportFlightData();
```

### Wind Tunnel Control

```cpp
// Start wind tunnel simulation
WindTunnel->StartSimulation();

// Set wind speed
WindTunnel->SetWindSpeed(50.0f); // m/s

// Set tunnel mode
WindTunnel->SetTunnelMode(EWindTunnelMode::Dynamic);

// Stop simulation
WindTunnel->StopSimulation();
```

---

## 🧮 Aerodynamic Calculations

### Lift Force

```
L = CL * q * S
where:
  CL = Lift coefficient (from airfoil)
  q = Dynamic pressure (0.5 * ρ * V²)
  S = Wing area
```

### Drag Force

```
D = CD * q * S
where:
  CD = CD_min + K * CL² (induced drag)
  K = 1 / (π * AR * e)
  AR = Aspect ratio (span² / area)
  e = Oswald efficiency factor (~0.8)
```

### Stall Speed

```
V_stall = sqrt((2 * W) / (ρ * S * CL_max))
where:
  W = Weight (mass * g)
  ρ = Air density
  S = Wing area
  CL_max = Maximum lift coefficient
```

### Turn Radius

```
R = V² / (g * tan(φ))
where:
  V = Velocity
  g = Gravity (9.81 m/s²)
  φ = Bank angle
```

---

## 🎨 Visualization Modes

### Force Vectors
- **Blue arrows** - Lift force direction and magnitude
- **Red arrows** - Drag force direction and magnitude
- **Scale adjustable** - 0.1x to 10x

### Pressure Map
- **Blue** - High suction (low pressure)
- **Green** - Neutral pressure
- **Red** - High pressure
- **Real-time updates** - 60 Hz

### Velocity Field
- **Streamlines** - Airflow visualization
- **Color-coded** - Speed magnitude
- **Turbulence** - Vortex wake

### Stall Regions
- **Red overlay** - Flow separation areas
- **Pulsing effect** - Warning indicator
- **Per-element** - Detailed stall map

---

## 📊 Data Export

### CSV Format

```csv
timestamp,airspeed_ms,altitude_m,mach,aoa_deg,g_force,lift_n,drag_n,stall_state
0.0,50.0,1000.0,0.146,5.2,1.05,12000.0,800.0,Normal
0.016,50.2,1000.1,0.147,5.3,1.06,12100.0,810.0,Normal
...
```

### Use Cases

- **Performance analysis** - Compare different airfoils
- **Flight envelope** - Determine safe operating limits
- **Optimization** - Tune for maximum efficiency
- **Validation** - Compare with real-world data

---

## 🎯 Use Cases

### Flight Simulators
- Realistic flight dynamics
- Accurate stall behavior
- Proper control surface response

### Aerospace Visualization
- Educational tools
- Engineering demonstrations
- Wind tunnel simulations

### Game Development
- Flying vehicles (planes, helicopters, drones)
- Space flight
- Sci-fi aircraft

### Research & Development
- Rapid prototyping
- Design iteration
- Performance prediction

---

## 🔬 Advanced Features

### Compressibility Corrections

- **Prandtl-Glauert** - Subsonic correction: `β = sqrt(1 - M²)`
- **Transonic modeling** - Simplified shock wave effects
- **Supersonic** - Area rule considerations

### Vortex Wake

- **Trailing vortices** - Wingtip vortex simulation
- **Induced drag** - Downwash effects
- **Vortex decay** - Viscous dissipation

### Atmospheric Turbulence

- **Perlin noise** - Realistic turbulence
- **Gust modeling** - Wind shear effects
- **Frequency control** - Adjustable intensity

---

## 📚 Documentation

- **User Manual** - Complete guide (included)
- **API Reference** - Blueprint/C++ documentation
- **Tutorial Videos** - Step-by-step walkthroughs
- **Example Projects** - Sample aircraft setups

---

## 🛠️ Support

- **Email:** support@kainfactory.com
- **Discord:** discord.gg/kainfactory
- **Documentation:** docs.kainfactory.com/aerotunnel
- **Updates:** Regular updates with new features

---

## 📜 License

Commercial license included with purchase. Royalty-free for shipped games.

---

## 🏆 Why AeroTunnel?

### vs. Manual Implementation
- **10x faster** - No need to write complex physics code
- **GPU-accelerated** - Real-time performance
- **Proven algorithms** - Blade Element Theory is industry-standard

### vs. Simple Physics
- **Accurate** - Real aerodynamic forces, not approximations
- **Detailed** - Per-element calculations, not whole-body
- **Realistic** - Proper stall behavior, compressibility effects

### vs. External Tools
- **Integrated** - No need to export/import data
- **Real-time** - Instant feedback in editor
- **Interactive** - Tweak and test immediately

---

## 🚀 Get Started Today!

Build professional flight physics in minutes, not months.

**Price:** $499-699  
**Build:** `kain build --ue5`  
**Support:** Lifetime updates included

---

*Built with KAIN - The future of UE5 plugin development*
