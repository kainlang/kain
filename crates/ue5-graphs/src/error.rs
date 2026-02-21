//! Error types for ue5-graphs crate

use thiserror::Error;

pub type Result<T> = std::result::Result<T, GraphError>;

#[derive(Error, Debug)]
pub enum GraphError {
    #[error("AST conversion error: {0}")]
    ASTConversion(String),
    
    #[error("IR validation error: {0}")]
    IRValidation(String),
    
    #[error("Factory generation error: {0}")]
    FactoryGeneration(String),
    
    #[error("Binary serialization error: {0}")]
    BinarySerialization(String),
    
    #[error("Schema building error: {0}")]
    SchemaBuilding(String),
    
    #[error("Node type error: {0}")]
    NodeType(String),
    
    #[error("Pin type error: {0}")]
    PinType(String),
    
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("Serialization error: {0}")]
    Serialization(#[from] serde_json::Error),
}
