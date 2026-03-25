use thiserror::Error;

#[derive(Debug, Error)]
pub enum BlueprintError {
    #[error("Blueprint IR error: {0}")]
    Ir(String),

    #[error("Asset write error: {0}")]
    AssetWrite(String),

    #[error("Unsupported node type: {0}")]
    UnsupportedNode(String),

    #[error("Missing required field: {0}")]
    MissingField(String),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}

pub type Result<T> = std::result::Result<T, BlueprintError>;
