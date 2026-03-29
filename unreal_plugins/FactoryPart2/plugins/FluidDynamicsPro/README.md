# FluidDynamicsPro

**Real-time GPU Fluid Simulation for Unreal Engine 5**

A production-ready fluid simulation plugin featuring SPH/FLIP solvers, GPU compute shaders, surface reconstruction, and advanced rendering materials.

## Features

### Simulation Systems
- **SPH Solver** (Smoothed Particle Hydrodynamics) - Classic particle-based fluid simulation
- **FLIP Solver** (Fluid Implicit Particle) - Grid-based hybrid approach
- **Hybrid Solver** - Combines SPH and FLIP for best results
- **GPU Acceleration** - 12 compute shaders for real-time performance
- **10,000+ Particles** - Supports large-scale simulations

### Physics Features
- Pressure and viscosity forces
- Surface tension
- Vorticity confinement (turbulence)
- Buoyancy
- Boundary conditions (solid, open, periodic)
- Collision detection (sphere, box, plane, mesh)
- Force fields (attract, repel, vortex, directional, turbulence, drag)

### Rendering
- **Surface Reconstruction** - Marching cubes algorithm for smooth surfaces
- **8 Material Graphs**:
  - Fluid surface with refraction and caustics
  - Particle rendering with velocity coloring
  - Underwater caustics
  - Foam generation
  - Depth-based coloring
  - Advanced refraction with chromatic aberration
  - Subsurface scattering
  - Procedural waves

### Performance
- Particle pooling (100,000 particles)
- Spatial hashing for O(n) neighbor search
- Automatic quality adjustment
- Time budget management
- Async surface reconstruction
- GPU compute for parallel processing

### Multiplayer
- Full network replication
- Replicated simulation state
- Efficient bandwidth usage

### Blueprint Integration
- 30+ Blueprint-callable functions
- Complete actor spawning and configuration
- Real-time parameter adjustment
- Statistics and debugging

## Quick Start

### 1. Build the Plugin
```bash
cd FactoryPart2/plugins/FluidDynamicsPro
kain build --ue5
```

### 2. Copy to UE5 Project
```bash
cp -r FluidDynamicsPro <YourProject>/Plugins/
```

### 3. Enable in UE5
- Edit → Plugins → Search "FluidDynamicsPro" → Enable → Restart

### 4. Place Actors in Level
- Place `FluidSimulatorActor` in level
- Configure particle count (start with 1,000)
- Press Play to see simulation

## Usage Examples

### Blueprint

#### Basic Simulation
```
1. Place FluidSimulatorActor in level
2. Set Particle Count = 10000
3. Set Solver Type = SPH
4. Set Bounds Min = (-10, -10, -10)
5. Set Bounds Max = (10, 10, 10)
6. Call "Start Simulation"
```

#### Add Emitter
```
1. Place FluidEmitterActor in level
2. Set Emit Rate = 100
3. Set Emit Velocity = (0, 0, 10)
4. Set Target Simulator = FluidSimulatorActor
5. Call "Start Emitting"
```

#### Add Collider
```
1. Place FluidColliderActor in level
2. Set Collider Type = Sphere
3. Set Scale = (5, 5, 5)
4. Set Target Simulator = FluidSimulatorActor
```

#### Add Force Field
```
Call "Add Force Field" on FluidSimulatorActor:
- Position = (0, 0, 0)
- Radius = 10
- Strength = 100
- Force Type = Vortex
```

### C++

```cpp
// Get subsystem
UFluidManagerSubsystem* FluidManager = GetWorld()->GetSubsystem<UFluidManagerSubsystem>();

// Spawn simulator
AFluidSimulatorActor* Simulator = GetWorld()->SpawnActor<AFluidSimulatorActor>();
Simulator->ParticleCount = 10000;
Simulator->SolverType = ESolverType::SPH;
Simulator->BoundsMin = FVector(-10, -10, -10);
Simulator->BoundsMax = FVector(10, 10, 10);
Simulator->StartSimulation();

// Configure global parameters
FluidManager->SetGlobalGravity(FVector(0, 0, -980));
FluidManager->SetGlobalViscosity(0.01f);

// Query statistics
int32 ActiveParticles = FluidManager->GetActiveParticleCount();
float FrameTime = FluidManager->GetCurrentFrameTime();
```

## Architecture

### Actors
- **FluidSimulatorActor** - Main simulation actor with solver, particles, and rendering
- **FluidEmitterActor** - Emits particles into simulation
- **FluidColliderActor** - Defines collision geometry

### Subsystem
- **FluidManager** - World subsystem managing particle pool and global parameters

### Solvers
- **SPHSolver** - Smoothed Particle Hydrodynamics implementation
- **FLIPSolver** - Fluid Implicit Particle implementation
- **HybridSolver** - Combines SPH and FLIP

### Compute Shaders
1. ParticleAdvection - Updates particle positions
2. DensityCalculation - Computes particle density
3. PressureSolve - Computes pressure from density
4. PressureForce - Applies pressure forces
5. ViscositySolve - Applies viscosity forces
6. SurfaceReconstruction - Builds density field
7. NormalCalculation - Computes surface normals
8. VorticityConfinement - Adds turbulence
9. BoundaryHandling - Enforces boundaries
10. ParticleCollision - Handles collisions
11. FluidRendering - Computes rendering data
12. SpatialGridConstruction - Builds spatial hash

### Materials
1. FluidSurfaceMaterial - Complete water surface
2. FluidParticleMaterial - Particle rendering
3. FluidCausticsMaterial - Underwater caustics
4. FluidFoamMaterial - Foam rendering
5. FluidDepthMaterial - Depth-based coloring
6. FluidRefractionMaterial - Advanced refraction
7. FluidSubsurfaceMaterial - Subsurface scattering
8. FluidWavesMaterial - Procedural waves

### Async Tasks
1. SurfaceReconstructionTask - Marching cubes mesh generation
2. ParticleSimulationTask - Background particle updates
3. DensityFieldComputeTask - Parallel density computation

## Configuration

### Fluid Parameters
- **Particle Count** - Number of particles (1,000 - 100,000)
- **Solver Type** - SPH, FLIP, or Hybrid
- **Rest Density** - Target fluid density (default: 1000 kg/m³)
- **Gas Constant** - Pressure stiffness (default: 2000)
- **Viscosity** - Fluid thickness (default: 0.01)
- **Surface Tension** - Surface cohesion (default: 0.0728)
- **Gravity** - Gravity vector (default: (0, 0, -980))
- **Smoothing Radius** - Particle interaction radius (default: 0.1)
- **Substeps** - Simulation substeps per frame (default: 4)

### Rendering Parameters
- **Render Mode** - Particles, Surface, Hybrid, Wireframe, Debug
- **Particle Size** - Visual particle size (default: 0.05)
- **Surface Smoothness** - Surface roughness (default: 0.8)
- **Refraction Index** - IOR for refraction (default: 1.33)
- **Foam Threshold** - Velocity threshold for foam (default: 0.5)
- **Caustics Intensity** - Underwater caustics strength (default: 1.0)

### Performance Parameters
- **Quality** - Low, Medium, High, Ultra, Cinematic
- **Time Budget** - Max simulation time per frame (default: 16ms)
- **Use GPU** - Enable GPU acceleration (default: true)
- **Enable Surface Reconstruction** - Enable marching cubes (default: true)

## Performance Guide

### Recommended Settings

#### Low-End (GTX 1060)
- Particle Count: 1,000 - 5,000
- Quality: Low
- Substeps: 2
- Surface Reconstruction: Disabled

#### Mid-Range (RTX 3060)
- Particle Count: 5,000 - 10,000
- Quality: Medium
- Substeps: 4
- Surface Reconstruction: Enabled (32³ grid)

#### High-End (RTX 3070+)
- Particle Count: 10,000 - 50,000
- Quality: High
- Substeps: 4
- Surface Reconstruction: Enabled (64³ grid)

#### Ultra (RTX 4090)
- Particle Count: 50,000 - 100,000
- Quality: Ultra
- Substeps: 8
- Surface Reconstruction: Enabled (128³ grid)

### Optimization Tips
1. Start with low particle counts and increase gradually
2. Use spatial hashing for neighbor search (automatic)
3. Enable GPU acceleration (default)
4. Adjust substeps based on stability needs
5. Disable surface reconstruction for particle-only rendering
6. Use performance mode for automatic quality adjustment
7. Profile with Unreal Insights to identify bottlenecks

## Troubleshooting

### Simulation is slow
- Reduce particle count
- Lower quality setting
- Reduce substeps
- Disable surface reconstruction
- Enable performance mode

### Particles explode
- Increase substeps
- Reduce time step
- Increase viscosity
- Check boundary conditions

### Surface looks jagged
- Increase marching cubes resolution
- Enable mesh smoothing
- Increase smoothing iterations

### Collisions not working
- Verify collider is registered with simulator
- Check collider scale
- Adjust friction and restitution

### Replication issues
- Verify `@replicated` attributes
- Check network bandwidth
- Reduce particle count for multiplayer

## Technical Details

### Implementation
- **Language**: KAIN (compiles to UE5 C++)
- **Lines of Code**: ~3,200 LOC KAIN → 11,000-13,000 LOC C++
- **Compute Shaders**: 12 .usf files
- **Materials**: 8 .uasset files
- **Actors**: 3 replicated actors
- **Subsystem**: 1 world subsystem with tick

### Dependencies
- Unreal Engine 5.4+
- RenderCore module (for compute shaders)
- RHI module (for GPU resources)
- Niagara module (optional, for particle rendering)

### Memory Usage
- Particle pool: ~12 MB (100,000 particles)
- Spatial grid: ~2 MB (64³ cells)
- Density field: ~1 MB (64³ floats)
- Total: ~15-20 MB

### Compression Ratio
- KAIN source: 3,200 LOC
- Generated C++: 11,000-13,000 LOC
- **Ratio: 1:3.5 to 1:4**

## License

This plugin is generated by the KAIN compiler as part of the Factory plugin assembly line.

## Credits

- **KAIN Compiler** - Multi-paradigm systems language
- **SPH Algorithm** - Müller et al. (2003)
- **FLIP Algorithm** - Bridson & Müller-Fischer (2007)
- **Marching Cubes** - Lorensen & Cline (1987)

## Support

For issues, questions, or contributions:
- Check `IMPLEMENTATION_COMPLETE.md` for technical details
- Review `BUILD_READY.md` for build instructions
- See KAIN documentation for language reference

## Version History

### v1.0.0 (Initial Release)
- SPH, FLIP, and Hybrid solvers
- 12 GPU compute shaders
- 8 material graphs
- 3 async tasks
- Network replication
- Blueprint integration
- Performance optimization
- Surface reconstruction
- Particle pooling
- Force fields and collisions

---

**FluidDynamicsPro** - Real-time GPU fluid simulation for Unreal Engine 5
