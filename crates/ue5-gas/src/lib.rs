pub mod tags_ir;
pub mod tags_codegen;
pub mod attribute_set_ir;
pub mod attribute_set_codegen;

pub use tags_ir::*;
pub use tags_codegen::generate as generate_tags;
pub use attribute_set_ir::*;
pub use attribute_set_codegen::generate as generate_attribute_set;
