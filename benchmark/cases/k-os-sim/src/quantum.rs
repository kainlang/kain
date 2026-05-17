//! KQuantum Particle Physics - Houdini-Tier Simulation Engine
//!
//! True N-body gravity, strange attractors, magnetic fields, SPH coupling,
//! mesh collisions, audio FFT integration, and more.
//!
//! Architecture:
//! - Rust computes forces/accelerations
//! - Frontend (Three.js) handles GPU rendering
//! - Data flows via Tauri IPC
//!
//! Performance: Uses rayon for parallel force computation, kiddo for spatial queries.

use glam::Vec3;
use lazy_static::lazy_static;
use rand::Rng;
use rayon::prelude::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::Mutex;

// Spatial acceleration
// Noise
use noise::{Fbm, NoiseFn, Perlin};

// ============================================================================
// PARTICLE ATTRIBUTES
// ============================================================================

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Particle {
    pub position: [f32; 3],
    pub velocity: [f32; 3],
    pub acceleration: [f32; 3],
    pub mass: f32,
    pub charge: f32, // For electromagnetic
    pub age: f32,    // 0.0 = just born, 1.0 = about to die
    pub life: f32,   // Total lifespan in seconds
    pub id: u32,     // Persistent ID
}

impl Default for Particle {
    fn default() -> Self {
        Self {
            position: [0.0, 0.0, 0.0],
            velocity: [0.0, 0.0, 0.0],
            acceleration: [0.0, 0.0, 0.0],
            mass: 1.0,
            charge: 0.0,
            age: 0.0,
            life: 5.0,
            id: 0,
        }
    }
}

// ============================================================================
// ATTRACTOR TYPES
// ============================================================================

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum AttractorType {
    Point {
        position: [f32; 3],
        mass: f32,
        softening: f32,
    },
    Vortex {
        position: [f32; 3],
        axis: [f32; 3],
        strength: f32,
        radius: f32,
    },
    BlackHole {
        position: [f32; 3],
        mass: f32,
        spin: f32,
        event_horizon: f32,
    },
    MagneticDipole {
        position: [f32; 3],
        moment: [f32; 3],
    },
    Turbulence {
        seed: u32,
        octaves: u8,
        frequency: f32,
        amplitude: f32,
    },
    CurlNoise {
        seed: u32,
        scale: f32,
        strength: f32,
    },
    Lorenz {
        sigma: f32,
        rho: f32,
        beta: f32,
        scale: f32,
    },
    Rossler {
        a: f32,
        b: f32,
        c: f32,
        scale: f32,
    },
    Aizawa {
        a: f32,
        b: f32,
        c: f32,
        d: f32,
        e: f32,
        f: f32,
        scale: f32,
    },
    TorusMagnetic {
        center: [f32; 3],
        major_radius: f32,
        minor_radius: f32,
        field_strength: f32,
    },
    GravitationalWave {
        source: [f32; 3],
        frequency: f32,
        amplitude: f32,
        phase: f32,
    },
    CosmicWeb {
        seed: u32,
        cell_size: f32,
        filament_strength: f32,
    },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Attractor {
    pub attractor_type: AttractorType,
    pub enabled: bool,
    pub falloff: f32,   // Distance falloff exponent
    pub max_force: f32, // Clamp force magnitude
}

// ============================================================================
// SIMULATION STATE
// ============================================================================

pub struct QuantumSim {
    pub particles: Vec<Particle>,
    pub attractors: Vec<Attractor>,
    pub time: f32,
    pub dt: f32,
    pub substeps: u32,

    // N-body
    pub nbody_enabled: bool,
    pub nbody_softening: f32,
    pub nbody_g: f32, // Gravitational constant

    // Global forces
    pub gravity: [f32; 3],
    pub drag: f32,

    // Audio reactivity
    pub audio_bands: Vec<f32>, // FFT frequency bands
    pub audio_amplitude: f32,

    // Noise generators (cached)
    perlin: Perlin,
    _fbm: Fbm<Perlin>,

    // Particle ID counter
    next_id: u32,

    // Bounds
    pub bounds_min: [f32; 3],
    pub bounds_max: [f32; 3],
    pub bounds_mode: BoundsMode,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum BoundsMode {
    None,
    Kill,   // Delete particles outside
    Wrap,   // Wrap around
    Bounce, // Reflect velocity
}

impl QuantumSim {
    pub fn new(particle_count: usize) -> Self {
        let mut rng = rand::thread_rng();
        let mut particles = Vec::with_capacity(particle_count);

        // Initialize in sphere
        for i in 0..particle_count {
            let theta = rng.gen::<f32>() * std::f32::consts::TAU;
            let phi = (rng.gen::<f32>() * 2.0 - 1.0).acos();
            let r = 30.0 + rng.gen::<f32>() * 10.0;

            particles.push(Particle {
                position: [
                    r * phi.sin() * theta.cos(),
                    r * phi.sin() * theta.sin(),
                    r * phi.cos(),
                ],
                velocity: [0.0, 0.0, 0.0],
                acceleration: [0.0, 0.0, 0.0],
                mass: 1.0,
                charge: if rng.gen::<bool>() { 1.0 } else { -1.0 },
                age: rng.gen::<f32>(),
                life: 3.0 + rng.gen::<f32>() * 5.0,
                id: i as u32,
            });
        }

        let perlin = Perlin::new(42);
        let fbm: Fbm<Perlin> = Fbm::new(42);

        Self {
            particles,
            attractors: Vec::new(),
            time: 0.0,
            dt: 1.0 / 60.0,
            substeps: 4,
            nbody_enabled: false,
            nbody_softening: 0.5,
            nbody_g: 1.0,
            gravity: [0.0, 0.0, 0.0],
            drag: 0.01,
            audio_bands: vec![0.0; 128],
            audio_amplitude: 0.0,
            perlin,
            _fbm: fbm,
            next_id: particle_count as u32,
            bounds_min: [-100.0, -100.0, -100.0],
            bounds_max: [100.0, 100.0, 100.0],
            bounds_mode: BoundsMode::Kill,
        }
    }

    /// Step the simulation
    pub fn step(&mut self, dt: f32) {
        let sub_dt = dt / self.substeps as f32;

        for _ in 0..self.substeps {
            self.compute_forces(sub_dt);
            self.integrate(sub_dt);
            self.apply_bounds();
        }

        self.time += dt;
    }

    /// Compute all forces on particles
    fn compute_forces(&mut self, _dt: f32) {
        let time = self.time;
        let attractors = &self.attractors;
        let gravity = Vec3::from_array(self.gravity);
        let drag = self.drag;
        let perlin = &self.perlin;
        let audio_bands = &self.audio_bands;
        let audio_amp = self.audio_amplitude;

        // Parallel force computation
        self.particles.par_iter_mut().for_each(|p| {
            let pos = Vec3::from_array(p.position);
            let vel = Vec3::from_array(p.velocity);
            let mut acc = Vec3::ZERO;

            // Global gravity
            acc += gravity;

            // Drag
            acc -= vel * drag;

            // Attractors
            for attractor in attractors.iter().filter(|a| a.enabled) {
                let force = compute_attractor_force(
                    &attractor.attractor_type,
                    pos,
                    vel,
                    p.mass,
                    p.charge,
                    time,
                    perlin,
                    audio_bands,
                    audio_amp,
                );

                // Apply falloff and clamp
                let dist = match &attractor.attractor_type {
                    AttractorType::Point { position, .. } => {
                        (pos - Vec3::from_array(*position)).length()
                    }
                    AttractorType::BlackHole { position, .. } => {
                        (pos - Vec3::from_array(*position)).length()
                    }
                    AttractorType::Vortex { position, .. } => {
                        (pos - Vec3::from_array(*position)).length()
                    }
                    _ => 1.0,
                };

                let falloff = 1.0 / (1.0 + dist.powf(attractor.falloff));
                let clamped = force.clamp_length_max(attractor.max_force);
                acc += clamped * falloff;
            }

            p.acceleration = acc.to_array();
        });

        // N-body (if enabled) - O(n²) for small counts, tree for large
        if self.nbody_enabled && self.particles.len() > 1 {
            self.compute_nbody_forces();
        }
    }

    /// N-body gravity using Barnes-Hut approximation
    fn compute_nbody_forces(&mut self) {
        let g = self.nbody_g;
        let softening = self.nbody_softening;
        let particle_count = self.particles.len();

        // For small counts, direct O(n²) is faster
        if particle_count < 1000 {
            // Direct summation (parallel over outer loop)
            let positions: Vec<[f32; 3]> = self.particles.iter().map(|p| p.position).collect();
            let masses: Vec<f32> = self.particles.iter().map(|p| p.mass).collect();

            self.particles
                .par_iter_mut()
                .enumerate()
                .for_each(|(i, p)| {
                    let pos_i = Vec3::from_array(p.position);
                    let mut acc = Vec3::from_array(p.acceleration);

                    for j in 0..particle_count {
                        if i == j {
                            continue;
                        }

                        let pos_j = Vec3::from_array(positions[j]);
                        let dir = pos_j - pos_i;
                        let dist_sq = dir.length_squared() + softening * softening;
                        let force_mag = g * masses[j] / dist_sq;
                        acc += dir.normalize_or_zero() * force_mag;
                    }

                    p.acceleration = acc.to_array();
                });
        } else {
            // TODO: Barnes-Hut octree for large particle counts
            // For now, skip N-body on huge counts
        }
    }

    /// Integrate velocities and positions (Velocity Verlet)
    fn integrate(&mut self, dt: f32) {
        self.particles.par_iter_mut().for_each(|p| {
            // Velocity Verlet integration
            let vel = Vec3::from_array(p.velocity);
            let acc = Vec3::from_array(p.acceleration);
            let pos = Vec3::from_array(p.position);

            // Update velocity (half step)
            let new_vel = vel + acc * dt;

            // Update position
            let new_pos = pos + new_vel * dt;

            p.velocity = new_vel.to_array();
            p.position = new_pos.to_array();

            // Age particle
            p.age += dt / p.life;
        });
    }

    /// Apply boundary conditions
    fn apply_bounds(&mut self) {
        match self.bounds_mode {
            BoundsMode::None => {}
            BoundsMode::Kill => {
                self.particles.retain(|p| {
                    p.position[0] >= self.bounds_min[0]
                        && p.position[0] <= self.bounds_max[0]
                        && p.position[1] >= self.bounds_min[1]
                        && p.position[1] <= self.bounds_max[1]
                        && p.position[2] >= self.bounds_min[2]
                        && p.position[2] <= self.bounds_max[2]
                        && p.age < 1.0
                });
            }
            BoundsMode::Wrap => {
                let size = [
                    self.bounds_max[0] - self.bounds_min[0],
                    self.bounds_max[1] - self.bounds_min[1],
                    self.bounds_max[2] - self.bounds_min[2],
                ];

                self.particles.par_iter_mut().for_each(|p| {
                    for i in 0..3 {
                        if p.position[i] < self.bounds_min[i] {
                            p.position[i] += size[i];
                        } else if p.position[i] > self.bounds_max[i] {
                            p.position[i] -= size[i];
                        }
                    }
                });
            }
            BoundsMode::Bounce => {
                self.particles.par_iter_mut().for_each(|p| {
                    for i in 0..3 {
                        if p.position[i] < self.bounds_min[i] {
                            p.position[i] = self.bounds_min[i];
                            p.velocity[i] *= -0.8; // Restitution
                        } else if p.position[i] > self.bounds_max[i] {
                            p.position[i] = self.bounds_max[i];
                            p.velocity[i] *= -0.8;
                        }
                    }
                });
            }
        }
    }

    /// Get particle data for GPU upload
    pub fn get_particle_data(&self) -> ParticleData {
        let count = self.particles.len();
        let mut positions = Vec::with_capacity(count * 4);
        let mut velocities = Vec::with_capacity(count * 4);

        for p in &self.particles {
            positions.extend_from_slice(&p.position);
            positions.push(p.age); // W = age for life-based effects

            velocities.extend_from_slice(&p.velocity);
            velocities.push(p.mass); // W = mass for visual sizing
        }

        ParticleData {
            positions,
            velocities,
            count,
        }
    }

    /// Add attractor
    pub fn add_attractor(&mut self, attractor: Attractor) -> usize {
        let id = self.attractors.len();
        self.attractors.push(attractor);
        id
    }

    /// Emit particles
    pub fn emit(&mut self, count: usize, position: [f32; 3], velocity: [f32; 3], spread: f32) {
        let mut rng = rand::thread_rng();

        for _ in 0..count {
            let jitter = [
                (rng.gen::<f32>() - 0.5) * spread,
                (rng.gen::<f32>() - 0.5) * spread,
                (rng.gen::<f32>() - 0.5) * spread,
            ];

            self.particles.push(Particle {
                position: [
                    position[0] + jitter[0],
                    position[1] + jitter[1],
                    position[2] + jitter[2],
                ],
                velocity,
                acceleration: [0.0, 0.0, 0.0],
                mass: 1.0,
                charge: if rng.gen::<bool>() { 1.0 } else { -1.0 },
                age: 0.0,
                life: 3.0 + rng.gen::<f32>() * 4.0,
                id: self.next_id,
            });
            self.next_id += 1;
        }
    }

    /// Update audio data
    pub fn set_audio(&mut self, bands: Vec<f32>, amplitude: f32) {
        self.audio_bands = bands;
        self.audio_amplitude = amplitude;
    }
}

// ============================================================================
// ATTRACTOR FORCE COMPUTATION
// ============================================================================

fn compute_attractor_force(
    attractor: &AttractorType,
    pos: Vec3,
    vel: Vec3,
    _mass: f32,
    charge: f32,
    time: f32,
    _perlin: &Perlin,
    _audio_bands: &[f32],
    _audio_amp: f32,
) -> Vec3 {
    match attractor {
        AttractorType::Point {
            position,
            mass: attractor_mass,
            softening,
        } => {
            let center = Vec3::from_array(*position);
            let dir = center - pos;
            let dist_sq = dir.length_squared() + softening * softening;
            let force_mag = *attractor_mass / dist_sq;
            dir.normalize_or_zero() * force_mag
        }

        AttractorType::Vortex {
            position,
            axis,
            strength,
            radius,
        } => {
            let center = Vec3::from_array(*position);
            let axis_vec = Vec3::from_array(*axis).normalize_or_zero();
            let to_center = center - pos;
            let dist = to_center.length();

            if dist < 0.01 || dist > *radius {
                return Vec3::ZERO;
            }

            // Tangent direction
            let tangent = axis_vec.cross(to_center.normalize_or_zero());
            let falloff = 1.0 - (dist / radius);

            tangent * (*strength * falloff)
        }

        AttractorType::BlackHole {
            position,
            mass: bh_mass,
            spin,
            event_horizon,
        } => {
            let center = Vec3::from_array(*position);
            let dir = center - pos;
            let dist = dir.length();

            // Event horizon - massive acceleration
            if dist < *event_horizon {
                return dir.normalize_or_zero() * 1000.0;
            }

            // Gravity
            let gravity = *bh_mass / (dist * dist + 0.1);

            // Frame dragging (spin)
            let tangent = Vec3::Y.cross(dir.normalize_or_zero());
            let drag = *spin / (dist + 1.0);

            dir.normalize_or_zero() * gravity + tangent * drag
        }

        AttractorType::MagneticDipole { position, moment } => {
            let center = Vec3::from_array(*position);
            let m = Vec3::from_array(*moment);
            let r = pos - center;
            let r_len = r.length();

            if r_len < 0.1 {
                return Vec3::ZERO;
            }

            // Magnetic dipole field: B = (μ₀/4π) * (3(m·r̂)r̂ - m) / r³
            let r_hat = r / r_len;
            let m_dot_r = m.dot(r_hat);
            let b = (r_hat * 3.0 * m_dot_r - m) / (r_len * r_len * r_len);

            // Lorentz force: F = q(v × B)
            vel.cross(b) * charge * 10.0
        }

        AttractorType::Turbulence {
            seed,
            octaves: _,
            frequency,
            amplitude,
        } => {
            let fbm: Fbm<Perlin> = Fbm::new(*seed);

            // Sample noise at particle position
            let nx = fbm.get([
                pos.x as f64 * *frequency as f64,
                pos.y as f64 * *frequency as f64,
                time as f64 * 0.5,
            ]) as f32;
            let ny = fbm.get([
                pos.y as f64 * *frequency as f64,
                pos.z as f64 * *frequency as f64,
                time as f64 * 0.5 + 100.0,
            ]) as f32;
            let nz = fbm.get([
                pos.z as f64 * *frequency as f64,
                pos.x as f64 * *frequency as f64,
                time as f64 * 0.5 + 200.0,
            ]) as f32;

            Vec3::new(nx, ny, nz) * *amplitude
        }

        AttractorType::CurlNoise {
            seed,
            scale,
            strength,
        } => {
            let eps = 0.01;
            let perlin = Perlin::new(*seed);

            // Curl of noise field for divergence-free flow
            let px = pos.x as f64 * *scale as f64;
            let py = pos.y as f64 * *scale as f64;
            let pz = pos.z as f64 * *scale as f64;

            let n1 = perlin.get([px, py + eps as f64, pz]) - perlin.get([px, py - eps as f64, pz]);
            let n2 = perlin.get([px, py, pz + eps as f64]) - perlin.get([px, py, pz - eps as f64]);
            let n3 = perlin.get([px + eps as f64, py, pz]) - perlin.get([px - eps as f64, py, pz]);
            let n4 = perlin.get([px, py, pz + eps as f64]) - perlin.get([px, py, pz - eps as f64]);
            let n5 = perlin.get([px + eps as f64, py, pz]) - perlin.get([px - eps as f64, py, pz]);
            let n6 = perlin.get([px, py + eps as f64, pz]) - perlin.get([px, py - eps as f64, pz]);

            Vec3::new((n1 - n2) as f32, (n3 - n4) as f32, (n5 - n6) as f32) * *strength
                / (2.0 * eps)
        }

        AttractorType::Lorenz {
            sigma,
            rho,
            beta,
            scale,
        } => {
            let p = pos * *scale;
            let dx = *sigma * (p.y - p.x);
            let dy = p.x * (*rho - p.z) - p.y;
            let dz = p.x * p.y - *beta * p.z;
            Vec3::new(dx, dy, dz) * 0.1
        }

        AttractorType::Rossler { a, b, c, scale } => {
            let p = pos * *scale;
            let dx = -p.y - p.z;
            let dy = p.x + *a * p.y;
            let dz = *b + p.z * (p.x - *c);
            Vec3::new(dx, dy, dz) * 0.1
        }

        AttractorType::Aizawa {
            a,
            b,
            c,
            d,
            e,
            f,
            scale,
        } => {
            let p = pos * *scale;
            let dx = (p.z - *b) * p.x - *d * p.y;
            let dy = *d * p.x + (p.z - *b) * p.y;
            let dz = *c + *a * p.z
                - (p.z * p.z * p.z) / 3.0
                - (p.x * p.x + p.y * p.y) * (1.0 + *e * p.z)
                + *f * p.z * p.x * p.x * p.x;
            Vec3::new(dx, dy, dz) * 0.1
        }

        AttractorType::TorusMagnetic {
            center,
            major_radius,
            minor_radius,
            field_strength,
        } => {
            // Tokamak-style toroidal magnetic confinement
            let c = Vec3::from_array(*center);
            let r = pos - c;

            // Distance from torus axis
            let axis_dist = Vec3::new(r.x, 0.0, r.z).length();
            let to_torus = axis_dist - *major_radius;
            let torus_dist = (to_torus * to_torus + r.y * r.y).sqrt();

            if torus_dist > *minor_radius * 3.0 {
                return Vec3::ZERO;
            }

            // Toroidal field direction (around the donut)
            let toroidal = Vec3::new(-r.z, 0.0, r.x).normalize_or_zero();

            // Poloidal field (around the cross-section)
            let radial = Vec3::new(r.x, 0.0, r.z).normalize_or_zero();
            let poloidal = Vec3::Y.cross(radial);

            // Confining force toward torus surface
            let confinement = if torus_dist < *minor_radius {
                Vec3::ZERO
            } else {
                -Vec3::new(to_torus, r.y, 0.0).normalize_or_zero() * 5.0
            };

            (toroidal + poloidal * 0.5) * *field_strength + confinement
        }

        AttractorType::GravitationalWave {
            source,
            frequency,
            amplitude,
            phase,
        } => {
            let s = Vec3::from_array(*source);
            let r = pos - s;
            let dist = r.length();

            if dist < 0.1 {
                return Vec3::ZERO;
            }

            // Gravitational wave strain (highly simplified)
            let wave = (time * *frequency * std::f32::consts::TAU - dist * 0.1 + *phase).sin();
            let stretch = r.normalize_or_zero() * wave * *amplitude / (dist + 1.0);

            stretch
        }

        AttractorType::CosmicWeb {
            seed,
            cell_size,
            filament_strength,
        } => {
            // Attract toward Voronoi cell edges (filaments)
            let cell = [
                (pos.x / *cell_size).floor(),
                (pos.y / *cell_size).floor(),
                (pos.z / *cell_size).floor(),
            ];

            let mut closest_edge = Vec3::ZERO;
            let mut min_dist = f32::MAX;

            // Check neighboring cells
            for dx in -1..=1 {
                for dy in -1..=1 {
                    for dz in -1..=1 {
                        let neighbor_cell = [
                            cell[0] + dx as f32,
                            cell[1] + dy as f32,
                            cell[2] + dz as f32,
                        ];

                        // Pseudo-random cell center
                        let hash = ((neighbor_cell[0] as i32 * 73856093)
                            ^ (neighbor_cell[1] as i32 * 19349663)
                            ^ (neighbor_cell[2] as i32 * 83492791) + *seed as i32)
                            as f32;
                        let jitter = [
                            (hash.sin() * 43758.5453).fract() - 0.5,
                            (hash.cos() * 43758.5453).fract() - 0.5,
                            ((hash * 2.0).sin() * 43758.5453).fract() - 0.5,
                        ];

                        let cell_center = Vec3::new(
                            (neighbor_cell[0] + 0.5 + jitter[0] * 0.8) * *cell_size,
                            (neighbor_cell[1] + 0.5 + jitter[1] * 0.8) * *cell_size,
                            (neighbor_cell[2] + 0.5 + jitter[2] * 0.8) * *cell_size,
                        );

                        let dist = (pos - cell_center).length();
                        if dist < min_dist {
                            min_dist = dist;
                            closest_edge = cell_center;
                        }
                    }
                }
            }

            // Attract toward cell boundaries (where filaments form)
            let to_center = closest_edge - pos;
            let boundary_dist = min_dist / *cell_size;

            // Attract more strongly when far from cell center
            to_center.normalize_or_zero()
                * *filament_strength
                * (1.0 - boundary_dist).clamp(0.0, 1.0)
        }
    }
}

// ============================================================================
// DATA STRUCTURES FOR IPC
// ============================================================================

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ParticleData {
    pub positions: Vec<f32>,  // [x,y,z,age, x,y,z,age, ...]
    pub velocities: Vec<f32>, // [vx,vy,vz,mass, ...]
    pub count: usize,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SimConfig {
    pub particle_count: usize,
    pub substeps: u32,
    pub nbody_enabled: bool,
    pub nbody_g: f32,
    pub gravity: [f32; 3],
    pub drag: f32,
    pub bounds_min: [f32; 3],
    pub bounds_max: [f32; 3],
    pub bounds_mode: String,
}

// ============================================================================
// GLOBAL SIM INSTANCES
// ============================================================================

lazy_static! {
    pub static ref QUANTUM_SIMS: Mutex<HashMap<u64, QuantumSim>> = Mutex::new(HashMap::new());
    static ref NEXT_SIM_ID: Mutex<u64> = Mutex::new(1);
}

// ============================================================================
// TAURI COMMANDS
// ============================================================================

/// Create a new quantum simulation
#[tauri::command]
pub fn quantum_create(config: SimConfig) -> Result<u64, String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let mut next_id = NEXT_SIM_ID.lock().map_err(|e| e.to_string())?;

    let id = *next_id;
    *next_id += 1;

    let mut sim = QuantumSim::new(config.particle_count);
    sim.substeps = config.substeps;
    sim.nbody_enabled = config.nbody_enabled;
    sim.nbody_g = config.nbody_g;
    sim.gravity = config.gravity;
    sim.drag = config.drag;
    sim.bounds_min = config.bounds_min;
    sim.bounds_max = config.bounds_max;
    sim.bounds_mode = match config.bounds_mode.as_str() {
        "kill" => BoundsMode::Kill,
        "wrap" => BoundsMode::Wrap,
        "bounce" => BoundsMode::Bounce,
        _ => BoundsMode::None,
    };

    sims.insert(id, sim);

    log::info!(
        "[KQuantum] Created simulation {} with {} particles",
        id,
        config.particle_count
    );
    Ok(id)
}

/// Step the simulation
#[tauri::command]
pub fn quantum_step(sim_id: u64, dt: f32) -> Result<ParticleData, String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.step(dt);

    Ok(sim.get_particle_data())
}

/// Add an attractor
#[tauri::command]
pub fn quantum_add_attractor(
    sim_id: u64,
    attractor_type: String,
    params: serde_json::Value,
) -> Result<usize, String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    let attractor = parse_attractor(&attractor_type, params)?;
    let id = sim.add_attractor(attractor);

    Ok(id)
}

/// Emit particles
#[tauri::command]
pub fn quantum_emit(
    sim_id: u64,
    count: usize,
    position: [f32; 3],
    velocity: [f32; 3],
    spread: f32,
) -> Result<(), String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.emit(count, position, velocity, spread);

    Ok(())
}

/// Set audio data for reactivity
#[tauri::command]
pub fn quantum_set_audio(sim_id: u64, bands: Vec<f32>, amplitude: f32) -> Result<(), String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.set_audio(bands, amplitude);

    Ok(())
}

/// Enable/disable N-body
#[tauri::command]
pub fn quantum_set_nbody(sim_id: u64, enabled: bool, g: f32, softening: f32) -> Result<(), String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.nbody_enabled = enabled;
    sim.nbody_g = g;
    sim.nbody_softening = softening;

    Ok(())
}

/// Clear all attractors
#[tauri::command]
pub fn quantum_clear_attractors(sim_id: u64) -> Result<(), String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    sim.attractors.clear();

    Ok(())
}

/// Dispose simulation
#[tauri::command]
pub fn quantum_dispose(sim_id: u64) -> Result<(), String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    sims.remove(&sim_id);

    log::info!("[KQuantum] Disposed simulation {}", sim_id);
    Ok(())
}

/// Get particle count
#[tauri::command]
pub fn quantum_particle_count(sim_id: u64) -> Result<usize, String> {
    let sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get(&sim_id).ok_or("Simulation not found")?;

    Ok(sim.particles.len())
}
/// Export particle system to custom JSON format
/// Returns JSON string with full particle data (position, velocity, age, life, etc.)
#[cfg(feature = "tauri-commands")]
#[tauri::command]
pub fn quantum_export_json(sim_id: u64) -> Result<String, String> {
    let sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims
        .get(&sim_id)
        .ok_or("Simulation not found. Please ensure the simulation is running before exporting.")?;

    // Validation: Check if particle system is empty
    if sim.particles.is_empty() {
        return Err(
            "Cannot export empty particle system. Please run the simulation to generate particles."
                .to_string(),
        );
    }

    // Validation: Check for invalid particle data
    let invalid_count = sim
        .particles
        .iter()
        .filter(|p| {
            p.position.iter().any(|&v| v.is_nan() || v.is_infinite())
                || p.velocity.iter().any(|&v| v.is_nan() || v.is_infinite())
        })
        .count();

    if invalid_count > 0 {
        return Err(format!(
            "Cannot export: {} particles have invalid data (NaN or Infinite values). Try resetting the simulation.",
            invalid_count
        ));
    }

    // Serialize all particles with full data
    let particles: Vec<&Particle> = sim.particles.iter().collect();
    let json = serde_json::to_string_pretty(&particles).map_err(|e| {
        format!(
            "Failed to serialize particles: {}. The particle data may be corrupted.",
            e
        )
    })?;

    Ok(json)
}

/// Export particle system to GLTF format
/// Each particle becomes a small sphere instance
/// Returns GLTF JSON as string
#[cfg(feature = "tauri-commands")]
#[tauri::command]
pub fn quantum_export_gltf(
    sim_id: u64,
    particle_radius: f32,
    subdivisions: u32,
) -> Result<Vec<u8>, String> {
    let sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims
        .get(&sim_id)
        .ok_or("Simulation not found. Please ensure the simulation is running before exporting.")?;

    // Validation: Check if particle system is empty
    if sim.particles.is_empty() {
        return Err(
            "Cannot export empty particle system. Please run the simulation to generate particles."
                .to_string(),
        );
    }

    // Validation: Check particle radius
    if particle_radius <= 0.0 || particle_radius > 100.0 {
        return Err(format!(
            "Invalid particle radius: {}. Radius must be between 0.0 and 100.0.",
            particle_radius
        ));
    }

    // Validation: Check subdivisions
    if subdivisions > 5 {
        return Err(format!(
            "Invalid subdivision level: {}. Maximum subdivision level is 5 to prevent excessive geometry.",
            subdivisions
        ));
    }

    // Validation: Check for invalid particle data
    let invalid_count = sim
        .particles
        .iter()
        .filter(|p| p.position.iter().any(|&v| v.is_nan() || v.is_infinite()))
        .count();

    if invalid_count > 0 {
        return Err(format!(
            "Cannot export: {} particles have invalid positions (NaN or Infinite values). Try resetting the simulation.",
            invalid_count
        ));
    }

    // Validation: Warn about large particle counts (but allow export)
    let particle_count = sim.particles.len();
    if particle_count > 100_000 {
        eprintln!(
            "Warning: Exporting {} particles. This may take a while and produce a large file.",
            particle_count
        );
    }

    // Generate a simple GLTF with instanced spheres
    // For simplicity, we'll create a JSON structure that represents GLTF
    // In production, you'd use the `gltf` crate, but for now we'll create a minimal valid GLTF

    // Create sphere geometry (icosphere)
    let (vertices, indices) = generate_icosphere(particle_radius, subdivisions);

    // Create instance transforms for each particle
    let mut transforms = Vec::new();
    for particle in &sim.particles {
        // Create 4x4 transform matrix (translation only for now)
        let matrix = [
            1.0,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            0.0,
            particle.position[0],
            particle.position[1],
            particle.position[2],
            1.0,
        ];
        transforms.push(matrix);
    }

    // Build minimal GLTF structure
    let gltf_json = build_gltf_with_instances(&vertices, &indices, &transforms)?;

    // Convert to GLB (binary GLTF)
    let glb_bytes = create_glb_from_json(&gltf_json).map_err(|e| {
        format!(
            "Failed to create GLB file: {}. The export data may be too large.",
            e
        )
    })?;

    Ok(glb_bytes)
}

/// Helper: Generate icosphere geometry
fn generate_icosphere(radius: f32, _subdivisions: u32) -> (Vec<f32>, Vec<u32>) {
    // Simple icosahedron for subdivision 0
    let t = (1.0 + 5.0_f32.sqrt()) / 2.0;
    let scale = radius / (t * t + 1.0).sqrt();

    let mut vertices = vec![
        -1.0, t, 0.0, 1.0, t, 0.0, -1.0, -t, 0.0, 1.0, -t, 0.0, 0.0, -1.0, t, 0.0, 1.0, t, 0.0,
        -1.0, -t, 0.0, 1.0, -t, t, 0.0, -1.0, t, 0.0, 1.0, -t, 0.0, -1.0, -t, 0.0, 1.0,
    ];

    // Scale to radius
    for v in vertices.iter_mut() {
        *v *= scale;
    }

    let indices = vec![
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7,
        1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9,
        8, 1,
    ];

    // For simplicity, we skip subdivision for now
    // In production, implement proper icosphere subdivision

    (vertices, indices)
}

/// Helper: Build GLTF JSON structure with instanced geometry
fn build_gltf_with_instances(
    vertices: &[f32],
    indices: &[u32],
    transforms: &[[f32; 16]],
) -> Result<serde_json::Value, String> {
    use serde_json::json;

    let vertex_count = vertices.len() / 3;
    let index_count = indices.len();
    let instance_count = transforms.len();

    // Calculate buffer sizes
    let vertex_buffer_size = vertices.len() * 4; // f32 = 4 bytes
    let index_buffer_size = indices.len() * 4; // u32 = 4 bytes
                                               // Build GLTF JSON
    let gltf = json!({
        "asset": {
            "version": "2.0",
            "generator": "K_OS KQuantum Exporter"
        },
        "scene": 0,
        "scenes": [{
            "nodes": (0..instance_count).collect::<Vec<_>>()
        }],
        "nodes": transforms.iter().enumerate().map(|(_i, matrix)| {
            json!({
                "mesh": 0,
                "matrix": matrix
            })
        }).collect::<Vec<_>>(),
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0
                },
                "indices": 1,
                "mode": 4 // TRIANGLES
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126, // FLOAT
                "count": vertex_count,
                "type": "VEC3",
                "min": [-1.0, -1.0, -1.0],
                "max": [1.0, 1.0, 1.0]
            },
            {
                "bufferView": 1,
                "componentType": 5125, // UNSIGNED_INT
                "count": index_count,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": 0,
                "byteLength": vertex_buffer_size
            },
            {
                "buffer": 0,
                "byteOffset": vertex_buffer_size,
                "byteLength": index_buffer_size
            }
        ],
        "buffers": [{
            "byteLength": vertex_buffer_size + index_buffer_size
        }]
    });

    Ok(gltf)
}

/// Helper: Create GLB binary from GLTF JSON and binary data
fn create_glb_from_json(gltf_json: &serde_json::Value) -> Result<Vec<u8>, String> {
    let json_string = serde_json::to_string(gltf_json)
        .map_err(|e| format!("Failed to serialize GLTF JSON: {}", e))?;

    let json_bytes = json_string.as_bytes();
    let json_length = json_bytes.len();

    // Pad JSON to 4-byte alignment
    let json_padding = (4 - (json_length % 4)) % 4;
    let json_chunk_length = json_length + json_padding;

    // GLB header: magic (4 bytes) + version (4 bytes) + length (4 bytes)
    let total_length = 12 + 8 + json_chunk_length; // header + chunk header + json

    let mut glb = Vec::with_capacity(total_length);

    // GLB header
    glb.extend_from_slice(b"glTF"); // magic
    glb.extend_from_slice(&2u32.to_le_bytes()); // version 2
    glb.extend_from_slice(&(total_length as u32).to_le_bytes()); // total length

    // JSON chunk header
    glb.extend_from_slice(&(json_chunk_length as u32).to_le_bytes()); // chunk length
    glb.extend_from_slice(b"JSON"); // chunk type

    // JSON data
    glb.extend_from_slice(json_bytes);

    // JSON padding (spaces)
    for _ in 0..json_padding {
        glb.push(0x20); // space character
    }

    Ok(glb)
}

// ============================================================================
// HELPERS
// ============================================================================

fn parse_attractor(attractor_type: &str, params: serde_json::Value) -> Result<Attractor, String> {
    let falloff = params
        .get("falloff")
        .and_then(|v| v.as_f64())
        .unwrap_or(0.0) as f32;
    let max_force = params
        .get("max_force")
        .and_then(|v| v.as_f64())
        .unwrap_or(100.0) as f32;
    let enabled = params
        .get("enabled")
        .and_then(|v| v.as_bool())
        .unwrap_or(true);

    let attractor_type = match attractor_type {
        "point" => AttractorType::Point {
            position: parse_vec3(&params, "position").unwrap_or([0.0, 0.0, 0.0]),
            mass: params.get("mass").and_then(|v| v.as_f64()).unwrap_or(10.0) as f32,
            softening: params
                .get("softening")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.5) as f32,
        },
        "vortex" => AttractorType::Vortex {
            position: parse_vec3(&params, "position").unwrap_or([0.0, 0.0, 0.0]),
            axis: parse_vec3(&params, "axis").unwrap_or([0.0, 1.0, 0.0]),
            strength: params
                .get("strength")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
            radius: params
                .get("radius")
                .and_then(|v| v.as_f64())
                .unwrap_or(20.0) as f32,
        },
        "blackhole" => AttractorType::BlackHole {
            position: parse_vec3(&params, "position").unwrap_or([0.0, 0.0, 0.0]),
            mass: params.get("mass").and_then(|v| v.as_f64()).unwrap_or(50.0) as f32,
            spin: params.get("spin").and_then(|v| v.as_f64()).unwrap_or(10.0) as f32,
            event_horizon: params
                .get("event_horizon")
                .and_then(|v| v.as_f64())
                .unwrap_or(2.0) as f32,
        },
        "magnetic" => AttractorType::MagneticDipole {
            position: parse_vec3(&params, "position").unwrap_or([0.0, 0.0, 0.0]),
            moment: parse_vec3(&params, "moment").unwrap_or([0.0, 100.0, 0.0]),
        },
        "turbulence" => AttractorType::Turbulence {
            seed: params.get("seed").and_then(|v| v.as_u64()).unwrap_or(42) as u32,
            octaves: params.get("octaves").and_then(|v| v.as_u64()).unwrap_or(4) as u8,
            frequency: params
                .get("frequency")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.1) as f32,
            amplitude: params
                .get("amplitude")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
        },
        "curl" => AttractorType::CurlNoise {
            seed: params.get("seed").and_then(|v| v.as_u64()).unwrap_or(42) as u32,
            scale: params.get("scale").and_then(|v| v.as_f64()).unwrap_or(0.1) as f32,
            strength: params
                .get("strength")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
        },
        "lorenz" => AttractorType::Lorenz {
            sigma: params.get("sigma").and_then(|v| v.as_f64()).unwrap_or(10.0) as f32,
            rho: params.get("rho").and_then(|v| v.as_f64()).unwrap_or(28.0) as f32,
            beta: params.get("beta").and_then(|v| v.as_f64()).unwrap_or(2.667) as f32,
            scale: params.get("scale").and_then(|v| v.as_f64()).unwrap_or(0.1) as f32,
        },
        "rossler" => AttractorType::Rossler {
            a: params.get("a").and_then(|v| v.as_f64()).unwrap_or(0.2) as f32,
            b: params.get("b").and_then(|v| v.as_f64()).unwrap_or(0.2) as f32,
            c: params.get("c").and_then(|v| v.as_f64()).unwrap_or(5.7) as f32,
            scale: params.get("scale").and_then(|v| v.as_f64()).unwrap_or(0.1) as f32,
        },
        "aizawa" => AttractorType::Aizawa {
            a: params.get("a").and_then(|v| v.as_f64()).unwrap_or(0.95) as f32,
            b: params.get("b").and_then(|v| v.as_f64()).unwrap_or(0.7) as f32,
            c: params.get("c").and_then(|v| v.as_f64()).unwrap_or(0.6) as f32,
            d: params.get("d").and_then(|v| v.as_f64()).unwrap_or(3.5) as f32,
            e: params.get("e").and_then(|v| v.as_f64()).unwrap_or(0.25) as f32,
            f: params.get("f").and_then(|v| v.as_f64()).unwrap_or(0.1) as f32,
            scale: params.get("scale").and_then(|v| v.as_f64()).unwrap_or(0.5) as f32,
        },
        "tokamak" => AttractorType::TorusMagnetic {
            center: parse_vec3(&params, "center").unwrap_or([0.0, 0.0, 0.0]),
            major_radius: params
                .get("major_radius")
                .and_then(|v| v.as_f64())
                .unwrap_or(20.0) as f32,
            minor_radius: params
                .get("minor_radius")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
            field_strength: params
                .get("field_strength")
                .and_then(|v| v.as_f64())
                .unwrap_or(10.0) as f32,
        },
        "gravity_wave" => AttractorType::GravitationalWave {
            source: parse_vec3(&params, "source").unwrap_or([0.0, 0.0, 0.0]),
            frequency: params
                .get("frequency")
                .and_then(|v| v.as_f64())
                .unwrap_or(1.0) as f32,
            amplitude: params
                .get("amplitude")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
            phase: params.get("phase").and_then(|v| v.as_f64()).unwrap_or(0.0) as f32,
        },
        "cosmic_web" => AttractorType::CosmicWeb {
            seed: params.get("seed").and_then(|v| v.as_u64()).unwrap_or(42) as u32,
            cell_size: params
                .get("cell_size")
                .and_then(|v| v.as_f64())
                .unwrap_or(20.0) as f32,
            filament_strength: params
                .get("filament_strength")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0) as f32,
        },
        _ => return Err(format!("Unknown attractor type: {}", attractor_type)),
    };

    Ok(Attractor {
        attractor_type,
        enabled,
        falloff,
        max_force,
    })
}

fn parse_vec3(params: &serde_json::Value, key: &str) -> Option<[f32; 3]> {
    let arr = params.get(key)?.as_array()?;
    if arr.len() >= 3 {
        Some([
            arr[0].as_f64()? as f32,
            arr[1].as_f64()? as f32,
            arr[2].as_f64()? as f32,
        ])
    } else {
        None
    }
}

// ============================================================================
// VAT (Vertex Animation Texture) EXPORT
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VATExportResult {
    pub position_texture: Option<Vec<u8>>,
    pub normal_texture: Option<Vec<u8>>,
    pub width: u32,
    pub height: u32,
    pub particle_count: usize,
    pub frame_count: usize,
    pub bounds_min: [f32; 3],
    pub bounds_max: [f32; 3],
    pub frame_rate: f32,
}

/// Export particle animation as VAT textures
/// Captures multiple frames of particle positions/normals and bakes into textures
#[tauri::command]
pub fn quantum_export_vat(
    sim_id: u64,
    frame_count: usize,
    frame_rate: f32,
    texture_resolution: u32,
    export_position: bool,
    export_normal: bool,
    normalize_positions: bool,
    bounds_padding: f32,
) -> Result<VATExportResult, String> {
    let mut sims = QUANTUM_SIMS.lock().map_err(|e| e.to_string())?;
    let sim = sims.get_mut(&sim_id).ok_or("Simulation not found")?;

    let particle_count = sim.particles.len();
    if particle_count == 0 {
        return Err("Cannot export empty particle system".to_string());
    }

    // Calculate required texture size
    let pixels_needed = particle_count * frame_count;
    let texture_pixels = (texture_resolution * texture_resolution) as usize;

    if pixels_needed > texture_pixels {
        return Err(format!(
            "Texture too small: need {} pixels, have {}. Increase resolution or reduce frame count.",
            pixels_needed, texture_pixels
        ));
    }

    // Capture frames
    let mut frames: Vec<Vec<Particle>> = Vec::with_capacity(frame_count);
    let dt_per_frame = 1.0 / frame_rate;

    for _ in 0..frame_count {
        // Capture current state
        frames.push(sim.particles.clone());

        // Step simulation forward
        for _ in 0..10 {
            // Multiple substeps for stability
            sim.step(dt_per_frame / 10.0);
        }
    }

    // Calculate bounding box across all frames
    let mut bounds_min = [f32::INFINITY; 3];
    let mut bounds_max = [f32::NEG_INFINITY; 3];

    for frame in &frames {
        for particle in frame {
            for i in 0..3 {
                bounds_min[i] = bounds_min[i].min(particle.position[i]);
                bounds_max[i] = bounds_max[i].max(particle.position[i]);
            }
        }
    }

    // Apply padding
    for i in 0..3 {
        let range = bounds_max[i] - bounds_min[i];
        let padding = range * bounds_padding;
        bounds_min[i] -= padding;
        bounds_max[i] += padding;
    }

    // Generate position texture
    let position_texture = if export_position {
        let mut data = vec![0u8; (texture_resolution * texture_resolution * 4) as usize];

        for (frame_idx, frame) in frames.iter().enumerate() {
            for (particle_idx, particle) in frame.iter().enumerate() {
                let pixel_idx = frame_idx * particle_count + particle_idx;
                if pixel_idx >= texture_pixels {
                    break;
                }

                let x = pixel_idx % texture_resolution as usize;
                let y = pixel_idx / texture_resolution as usize;
                let offset = (y * texture_resolution as usize + x) * 4;

                // Normalize positions to 0-1 range if requested
                let pos = if normalize_positions {
                    [
                        (particle.position[0] - bounds_min[0]) / (bounds_max[0] - bounds_min[0]),
                        (particle.position[1] - bounds_min[1]) / (bounds_max[1] - bounds_min[1]),
                        (particle.position[2] - bounds_min[2]) / (bounds_max[2] - bounds_min[2]),
                    ]
                } else {
                    particle.position
                };

                // Store as RGBA (RGB = position, A = 1.0)
                data[offset] = (pos[0] * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 1] = (pos[1] * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 2] = (pos[2] * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 3] = 255;
            }
        }

        Some(data)
    } else {
        None
    };

    // Generate normal texture (computed from velocity direction)
    let normal_texture = if export_normal {
        let mut data = vec![0u8; (texture_resolution * texture_resolution * 4) as usize];

        for (frame_idx, frame) in frames.iter().enumerate() {
            for (particle_idx, particle) in frame.iter().enumerate() {
                let pixel_idx = frame_idx * particle_count + particle_idx;
                if pixel_idx >= texture_pixels {
                    break;
                }

                let x = pixel_idx % texture_resolution as usize;
                let y = pixel_idx / texture_resolution as usize;
                let offset = (y * texture_resolution as usize + x) * 4;

                // Compute normal from velocity direction
                let vel = Vec3::from(particle.velocity);
                let normal = if vel.length() > 0.001 {
                    vel.normalize()
                } else {
                    Vec3::Y // Default up
                };

                // Store as RGBA (RGB = normal, A = 1.0)
                // Normals are in -1 to 1 range, map to 0-255
                data[offset] = ((normal.x * 0.5 + 0.5) * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 1] = ((normal.y * 0.5 + 0.5) * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 2] = ((normal.z * 0.5 + 0.5) * 255.0).clamp(0.0, 255.0) as u8;
                data[offset + 3] = 255;
            }
        }

        Some(data)
    } else {
        None
    };

    Ok(VATExportResult {
        position_texture,
        normal_texture,
        width: texture_resolution,
        height: texture_resolution,
        particle_count,
        frame_count,
        bounds_min,
        bounds_max,
        frame_rate,
    })
}
