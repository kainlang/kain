# AeroTunnel - Technical Specification

## Architecture Overview

AeroTunnel implements a **hybrid CPU-GPU architecture** for aerodynamic simulation:

- **CPU:** High-level physics integration, state management, networking
- **GPU:** Blade element force calculations, vortex wake, turbulence, stall detection

---

## Blade Element Theory Implementation

### Mesh Discretization

1. **Mesh Analysis Phase** (CPU)
   - Parse Static/Skeletal Mesh vertices and triangles
   - Divide mesh into blade elements (typically 500-10,000 elements)
   - Calculate element properties:
     - Position (center of triangle)
     - Normal vector
     - Area (m²)
     - Local chord length
     - Spanwise width

2. **Element Data Structure**
   ```cpp
   struct BladeElement {
       FVector Position;        // Element center (world space)
       FVector Normal;          // Surface normal
       float Area;              // Element area (m²)
       float Chord;             // Local chord length (m)
       float Span;              // Spanwise width (m)
       float AngleOfAttack;     // Local AoA (radians)
       FVector RelativeVelocity; // Local airflow velocity
       FVector LiftForce;       // Computed lift force (N)
       FVector DragForce;       // Computed drag force (N)
       float PressureCoefficient; // Cp value
   };
   ```

### Force Calculation (GPU Shader)

**Shader:** `BladeElementForces`

**Per-Element Calculation:**

1. **Relative Velocity**
   ```
   V_rel = V_freestream - V_element
   ```

2. **Angle of Attack**
   ```
   α = arcsin(dot(V_direction, normal))
   ```

3. **Dynamic Pressure**
   ```
   q = 0.5 * ρ * V²
   ```

4. **Lift Coefficient**
   ```
   if |α| < α_stall:
       CL = CL_alpha * α / β  (Prandtl-Glauert correction)
   else:
       CL = CL_alpha * α_stall * 0.5 / β  (post-stall)
   ```

5. **Drag Coefficient**
   ```
   CD = CD_min + K * CL²  (induced drag)
   
   if ENABLE_VISCOUS_EFFECTS:
       Re = V * c / ν
       Cf = 0.074 / Re^0.2  (skin friction)
       CD += Cf
   ```

6. **Force Vectors**
   ```
   Lift = CL * q * A * normal_perpendicular
   Drag = CD * q * A * V_direction
   ```

7. **Pressure Coefficient**
   ```
   Cp = 1 - (V_local / V_freestream)²
   ```

### Force Integration (CPU)

1. **Sum all element forces:**
   ```cpp
   FVector TotalLift = FVector::ZeroVector;
   FVector TotalDrag = FVector::ZeroVector;
   FVector TotalMoment = FVector::ZeroVector;
   FVector CenterOfPressure = FVector::ZeroVector;
   
   for (const BladeElement& Element : BladeElements) {
       TotalLift += Element.LiftForce;
       TotalDrag += Element.DragForce;
       
       // Moment = r × F
       FVector r = Element.Position - CenterOfMass;
       TotalMoment += FVector::CrossProduct(r, Element.LiftForce + Element.DragForce);
       
       // Weighted CoP
       float ForceMagnitude = (Element.LiftForce + Element.DragForce).Size();
       CenterOfPressure += Element.Position * ForceMagnitude;
   }
   
   CenterOfPressure /= TotalForce;
   ```

2. **Apply to rigid body:**
   ```cpp
   // Linear acceleration
   FVector Acceleration = (TotalLift + TotalDrag) / Mass;
   Velocity += Acceleration * DeltaTime;
   Position += Velocity * DeltaTime;
   
   // Angular acceleration
   FVector AngularAcceleration = TotalMoment / MomentOfInertia;
   AngularVelocity += AngularAcceleration * DeltaTime;
   Orientation += AngularVelocity * DeltaTime;
   ```

---

## Physics Sub-Stepping

### Why 120 Hz?

Aerodynamic forces can change rapidly, especially near stall. Standard 60 Hz game tick is insufficient for stability.

### Implementation

```cpp
void AerodynamicAircraft::Tick(float DeltaTime) {
    AccumulatedTime += DeltaTime;
    float SubstepDT = 1.0f / PhysicsSubstepHz; // 1/120 = 0.00833s
    
    while (AccumulatedTime >= SubstepDT) {
        PhysicsSubstep(SubstepDT);
        AccumulatedTime -= SubstepDT;
    }
}

void AerodynamicAircraft::PhysicsSubstep(float DT) {
    // 1. Calculate aerodynamic forces (GPU)
    DispatchBladeElementShader();
    
    // 2. Integrate forces
    Velocity += (Lift + Drag) / Mass * DT;
    Position += Velocity * DT;
    
    // 3. Integrate moments
    AngularVelocity += Moment / Inertia * DT;
    Orientation += AngularVelocity * DT;
    
    // 4. Update derived quantities
    Airspeed = Velocity.Size();
    Mach = Airspeed / SpeedOfSound;
    AoA = CalculateAngleOfAttack();
}
```

---

## GPU Compute Shaders

### 1. BladeElementForces

**Purpose:** Calculate lift and drag for each blade element

**Thread Group:** 64 threads per group

**Dispatch:** `ceil(ElementCount / 64)` groups

**Permutations:**
- `CFG_HIGH_PRECISION` - Double precision math
- `CFG_COMPRESSIBILITY` - Prandtl-Glauert correction
- `ENABLE_VISCOUS_EFFECTS` - Reynolds number effects

**Performance:** ~0.5ms for 5000 elements @ 60 FPS

### 2. VortexWake

**Purpose:** Simulate trailing vortices for induced drag

**Algorithm:**
1. Create vortex filaments at wing tips
2. Convect with freestream
3. Calculate induced velocity (Biot-Savart law)
4. Apply viscous decay

**Performance:** ~0.2ms for 100 vortices

### 3. AtmosphericTurbulence

**Purpose:** Generate realistic turbulence field

**Algorithm:**
1. 3D Perlin noise
2. Time-varying frequency
3. Intensity scaling

**Performance:** ~0.1ms for 64³ field

### 4. StallDetection

**Purpose:** Detect flow separation per element

**Algorithm:**
1. Analyze pressure coefficient
2. Compare to stall threshold
3. Flag stalled elements

**Performance:** ~0.1ms for 5000 elements

**Total GPU Time:** ~0.9ms per frame (well under 16.67ms budget)

---

## Airfoil Database

### NACA 0012 (Symmetric)

```
CL_alpha = 6.28 rad⁻¹ (2π)
CD_min = 0.006
α_stall = 15°
CL_max = 1.2
CM = 0.0 (symmetric)
```

**Use:** Aerobatics, vertical stabilizers

### NACA 2412 (Cambered)

```
CL_alpha = 6.28 rad⁻¹
CD_min = 0.008
α_stall = 16°
CL_max = 1.6
CM = -0.05
α_zero_lift = -2°
```

**Use:** General aviation, high lift

### NACA 4412 (High Camber)

```
CL_alpha = 6.28 rad⁻¹
CD_min = 0.010
α_stall = 14°
CL_max = 1.8
CM = -0.08
α_zero_lift = -4°
```

**Use:** Slow flight, maximum lift

### Clark Y (Classic)

```
CL_alpha = 6.10 rad⁻¹
CD_min = 0.009
α_stall = 15°
CL_max = 1.5
CM = -0.04
```

**Use:** Stable, forgiving, vintage aircraft

### NACA 6409 (Laminar Flow)

```
CL_alpha = 6.28 rad⁻¹
CD_min = 0.004
α_stall = 12°
CL_max = 1.4
CM = -0.03
```

**Use:** High speed, low drag

### Flat Plate

```
CL_alpha = 2π rad⁻¹ (theoretical)
CD_min = 0.020
α_stall = 10°
CL_max = 0.8
CM = 0.0
```

**Use:** Simple simulation, testing

---

## Compressibility Corrections

### Prandtl-Glauert (Subsonic)

```
β = sqrt(1 - M²)
CL_compressible = CL_incompressible / β
```

**Valid:** M < 0.8

### Transonic (0.8 < M < 1.2)

Simplified model:
```
β = 0.5 (constant)
CL_transonic = CL_incompressible / 0.5
```

**Note:** Full transonic modeling requires CFD

### Supersonic (M > 1.2)

Simplified model:
```
CL_supersonic = 4 * α / sqrt(M² - 1)
```

**Note:** Shock wave effects not fully modeled

---

## Stall Modeling

### Pre-Stall (|α| < α_stall)

Linear lift curve:
```
CL = CL_alpha * α
```

### Stall (|α| ≥ α_stall)

Reduced lift:
```
CL = CL_alpha * α_stall * 0.5
```

### Deep Stall (|α| > α_stall + 5°)

Minimal lift:
```
CL = CL_alpha * α_stall * 0.2
```

### Stall Detection

Per-element pressure coefficient:
```
if Cp > Cp_threshold:
    Element is stalled
```

**Threshold:** Typically `Cp > 0.5`

---

## Telemetry System

### Data Collection

**Sample Rate:** 60 Hz (configurable)

**Buffering:** Ring buffer, last 300 samples (5 seconds)

**Data Points:**
- G-Force
- Mach number
- Angle of attack
- Airspeed
- Altitude
- Lift force
- Drag force
- Stall state

### Graph Rendering

**Implementation:** Custom Slate widget with line rendering

**Update Rate:** 60 FPS

**Time Window:** 10 seconds (configurable)

---

## Wind Tunnel Modes

### Static

Constant wind speed and direction:
```
V_wind = constant
```

### Dynamic

Sinusoidal variation:
```
V_wind = V_base + A * sin(ω * t)
```

### Gust

Turbulent gusts:
```
V_wind = V_base + Gust(t)
Gust(t) = A * Perlin3D(t * f)
```

### Vortex

Vortex wake interaction:
```
V_wind = V_base + V_induced
V_induced = Σ (Γ × r) / (4π * |r|³)
```

---

## Debug Visualization

### Force Vectors

**Rendering:** `FPrimitiveSceneProxy` with custom draw calls

**Update Rate:** 60 FPS

**Scaling:** Adjustable 0.1x - 10x

**Colors:**
- Lift: Blue (0.2, 0.5, 1.0)
- Drag: Red (1.0, 0.3, 0.2)

### Pressure Map

**Rendering:** Material parameter collection

**Update Rate:** 60 FPS

**Color Gradient:**
- Blue: Cp < -1.0 (high suction)
- Green: Cp ≈ 0.0 (neutral)
- Red: Cp > 1.0 (high pressure)

### Stall Regions

**Rendering:** Translucent overlay

**Update Rate:** 60 FPS

**Effect:** Pulsing red warning

---

## Performance Optimization

### GPU Compute

- **Thread Group Size:** 64 (optimal for most GPUs)
- **Memory Coalescing:** Contiguous buffer access
- **Early Exit:** Skip elements outside influence

### CPU Integration

- **SIMD:** Vectorized force summation
- **Parallel:** Multi-threaded element processing
- **Cache-Friendly:** Structure-of-arrays layout

### Memory

- **Blade Elements:** ~100 bytes each
- **5000 elements:** ~500 KB
- **GPU Buffers:** ~2 MB total
- **Telemetry History:** ~50 KB

### Profiling Results

**Test Setup:** 5000 blade elements, 60 FPS

| Component | Time (ms) | % Frame |
|-----------|-----------|---------|
| GPU Shaders | 0.9 | 5.4% |
| Force Integration | 0.3 | 1.8% |
| Physics Substep | 0.5 | 3.0% |
| Telemetry | 0.1 | 0.6% |
| Visualization | 0.8 | 4.8% |
| **Total** | **2.6** | **15.6%** |

**Headroom:** 14.07ms remaining (84.4% of 16.67ms frame budget)

---

## Networking

### Replication

**Replicated State:**
- Current velocity
- Current position
- Current orientation
- Airspeed
- Altitude
- Angle of attack

**Update Rate:** 20 Hz (sufficient for visual smoothness)

**Bandwidth:** ~200 bytes/sec per aircraft

### RPCs

**Server RPCs:**
- `Server_AnalyzeMesh()`
- `Server_SetAirfoilProfile()`
- `Server_SetVisualizationMode()`

**Client RPCs:**
- `Client_TriggerStallWarning()`

**Multicast RPCs:**
- `Multicast_UpdateAirfoilProfile()`
- `Multicast_UpdateVisualization()`

---

## Data Export

### CSV Format

```csv
timestamp,airspeed_ms,altitude_m,mach,aoa_deg,g_force,lift_n,drag_n,stall_state
0.000,50.0,1000.0,0.146,5.2,1.05,12000.0,800.0,Normal
0.016,50.2,1000.1,0.147,5.3,1.06,12100.0,810.0,Normal
```

### Export Frequency

**Options:**
- Real-time (60 Hz)
- Downsampled (10 Hz)
- Event-based (on stall, etc.)

---

## Future Enhancements

### Planned Features

1. **Control Surface Modeling**
   - Elevator, aileron, rudder deflection
   - Hinge moments
   - Trim calculations

2. **Propeller/Jet Thrust**
   - Propeller disk theory
   - Jet engine thrust curves
   - Thrust vectoring

3. **Ground Effect**
   - Increased lift near ground
   - Reduced induced drag

4. **Multi-Element Airfoils**
   - Flaps, slats
   - High-lift devices

5. **Unsteady Aerodynamics**
   - Wagner function
   - Dynamic stall

6. **CFD Integration**
   - Import CFD results
   - Hybrid CFD-BET

---

## References

### Textbooks

1. Anderson, J.D. "Fundamentals of Aerodynamics" (6th Ed.)
2. McCormick, B.W. "Aerodynamics, Aeronautics, and Flight Mechanics"
3. Katz, J. "Low-Speed Aerodynamics"

### Papers

1. Glauert, H. "The Elements of Aerofoil and Airscrew Theory" (1926)
2. Prandtl, L. "Applications of Modern Hydrodynamics to Aeronautics" (1921)

### Standards

1. NACA Airfoil Database
2. ISO 2533:1975 (Standard Atmosphere)

---

## Support

For technical questions:
- **Email:** tech@kainfactory.com
- **Discord:** #aerotunnel-tech
- **Docs:** docs.kainfactory.com/aerotunnel/technical

---

*AeroTunnel - Professional aerodynamics for Unreal Engine 5*
