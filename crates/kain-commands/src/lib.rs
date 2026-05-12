pub mod blade;
pub mod codebase;
pub mod fabric;
pub mod generated;
pub mod kain;
pub mod omni;
pub mod registry;
pub mod repair;
pub mod runtime;
pub mod selfhost;
pub mod shared;

pub use registry::{
    builtin_command_definitions, builtin_registry, BuiltinCommandArgDefinition,
    BuiltinCommandDefinition, CommandDefinition, CommandRegistry,
};
