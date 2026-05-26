//! Deterministic state-coupling primitives for Kain `entangle`.
//!
//! This crate owns the portable policy and endpoint graph. Language meaning
//! stays in `kain-core`; target adapters can consume these descriptors without
//! inventing a second entanglement model.

use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fmt;

pub const STATE_ENTANGLE_CAPABILITY: &str = "state.entangle";
pub const SINGLE_WRITER_POLICY: &str = "single_writer";

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct EntangleEndpointId(String);

impl EntangleEndpointId {
    pub fn new(path: impl Into<String>) -> Self {
        Self(path.into())
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl fmt::Display for EntangleEndpointId {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output.write_str(&self.0)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum EntanglePolicy {
    SingleWriter,
}

impl EntanglePolicy {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::SingleWriter => SINGLE_WRITER_POLICY,
        }
    }

    pub fn from_keyword(value: &str) -> Option<Self> {
        match value {
            SINGLE_WRITER_POLICY => Some(Self::SingleWriter),
            _ => None,
        }
    }
}

impl fmt::Display for EntanglePolicy {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output.write_str(self.as_str())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EntangleBindingDescriptor {
    pub authority: EntangleEndpointId,
    pub mirror: EntangleEndpointId,
    pub policy: EntanglePolicy,
}

impl EntangleBindingDescriptor {
    pub fn single_writer(authority: EntangleEndpointId, mirror: EntangleEndpointId) -> Self {
        Self {
            authority,
            mirror,
            policy: EntanglePolicy::SingleWriter,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EntangleGraphError {
    DuplicateEndpoint(String),
    MirrorWriteDenied { endpoint: String, authority: String },
}

impl fmt::Display for EntangleGraphError {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::DuplicateEndpoint(endpoint) => write!(
                output,
                "entangle endpoint '{endpoint}' participates in more than one binding"
            ),
            Self::MirrorWriteDenied {
                endpoint,
                authority,
            } => write!(
                output,
                "cannot write entangle mirror '{endpoint}' directly under single_writer; write authority '{authority}' instead"
            ),
        }
    }
}

impl std::error::Error for EntangleGraphError {}

#[derive(Debug, Clone)]
pub struct EntangleGraph {
    bindings: Vec<EntangleBindingDescriptor>,
    mirrors_by_authority: BTreeMap<EntangleEndpointId, Vec<EntangleEndpointId>>,
    authority_by_mirror: BTreeMap<EntangleEndpointId, EntangleEndpointId>,
    claimed_endpoints: BTreeSet<EntangleEndpointId>,
}

impl Default for EntangleGraph {
    fn default() -> Self {
        Self {
            bindings: Vec::new(),
            mirrors_by_authority: BTreeMap::new(),
            authority_by_mirror: BTreeMap::new(),
            claimed_endpoints: BTreeSet::new(),
        }
    }
}

impl EntangleGraph {
    pub fn register(
        &mut self,
        binding: EntangleBindingDescriptor,
    ) -> Result<(), EntangleGraphError> {
        self.ensure_endpoint_unclaimed(&binding.authority)?;
        self.ensure_endpoint_unclaimed(&binding.mirror)?;
        if binding.authority == binding.mirror {
            return Err(EntangleGraphError::DuplicateEndpoint(
                binding.authority.to_string(),
            ));
        }
        self.claimed_endpoints.insert(binding.authority.clone());
        self.claimed_endpoints.insert(binding.mirror.clone());

        match binding.policy {
            EntanglePolicy::SingleWriter => {
                self.mirrors_by_authority
                    .entry(binding.authority.clone())
                    .or_default()
                    .push(binding.mirror.clone());
                self.authority_by_mirror
                    .insert(binding.mirror.clone(), binding.authority.clone());
            }
        }

        self.bindings.push(binding);
        Ok(())
    }

    pub fn mirrors_for_authority(&self, endpoint: &str) -> Vec<String> {
        self.mirrors_by_authority
            .get(&EntangleEndpointId::new(endpoint))
            .map(|mirrors| mirrors.iter().map(ToString::to_string).collect())
            .unwrap_or_default()
    }

    pub fn ensure_write_allowed(&self, endpoint: &str) -> Result<(), EntangleGraphError> {
        if let Some(authority) = self
            .authority_by_mirror
            .get(&EntangleEndpointId::new(endpoint))
        {
            return Err(EntangleGraphError::MirrorWriteDenied {
                endpoint: endpoint.to_string(),
                authority: authority.to_string(),
            });
        }
        Ok(())
    }

    pub fn bindings(&self) -> &[EntangleBindingDescriptor] {
        &self.bindings
    }

    fn ensure_endpoint_unclaimed(
        &self,
        endpoint: &EntangleEndpointId,
    ) -> Result<(), EntangleGraphError> {
        if self.claimed_endpoints.contains(endpoint) {
            return Err(EntangleGraphError::DuplicateEndpoint(endpoint.to_string()));
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_writer_graph_reports_mirrors_and_blocks_mirror_writes() {
        let mut graph = EntangleGraph::default();
        graph
            .register(EntangleBindingDescriptor::single_writer(
                EntangleEndpointId::new("Physics.health"),
                EntangleEndpointId::new("Ui.health"),
            ))
            .expect("register binding");

        assert_eq!(
            graph.mirrors_for_authority("Physics.health"),
            vec!["Ui.health".to_string()]
        );
        assert!(graph.ensure_write_allowed("Physics.health").is_ok());
        assert!(matches!(
            graph.ensure_write_allowed("Ui.health"),
            Err(EntangleGraphError::MirrorWriteDenied { .. })
        ));
    }

    #[test]
    fn graph_rejects_duplicate_endpoint_claims() {
        let mut graph = EntangleGraph::default();
        graph
            .register(EntangleBindingDescriptor::single_writer(
                EntangleEndpointId::new("A.x"),
                EntangleEndpointId::new("B.x"),
            ))
            .expect("first binding");

        let error = graph
            .register(EntangleBindingDescriptor::single_writer(
                EntangleEndpointId::new("A.x"),
                EntangleEndpointId::new("C.x"),
            ))
            .expect_err("duplicate endpoint should fail");
        assert_eq!(
            error.to_string(),
            "entangle endpoint 'A.x' participates in more than one binding"
        );
    }

    #[test]
    fn graph_rejects_self_entanglement() {
        let mut graph = EntangleGraph::default();
        let error = graph
            .register(EntangleBindingDescriptor::single_writer(
                EntangleEndpointId::new("A.x"),
                EntangleEndpointId::new("A.x"),
            ))
            .expect_err("self entanglement should fail");

        assert_eq!(
            error.to_string(),
            "entangle endpoint 'A.x' participates in more than one binding"
        );
    }
}
