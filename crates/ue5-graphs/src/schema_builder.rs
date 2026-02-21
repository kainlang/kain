//! Graph Schema Builder
//!
//! Builds graph schemas with connection rules and validation

use crate::{ConnectionRule, GraphSchema, PinType, Result};

pub struct SchemaBuilder {
    schema: GraphSchema,
}

impl SchemaBuilder {
    /// Create a new schema builder
    pub fn new() -> Self {
        Self {
            schema: GraphSchema::default(),
        }
    }
    
    /// Allow connection between two pin types
    pub fn allow_connection(
        mut self,
        from: PinType,
        to: PinType,
    ) -> Self {
        self.schema.allowed_connections.push(ConnectionRule {
            from,
            to,
            allowed: true,
            error_message: None,
        });
        self
    }
    
    /// Disallow connection between two pin types
    pub fn disallow_connection(
        mut self,
        from: PinType,
        to: PinType,
        error_message: impl Into<String>,
    ) -> Self {
        self.schema.allowed_connections.push(ConnectionRule {
            from,
            to,
            allowed: false,
            error_message: Some(error_message.into()),
        });
        self
    }
    
    /// Build the schema
    pub fn build(self) -> GraphSchema {
        self.schema
    }
}

impl Default for SchemaBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_schema_builder() {
        let schema = SchemaBuilder::new()
            .allow_connection(PinType::Exec, PinType::Exec)
            .allow_connection(PinType::Float, PinType::Float)
            .disallow_connection(
                PinType::Exec,
                PinType::Float,
                "Cannot connect execution to data"
            )
            .build();
        
        assert!(schema.allowed_connections.len() >= 3);
    }
}
