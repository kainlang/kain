use crate::address::{ActorName, ActorPath};
use crate::id::ActorId;
use crate::runtime::ActorRegistryEntry;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorNameBinding {
    pub name: ActorName,
    pub id: ActorId,
    pub path: ActorPath,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRegistryModel {
    pub entries: Vec<ActorRegistryEntry>,
    pub names: Vec<ActorNameBinding>,
}

impl ActorRegistryModel {
    pub fn find_entry(&self, id: ActorId) -> Option<&ActorRegistryEntry> {
        self.entries.iter().find(|entry| entry.id == id)
    }

    pub fn find_named(&self, name: &str) -> Option<&ActorNameBinding> {
        self.names
            .iter()
            .find(|binding| binding.name.as_str() == name)
    }

    pub fn bind_name(&mut self, binding: ActorNameBinding) {
        self.names.retain(|existing| existing.name != binding.name);
        self.names.push(binding);
    }
}
