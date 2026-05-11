use std::collections::BTreeMap;

use crate::authoring::{Geometry, GeometryError};
use crate::scene::Mesh;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AuthoredPrimitiveError {
    EmptyPrimitiveId,
    EmptyRegistryId,
    DuplicatePrimitiveId(String),
    MissingStartupPrimitive(String),
    Geometry(GeometryError),
}

impl std::fmt::Display for AuthoredPrimitiveError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EmptyPrimitiveId => write!(f, "authored primitive id cannot be empty"),
            Self::EmptyRegistryId => write!(f, "authored primitive registry id cannot be empty"),
            Self::DuplicatePrimitiveId(id) => {
                write!(f, "authored primitive registry already contains `{id}`")
            }
            Self::MissingStartupPrimitive(id) => {
                write!(f, "startup primitive `{id}` is not present in the registry")
            }
            Self::Geometry(error) => write!(f, "authored primitive geometry is invalid: {error}"),
        }
    }
}

impl std::error::Error for AuthoredPrimitiveError {}

impl From<GeometryError> for AuthoredPrimitiveError {
    fn from(value: GeometryError) -> Self {
        Self::Geometry(value)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct AuthoredPrimitive {
    pub id: String,
    pub display_name: Option<String>,
    pub resource_uri: Option<String>,
    pub geometry: Geometry,
    pub metadata: BTreeMap<String, String>,
}

impl AuthoredPrimitive {
    pub fn new(id: impl Into<String>, geometry: Geometry) -> Result<Self, AuthoredPrimitiveError> {
        let primitive = Self {
            id: id.into(),
            display_name: None,
            resource_uri: None,
            geometry,
            metadata: BTreeMap::new(),
        };
        primitive.validate()?;
        Ok(primitive)
    }

    pub fn with_display_name(mut self, display_name: impl Into<String>) -> Self {
        self.display_name = Some(display_name.into());
        self
    }

    pub fn with_resource_uri(mut self, resource_uri: impl Into<String>) -> Self {
        self.resource_uri = Some(resource_uri.into());
        self
    }

    pub fn with_metadata_entry(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.metadata.insert(key.into(), value.into());
        self
    }

    pub fn display_label(&self) -> &str {
        self.display_name.as_deref().unwrap_or(&self.id)
    }

    pub fn validate(&self) -> Result<(), AuthoredPrimitiveError> {
        if self.id.trim().is_empty() {
            return Err(AuthoredPrimitiveError::EmptyPrimitiveId);
        }
        self.geometry.validate()?;
        Ok(())
    }

    pub fn to_geometry(&self) -> Result<Geometry, AuthoredPrimitiveError> {
        self.validate()?;
        Ok(self.geometry.clone())
    }

    pub fn to_mesh(&self) -> Result<Mesh, AuthoredPrimitiveError> {
        Ok(self.to_geometry()?.to_mesh()?)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct AuthoredPrimitiveRegistry {
    pub id: String,
    pub resource_document_uri: Option<String>,
    pub startup_primitive_id: Option<String>,
    pub primitives: BTreeMap<String, AuthoredPrimitive>,
    pub metadata: BTreeMap<String, String>,
}

impl AuthoredPrimitiveRegistry {
    pub fn new(id: impl Into<String>) -> Result<Self, AuthoredPrimitiveError> {
        let registry = Self {
            id: id.into(),
            resource_document_uri: None,
            startup_primitive_id: None,
            primitives: BTreeMap::new(),
            metadata: BTreeMap::new(),
        };
        registry.validate()?;
        Ok(registry)
    }

    pub fn from_primitives(
        id: impl Into<String>,
        primitives: impl IntoIterator<Item = AuthoredPrimitive>,
    ) -> Result<Self, AuthoredPrimitiveError> {
        let mut registry = Self::new(id)?;
        for primitive in primitives {
            registry.insert(primitive)?;
        }
        Ok(registry)
    }

    pub fn with_resource_document_uri(mut self, resource_uri: impl Into<String>) -> Self {
        self.resource_document_uri = Some(resource_uri.into());
        self
    }

    pub fn with_startup_primitive_id(mut self, primitive_id: impl Into<String>) -> Self {
        self.startup_primitive_id = Some(primitive_id.into());
        self
    }

    pub fn with_metadata_entry(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.metadata.insert(key.into(), value.into());
        self
    }

    pub fn insert(
        &mut self,
        primitive: AuthoredPrimitive,
    ) -> Result<&mut Self, AuthoredPrimitiveError> {
        primitive.validate()?;
        if self.primitives.contains_key(&primitive.id) {
            return Err(AuthoredPrimitiveError::DuplicatePrimitiveId(primitive.id));
        }
        self.primitives.insert(primitive.id.clone(), primitive);
        Ok(self)
    }

    pub fn primitive(&self, id: &str) -> Option<&AuthoredPrimitive> {
        self.primitives.get(id)
    }

    pub fn iter(&self) -> impl Iterator<Item = &AuthoredPrimitive> {
        self.primitives.values()
    }

    pub fn ids(&self) -> impl Iterator<Item = &String> {
        self.primitives.keys()
    }

    pub fn len(&self) -> usize {
        self.primitives.len()
    }

    pub fn is_empty(&self) -> bool {
        self.primitives.is_empty()
    }

    pub fn validate(&self) -> Result<(), AuthoredPrimitiveError> {
        if self.id.trim().is_empty() {
            return Err(AuthoredPrimitiveError::EmptyRegistryId);
        }
        if let Some(startup_id) = &self.startup_primitive_id {
            if !self.primitives.contains_key(startup_id) {
                return Err(AuthoredPrimitiveError::MissingStartupPrimitive(
                    startup_id.clone(),
                ));
            }
        }
        for primitive in self.primitives.values() {
            primitive.validate()?;
        }
        Ok(())
    }

    pub fn summary(&self) -> String {
        let startup = self.startup_primitive_id.as_deref().unwrap_or("none");
        format!(
            "authored primitive registry `{}`: {} authored meshes, startup {}",
            self.id,
            self.primitives.len(),
            startup
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{Geometry, Vec3};

    fn authored_triangle_geometry() -> Geometry {
        Geometry::triangle_mesh()
            .with_positions(vec![
                Vec3::new(-1.0, 0.0, 0.0),
                Vec3::new(1.0, 0.0, 0.0),
                Vec3::new(0.0, 1.0, 0.0),
            ])
            .with_indices(vec![0, 1, 2])
    }

    #[test]
    fn authored_primitive_wraps_explicit_kain_owned_geometry() {
        let primitive = AuthoredPrimitive::new("authored-triangle", authored_triangle_geometry())
            .expect("authored geometry should validate")
            .with_display_name("Authored Triangle")
            .with_resource_uri("kain://scene/primitives/authored-triangle")
            .with_metadata_entry("owner", "kain-source");

        let mesh = primitive.to_mesh().expect("primitive should become mesh");

        assert_eq!(primitive.display_label(), "Authored Triangle");
        assert_eq!(
            primitive.metadata.get("owner"),
            Some(&"kain-source".to_string())
        );
        assert_eq!(mesh.vertices.len(), 3);
        assert_eq!(mesh.triangles, vec![[0, 1, 2]]);
    }

    #[test]
    fn authored_primitive_registry_is_empty_until_source_adds_meshes() {
        let registry =
            AuthoredPrimitiveRegistry::new("source-owned").expect("registry id should validate");

        assert!(registry.is_empty());
        assert_eq!(
            registry.summary(),
            "authored primitive registry `source-owned`: 0 authored meshes, startup none"
        );
    }

    #[test]
    fn authored_primitive_registry_validates_identity_contracts() {
        let primitive = AuthoredPrimitive::new("authored-triangle", authored_triangle_geometry())
            .expect("authored geometry should validate");
        let mut registry = AuthoredPrimitiveRegistry::new("source-owned")
            .expect("registry id should validate")
            .with_startup_primitive_id("authored-triangle");

        registry
            .insert(primitive.clone())
            .expect("first insert should succeed");
        let duplicate = registry
            .insert(primitive)
            .expect_err("duplicate ids should be rejected");

        assert_eq!(
            duplicate,
            AuthoredPrimitiveError::DuplicatePrimitiveId("authored-triangle".to_string())
        );
        registry.validate().expect("startup id is authored");
    }
}
