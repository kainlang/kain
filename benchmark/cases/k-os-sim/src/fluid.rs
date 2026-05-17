//! K_OS Fluid Simulation - Salva3D SPH for Texture Painting
//!
//! 2D UV-space SPH simulation (constrained to Z=0 plane).
//! Computes velocity fields that drive GPU texture advection.
//! Powers: HYDRO, DRIP, FLOW, VORTEX, PARTICULATE effects.

use lazy_static::lazy_static;
use rand::Rng;
use salva3d::integrations::rapier::FluidsPipeline;
use salva3d::math::{Point, Vector};
use salva3d::object::Fluid;
use salva3d::solver::{Akinci2013SurfaceTension, XSPHViscosity};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::Mutex;

// ============================================================================
// HELPERS
// ============================================================================

fn make_point(x: f32, y: f32) -> Point<f32> {
    Point::new(x, y, 0.0) // Z=0 for UV space
}

fn make_vector(x: f32, y: f32) -> Vector<f32> {
    Vector::new(x, y, 0.0)
}

// ============================================================================
// UV-SPACE FLUID SIMULATION
// ============================================================================

lazy_static! {
    pub static ref FLUID_SIMS: Mutex<HashMap<u64, UVFluidSim>> = Mutex::new(HashMap::new());
    static ref NEXT_SIM_ID: Mutex<u64> = Mutex::new(1);
}

/// UV-space fluid simulation (2D constrained to Z=0)
pub struct UVFluidSim {
    pub fluids_pipeline: FluidsPipeline,
    has_fluid: bool,
    particle_radius: f32,
    gravity: Vector<f32>,

    // Cached data
    positions: Vec<[f32; 2]>,
    velocities: Vec<[f32; 2]>,

    // Pending velocity modifications
    pending_mods: Vec<(usize, f32, f32)>, // (index, dvx, dvy)
}

impl UVFluidSim {
    pub fn new(gravity_y: f32, particle_radius: f32) -> Self {
        Self {
            fluids_pipeline: FluidsPipeline::new(particle_radius, 2.0),
            has_fluid: false,
            particle_radius,
            gravity: make_vector(0.0, gravity_y),
            positions: Vec::new(),
            velocities: Vec::new(),
            pending_mods: Vec::new(),
        }
    }

    /// Initialize fluid with particles in UV space
    pub fn create_fluid(&mut self, positions: Vec<[f32; 2]>, density: f32) {
        let points: Vec<Point<f32>> = positions.iter().map(|p| make_point(p[0], p[1])).collect();

        // High viscosity for paint-like behavior
        let viscosity = XSPHViscosity::new(2.0, 0.0);
        let tension = Akinci2013SurfaceTension::new(2.0, 0.0);

        let mut fluid = Fluid::new(points, self.particle_radius, density);
        fluid.nonpressure_forces.push(Box::new(viscosity));
        fluid.nonpressure_forces.push(Box::new(tension));

        self.fluids_pipeline.liquid_world.add_fluid(fluid);
        self.has_fluid = true;

        log::info!(
            "[K_OS Fluid] Created UV fluid with {} particles",
            positions.len()
        );
    }

    /// Spawn particles at UV position (from brush stroke)
    pub fn spawn_at_uv(&mut self, uv: [f32; 2], velocity: [f32; 2], count: usize) {
        let mut rng = rand::thread_rng();

        if !self.has_fluid {
            // Create initial fluid
            let mut positions = Vec::with_capacity(count);
            for _ in 0..count {
                let jitter_x = (rng.gen::<f32>() - 0.5) * self.particle_radius * 4.0;
                let jitter_y = (rng.gen::<f32>() - 0.5) * self.particle_radius * 4.0;
                positions.push([
                    (uv[0] + jitter_x).clamp(0.0, 1.0),
                    (uv[1] + jitter_y).clamp(0.0, 1.0),
                ]);
            }
            self.create_fluid(positions.clone(), 1000.0);

            // Set initial velocities via the positions/velocities cache
            self.velocities = vec![velocity; count];
            self.positions = positions;
        }
        // Note: Adding to existing fluid would require more complex handling
        // For now, we just create new fluids per spawn
    }

    /// Step simulation
    pub fn step(&mut self, dt: f32) {
        // Apply pending velocity modifications first
        self.apply_pending_mods();

        // Step the physics
        self.fluids_pipeline.liquid_world.step(dt, &self.gravity);

        // Cache and constrain to UV bounds
        self.positions.clear();
        self.velocities.clear();

        for (_, fluid) in self.fluids_pipeline.liquid_world.fluids().iter() {
            for (i, p) in fluid.positions.iter().enumerate() {
                let mut px = p.x.clamp(0.0, 1.0);
                let mut py = p.y.clamp(0.0, 1.0);

                let vx = fluid.velocities[i].x;
                let vy = fluid.velocities[i].y;

                // Boundary reflection (crude but works)
                let mut nvx = vx;
                let mut nvy = vy;
                if px <= 0.001 || px >= 0.999 {
                    nvx *= -0.5;
                    px = px.clamp(0.001, 0.999);
                }
                if py <= 0.001 || py >= 0.999 {
                    nvy *= -0.5;
                    py = py.clamp(0.001, 0.999);
                }

                self.positions.push([px, py]);
                self.velocities.push([nvx, nvy]);
            }
        }
    }

    /// Apply pending modifications (called at step time)
    fn apply_pending_mods(&mut self) {
        if self.pending_mods.is_empty() {
            return;
        }

        // We have to work through the fluids iter safely
        // For now, we accumulate to cached velocities which will affect next spawn
        for (idx, dvx, dvy) in self.pending_mods.drain(..) {
            if idx < self.velocities.len() {
                self.velocities[idx][0] += dvx;
                self.velocities[idx][1] += dvy;
            }
        }
    }

    /// Get velocity field as a grid for GPU upload - PARALLEL WITH RAYON!
    /// Returns flat array: [vx0, vy0, vx1, vy1, ...] row-major
    pub fn get_velocity_grid(&self, resolution: usize) -> Vec<f32> {
        use rayon::prelude::*;

        let cell_size = 1.0 / resolution as f32;
        let influence_radius = cell_size * 3.0;
        let inv_influence = 1.0 / influence_radius;

        // Pre-compute particle data for cache efficiency
        let particle_data: Vec<_> = self
            .positions
            .iter()
            .zip(self.velocities.iter())
            .map(|(p, v)| (p[0], p[1], v[0], v[1]))
            .collect();

        // Parallel grid computation - each cell computed independently
        let grid: Vec<f32> = (0..resolution)
            .into_par_iter()
            .flat_map(|gy| {
                let cell_y = (gy as f32 + 0.5) * cell_size;
                let mut row = vec![0.0f32; resolution * 2];

                for gx in 0..resolution {
                    let cell_x = (gx as f32 + 0.5) * cell_size;
                    let mut vx = 0.0f32;
                    let mut vy = 0.0f32;

                    // Sum contributions from all nearby particles
                    for &(px, py, pvx, pvy) in &particle_data {
                        let dx = px - cell_x;
                        let dy = py - cell_y;
                        let dist_sq = dx * dx + dy * dy;

                        if dist_sq < influence_radius * influence_radius {
                            let dist = dist_sq.sqrt();
                            let weight = 1.0 - dist * inv_influence;
                            let weight = weight * weight;
                            vx += pvx * weight;
                            vy += pvy * weight;
                        }
                    }

                    row[gx * 2] = vx;
                    row[gx * 2 + 1] = vy;
                }
                row
            })
            .collect();

        grid
    }

    /// Queue force at UV position (applied at next step)
    pub fn queue_force(&mut self, uv: [f32; 2], force: [f32; 2], radius: f32) {
        for (i, pos) in self.positions.iter().enumerate() {
            let dx = uv[0] - pos[0];
            let dy = uv[1] - pos[1];
            let dist = (dx * dx + dy * dy).sqrt();

            if dist < radius && dist > 0.0001 {
                let falloff = 1.0 - (dist / radius);
                self.pending_mods
                    .push((i, force[0] * falloff, force[1] * falloff));
            }
        }
    }

    /// Queue vortex (swirl) at UV position
    pub fn queue_vortex(&mut self, uv: [f32; 2], strength: f32, radius: f32) {
        for (i, pos) in self.positions.iter().enumerate() {
            let dx = uv[0] - pos[0];
            let dy = uv[1] - pos[1];
            let dist = (dx * dx + dy * dy).sqrt();

            if dist < radius && dist > 0.0001 {
                let falloff = 1.0 - (dist / radius);
                // Perpendicular (tangent) direction
                let tx = -dy / dist;
                let ty = dx / dist;
                self.pending_mods
                    .push((i, tx * strength * falloff, ty * strength * falloff));
            }
        }
    }

    /// Queue black hole (inward + spin)
    pub fn queue_black_hole(&mut self, uv: [f32; 2], strength: f32, spin: f32, radius: f32) {
        for (i, pos) in self.positions.iter().enumerate() {
            let dx = uv[0] - pos[0];
            let dy = uv[1] - pos[1];
            let dist = (dx * dx + dy * dy).sqrt();

            if dist < radius && dist > 0.0001 {
                // Inward gravity
                let grav = strength / (dist * dist + 0.01);
                let inward_x = (dx / dist) * grav;
                let inward_y = (dy / dist) * grav;

                // Frame dragging (spin)
                let tx = -dy / dist;
                let ty = dx / dist;
                let drag = spin / (dist + 0.1);

                self.pending_mods
                    .push((i, inward_x + tx * drag, inward_y + ty * drag));
            }
        }
    }

    /// Get particle count
    pub fn particle_count(&self) -> usize {
        self.positions.len()
    }

    /// Clear all particles
    pub fn clear(&mut self) {
        self.fluids_pipeline = FluidsPipeline::new(self.particle_radius, 2.0);
        self.has_fluid = false;
        self.positions.clear();
        self.velocities.clear();
        self.pending_mods.clear();
    }
}

// ============================================================================
// TAURI COMMANDS
// ============================================================================

#[derive(Serialize, Deserialize)]
pub struct FluidState {
    pub positions: Vec<[f32; 2]>,
    pub velocities: Vec<[f32; 2]>,
    pub particle_count: usize,
}

/// Create UV-space fluid simulation
#[tauri::command]
pub fn create_fluid_sim(gravity_y: f32, particle_radius: f32) -> Result<u64, String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let mut next_id = NEXT_SIM_ID.lock().map_err(|e| e.to_string())?;

    let id = *next_id;
    *next_id += 1;

    let sim = UVFluidSim::new(gravity_y, particle_radius);
    sims.insert(id, sim);

    log::info!("[K_OS Fluid] Created UV simulation {}", id);
    Ok(id)
}

/// Spawn particles at UV position (from brush)
#[tauri::command]
pub fn spawn_fluid_at_uv(
    sim_id: u64,
    uv: [f32; 2],
    velocity: [f32; 2],
    count: usize,
) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.spawn_at_uv(uv, velocity, count);
    Ok(())
}

/// Step simulation
#[tauri::command]
pub fn step_fluid_sim(sim_id: u64, dt: f32) -> Result<FluidState, String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.step(dt);

    Ok(FluidState {
        positions: sim.positions.clone(),
        velocities: sim.velocities.clone(),
        particle_count: sim.particle_count(),
    })
}

/// Get velocity grid for GPU upload
#[tauri::command]
pub fn get_velocity_grid(sim_id: u64, resolution: usize) -> Result<Vec<f32>, String> {
    let sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get(&sim_id).ok_or("Simulation not found")?;
    Ok(sim.get_velocity_grid(resolution))
}

/// Apply force at UV position
#[tauri::command]
pub fn apply_fluid_force(
    sim_id: u64,
    uv: [f32; 2],
    force: [f32; 2],
    radius: f32,
) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.queue_force(uv, force, radius);
    Ok(())
}

/// Apply vortex
#[tauri::command]
pub fn apply_fluid_vortex(
    sim_id: u64,
    uv: [f32; 2],
    strength: f32,
    radius: f32,
) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.queue_vortex(uv, strength, radius);
    Ok(())
}

/// Apply black hole
#[tauri::command]
pub fn apply_fluid_black_hole(
    sim_id: u64,
    uv: [f32; 2],
    strength: f32,
    spin: f32,
    radius: f32,
) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.queue_black_hole(uv, strength, spin, radius);
    Ok(())
}

/// Clear simulation
#[tauri::command]
pub fn clear_fluid_sim(sim_id: u64) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.clear();
    Ok(())
}

/// Dispose simulation
#[tauri::command]
pub fn dispose_fluid_sim(sim_id: u64) -> Result<(), String> {
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    sims.remove(&sim_id);
    log::info!("[K_OS Fluid] Disposed simulation {}", sim_id);
    Ok(())
}

// Legacy compatibility commands
#[tauri::command]
pub fn init_fluid_particles(
    sim_id: u64,
    positions: Vec<[f32; 3]>,
    density: f32,
) -> Result<(), String> {
    let positions_2d: Vec<[f32; 2]> = positions.iter().map(|p| [p[0], p[1]]).collect();
    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.create_fluid(positions_2d, density);
    Ok(())
}

#[tauri::command]
pub fn add_fluid_particles(sim_id: u64, positions: Vec<[f32; 3]>) -> Result<(), String> {
    if positions.is_empty() {
        return Ok(());
    }
    let avg_x: f32 = positions.iter().map(|p| p[0]).sum::<f32>() / positions.len() as f32;
    let avg_y: f32 = positions.iter().map(|p| p[1]).sum::<f32>() / positions.len() as f32;

    let mut sims = FLUID_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;
    sim.spawn_at_uv([avg_x, avg_y], [0.0, 0.0], positions.len());
    Ok(())
}

#[tauri::command]
pub fn add_fluid_boundary(
    _sim_id: u64,
    _vertices: Vec<[f32; 3]>,
    _indices: Vec<[u32; 3]>,
) -> Result<(), String> {
    Ok(())
}
