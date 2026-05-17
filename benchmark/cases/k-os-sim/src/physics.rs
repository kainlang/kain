//! K_OS Physics Engine - Rapier3D Integration
//!
//! High-performance rigid body physics for the entire K_OS ecosystem.
//! Used by: KPainter (fluid interactions), KScatter (collision detection),
//!          KSculpt (soft body potential), KGraphos (physics-based animation)

use lazy_static::lazy_static;
use rapier3d::prelude::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::Mutex;

// ============================================================================
// PHYSICS WORLD STATE
// ============================================================================

lazy_static! {
    static ref PHYSICS_WORLDS: Mutex<HashMap<u64, PhysicsWorld>> = Mutex::new(HashMap::new());
    static ref NEXT_WORLD_ID: Mutex<u64> = Mutex::new(1);
}

/// Complete physics simulation state
pub struct PhysicsWorld {
    pub rigid_body_set: RigidBodySet,
    pub collider_set: ColliderSet,
    pub gravity: Vector<f32>,
    pub integration_parameters: IntegrationParameters,
    pub physics_pipeline: PhysicsPipeline,
    pub island_manager: IslandManager,
    pub broad_phase: DefaultBroadPhase,
    pub narrow_phase: NarrowPhase,
    pub impulse_joint_set: ImpulseJointSet,
    pub multibody_joint_set: MultibodyJointSet,
    pub ccd_solver: CCDSolver,
    pub query_pipeline: QueryPipeline,
    // Handle mappings
    body_handles: HashMap<u64, RigidBodyHandle>,
    collider_handles: HashMap<u64, ColliderHandle>,
    next_body_id: u64,
    next_collider_id: u64,
}

impl PhysicsWorld {
    pub fn new(gravity_y: f32) -> Self {
        Self {
            rigid_body_set: RigidBodySet::new(),
            collider_set: ColliderSet::new(),
            gravity: vector![0.0, gravity_y, 0.0],
            integration_parameters: IntegrationParameters::default(),
            physics_pipeline: PhysicsPipeline::new(),
            island_manager: IslandManager::new(),
            broad_phase: DefaultBroadPhase::new(),
            narrow_phase: NarrowPhase::new(),
            impulse_joint_set: ImpulseJointSet::new(),
            multibody_joint_set: MultibodyJointSet::new(),
            ccd_solver: CCDSolver::new(),
            query_pipeline: QueryPipeline::new(),
            body_handles: HashMap::new(),
            collider_handles: HashMap::new(),
            next_body_id: 1,
            next_collider_id: 1,
        }
    }

    /// Step the physics simulation
    pub fn step(&mut self) {
        self.physics_pipeline.step(
            &self.gravity,
            &self.integration_parameters,
            &mut self.island_manager,
            &mut self.broad_phase,
            &mut self.narrow_phase,
            &mut self.rigid_body_set,
            &mut self.collider_set,
            &mut self.impulse_joint_set,
            &mut self.multibody_joint_set,
            &mut self.ccd_solver,
            Some(&mut self.query_pipeline),
            &(),
            &(),
        );
    }

    /// Add a dynamic rigid body (e.g., a particle, movable object)
    pub fn add_dynamic_body(&mut self, position: [f32; 3], radius: f32) -> u64 {
        let rigid_body = RigidBodyBuilder::dynamic()
            .translation(vector![position[0], position[1], position[2]])
            .build();

        let body_handle = self.rigid_body_set.insert(rigid_body);

        let collider = ColliderBuilder::ball(radius)
            .restitution(0.7)
            .friction(0.3)
            .build();

        self.collider_set
            .insert_with_parent(collider, body_handle, &mut self.rigid_body_set);

        let id = self.next_body_id;
        self.body_handles.insert(id, body_handle);
        self.next_body_id += 1;
        id
    }

    /// Add a static collider (e.g., the mesh being painted)
    pub fn add_static_mesh(&mut self, vertices: Vec<[f32; 3]>, indices: Vec<[u32; 3]>) -> u64 {
        let points: Vec<Point<f32>> = vertices.iter().map(|v| point![v[0], v[1], v[2]]).collect();

        let indices_u32: Vec<[u32; 3]> = indices;

        let collider = ColliderBuilder::trimesh(points, indices_u32)
            .friction(0.5)
            .build();

        let handle = self.collider_set.insert(collider);

        let id = self.next_collider_id;
        self.collider_handles.insert(id, handle);
        self.next_collider_id += 1;
        id
    }

    /// Get all body positions (for sending back to frontend)
    pub fn get_body_positions(&self) -> Vec<(u64, [f32; 3])> {
        let mut positions = Vec::new();
        for (id, handle) in &self.body_handles {
            if let Some(body) = self.rigid_body_set.get(*handle) {
                let pos = body.translation();
                positions.push((*id, [pos.x, pos.y, pos.z]));
            }
        }
        positions
    }

    /// Apply impulse to a body (e.g., from brush stroke)
    #[allow(dead_code)] // Public API - will be exposed as Tauri command
    pub fn apply_impulse(&mut self, body_id: u64, impulse: [f32; 3]) {
        if let Some(handle) = self.body_handles.get(&body_id) {
            if let Some(body) = self.rigid_body_set.get_mut(*handle) {
                body.apply_impulse(vector![impulse[0], impulse[1], impulse[2]], true);
            }
        }
    }

    /// Apply radial force (like black hole gravity)
    pub fn apply_radial_force(&mut self, center: [f32; 3], strength: f32, radius: f32) {
        let center_vec = vector![center[0], center[1], center[2]];

        for (_, handle) in &self.body_handles {
            if let Some(body) = self.rigid_body_set.get_mut(*handle) {
                let pos = *body.translation();
                let dir = center_vec - pos;
                let dist = dir.magnitude();

                if dist < radius && dist > 0.001 {
                    // Inverse square law with falloff
                    let force_magnitude = strength / (dist * dist + 0.1);
                    let force = dir.normalize() * force_magnitude;
                    body.apply_impulse(force, true);
                }
            }
        }
    }
}

// ============================================================================
// TAURI COMMANDS
// ============================================================================

#[derive(Serialize, Deserialize)]
pub struct BodyState {
    pub id: u64,
    pub position: [f32; 3],
}

/// Create a new physics world
#[tauri::command]
pub fn create_physics_world(gravity_y: f32) -> Result<u64, String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    let mut next_id = NEXT_WORLD_ID.lock().map_err(|e| e.to_string())?;

    let id = *next_id;
    *next_id += 1;

    let world = PhysicsWorld::new(gravity_y);
    worlds.insert(id, world);

    log::info!(
        "[K_OS Physics] Created world {} with gravity {}",
        id,
        gravity_y
    );
    Ok(id)
}

/// Step physics simulation
#[tauri::command]
pub fn step_physics(world_id: u64) -> Result<Vec<BodyState>, String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    let world = worlds.get_mut(&world_id).ok_or("World not found")?;

    world.step();

    let positions = world.get_body_positions();
    Ok(positions
        .into_iter()
        .map(|(id, pos)| BodyState { id, position: pos })
        .collect())
}

/// Add a dynamic body to the physics world
#[tauri::command]
pub fn add_physics_body(world_id: u64, position: [f32; 3], radius: f32) -> Result<u64, String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    let world = worlds.get_mut(&world_id).ok_or("World not found")?;

    Ok(world.add_dynamic_body(position, radius))
}

/// Add a static mesh collider
#[tauri::command]
pub fn add_physics_mesh(
    world_id: u64,
    vertices: Vec<[f32; 3]>,
    indices: Vec<[u32; 3]>,
) -> Result<u64, String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    let world = worlds.get_mut(&world_id).ok_or("World not found")?;

    Ok(world.add_static_mesh(vertices, indices))
}

/// Apply black hole gravity effect
#[tauri::command]
pub fn apply_black_hole_gravity(
    world_id: u64,
    center: [f32; 3],
    strength: f32,
    radius: f32,
) -> Result<(), String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    let world = worlds.get_mut(&world_id).ok_or("World not found")?;

    world.apply_radial_force(center, strength, radius);
    Ok(())
}

/// Clean up physics world
#[tauri::command]
pub fn dispose_physics_world(world_id: u64) -> Result<(), String> {
    let mut worlds = PHYSICS_WORLDS.lock().map_err(|e| e.to_string())?;
    worlds.remove(&world_id);
    log::info!("[K_OS Physics] Disposed world {}", world_id);
    Ok(())
}
