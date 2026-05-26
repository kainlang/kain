use thiserror::Error;

pub type AsmResult<T> = Result<T, AsmError>;

#[derive(Debug, Error)]
pub enum AsmError {
    #[error("{0}")]
    Message(String),
    #[error(transparent)]
    Io(#[from] std::io::Error),
}

impl AsmError {
    pub fn runtime(message: impl Into<String>) -> Self {
        Self::Message(message.into())
    }
}
