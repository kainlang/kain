pub mod tags_ir;
pub mod tags_codegen;
pub mod attribute_set_ir;
pub mod attribute_set_codegen;
pub mod ability_ir;
pub mod ability_codegen;
pub mod effect_ir;
pub mod effect_codegen;

pub use tags_ir::*;
pub use tags_codegen::generate as generate_tags;
pub use attribute_set_ir::*;
pub use attribute_set_codegen::generate as generate_attribute_set;
pub use ability_ir::*;
pub use ability_codegen::generate as generate_ability;
pub use effect_ir::*;
pub use effect_codegen::generate as generate_effect;
