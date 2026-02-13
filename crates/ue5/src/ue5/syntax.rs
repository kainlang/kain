//! UE5 Macro Builders
//! 
//! Abstract helpers for UFUNCTION, UPROPERTY, UCLASS, etc.
//! Instead of manual string formatting, use fluent builders.

/// Builder for UPROPERTY macro
#[derive(Debug, Clone, Default)]
pub struct PropertyBuilder {
    specifiers: Vec<String>,
    meta_tags: Vec<String>,
    category: Option<String>,
}

impl PropertyBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn edit_anywhere(mut self) -> Self {
        self.specifiers.push("EditAnywhere".to_string());
        self
    }

    pub fn edit_defaults_only(mut self) -> Self {
        self.specifiers.push("EditDefaultsOnly".to_string());
        self
    }

    pub fn visible_anywhere(mut self) -> Self {
        self.specifiers.push("VisibleAnywhere".to_string());
        self
    }

    pub fn blueprint_read_write(mut self) -> Self {
        self.specifiers.push("BlueprintReadWrite".to_string());
        self
    }

    pub fn blueprint_read_only(mut self) -> Self {
        self.specifiers.push("BlueprintReadOnly".to_string());
        self
    }

    pub fn replicated(mut self) -> Self {
        self.specifiers.push("Replicated".to_string());
        self
    }

    pub fn transient(mut self) -> Self {
        self.specifiers.push("Transient".to_string());
        self
    }

    pub fn savegame(mut self) -> Self {
        self.specifiers.push("SaveGame".to_string());
        self
    }

    pub fn blueprint_assignable(mut self) -> Self {
        self.specifiers.push("BlueprintAssignable".to_string());
        self
    }

    pub fn category(mut self, cat: impl Into<String>) -> Self {
        self.category = Some(cat.into());
        self
    }

    pub fn display_name(mut self, name: impl Into<String>) -> Self {
        self.meta_tags.push(format!("DisplayName = \"{}\"", name.into()));
        self
    }

    pub fn tooltip(mut self, tip: impl Into<String>) -> Self {
        self.meta_tags.push(format!("ToolTip = \"{}\"", tip.into()));
        self
    }

    pub fn clamp_min(mut self, min: impl std::fmt::Display) -> Self {
        self.meta_tags.push(format!("ClampMin = \"{}\"", min));
        self
    }

    pub fn clamp_max(mut self, max: impl std::fmt::Display) -> Self {
        self.meta_tags.push(format!("ClampMax = \"{}\"", max));
        self
    }

    pub fn units(mut self, unit: impl Into<String>) -> Self {
        self.meta_tags.push(format!("Units = \"{}\"", unit.into()));
        self
    }

    pub fn build(self) -> String {
        let mut parts = self.specifiers;
        
        if let Some(cat) = self.category {
            parts.push(format!("Category = \"{}\"", cat));
        }
        
        if !self.meta_tags.is_empty() {
            parts.push(format!("meta = ({})", self.meta_tags.join(", ")));
        }
        
        format!("UPROPERTY({})", parts.join(", "))
    }
}

/// Builder for UFUNCTION macro
#[derive(Debug, Clone, Default)]
pub struct FunctionBuilder {
    specifiers: Vec<String>,
    category: Option<String>,
    meta_tags: Vec<String>,
}

impl FunctionBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn blueprint_callable(mut self) -> Self {
        self.specifiers.push("BlueprintCallable".to_string());
        self
    }

    pub fn blueprint_pure(mut self) -> Self {
        self.specifiers.push("BlueprintPure".to_string());
        self
    }

    pub fn server(mut self) -> Self {
        self.specifiers.push("Server".to_string());
        self
    }

    pub fn client(mut self) -> Self {
        self.specifiers.push("Client".to_string());
        self
    }

    pub fn net_multicast(mut self) -> Self {
        self.specifiers.push("NetMulticast".to_string());
        self
    }

    pub fn reliable(mut self) -> Self {
        self.specifiers.push("Reliable".to_string());
        self
    }

    pub fn category(mut self, cat: impl Into<String>) -> Self {
        self.category = Some(cat.into());
        self
    }

    pub fn build(self) -> String {
        let mut parts = self.specifiers;
        
        if let Some(cat) = self.category {
            parts.push(format!("Category = \"{}\"", cat));
        }
        
        if !self.meta_tags.is_empty() {
            parts.push(format!("meta = ({})", self.meta_tags.join(", ")));
        }
        
        format!("UFUNCTION({})", parts.join(", "))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_property_builder() {
        let prop = PropertyBuilder::new()
            .edit_anywhere()
            .blueprint_read_write()
            .category("Test")
            .clamp_min(0)
            .clamp_max(100)
            .build();
        
        assert!(prop.contains("EditAnywhere"));
        assert!(prop.contains("BlueprintReadWrite"));
        assert!(prop.contains("Category = \"Test\""));
        assert!(prop.contains("ClampMin = \"0\""));
    }

    #[test]
    fn test_function_builder() {
        let func = FunctionBuilder::new()
            .blueprint_callable()
            .category("MyCategory")
            .build();
        
        assert!(func.contains("BlueprintCallable"));
        assert!(func.contains("Category = \"MyCategory\""));
    }
}
