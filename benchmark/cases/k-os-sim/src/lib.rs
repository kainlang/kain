//! K_OS Simulation Domain
//!
//! This crate is the ownership boundary for simulation logic.

pub mod cfd;
pub mod fluid;
pub mod physics;
pub mod quantum;

pub use cfd::{
    cfd_add_source, cfd_create, cfd_dispose, cfd_get_density_field, cfd_get_velocity_field,
    cfd_step,
};
pub use fluid::{create_fluid_sim, dispose_fluid_sim, step_fluid_sim, FLUID_SIMS};
pub use physics::{create_physics_world, dispose_physics_world, step_physics};
pub use quantum::{quantum_add_attractor, quantum_create, quantum_step};
