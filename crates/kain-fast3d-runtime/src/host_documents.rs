use std::{fs, path::Path};

use serde::Deserialize;

use crate::model::CombineMode;

#[derive(Clone, Debug, Default, Deserialize)]
pub struct Fast3dGameplayStateDocument {
    #[serde(default)]
    pub actor_bindings: Vec<Fast3dActorBindingDefinition>,
}

impl Fast3dGameplayStateDocument {
    pub fn load_from_path(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let text = fs::read_to_string(path)?;
        Ok(serde_json::from_str(&text)?)
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct Fast3dActorBindingDefinition {
    pub display_list_id: String,
    #[serde(default)]
    pub animation: ActorAnimationDefinition,
}

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum ActorAnimationDefinition {
    #[default]
    None,
    SpinY {
        degrees_per_second: f32,
    },
    BobY {
        amplitude: f32,
        cycles_per_second: f32,
        #[serde(default)]
        base_height: f32,
    },
}

#[derive(Clone, Debug, Default, Deserialize)]
pub struct Fast3dShaderOverrideDocument {
    #[serde(default)]
    pub display_list_overrides: Vec<Fast3dDisplayListOverrideDefinition>,
}

impl Fast3dShaderOverrideDocument {
    pub fn load_from_path(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let text = fs::read_to_string(path)?;
        Ok(serde_json::from_str(&text)?)
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct Fast3dDisplayListOverrideDefinition {
    pub display_list_id: String,
    #[serde(default)]
    pub combine_mode: Option<CombineMode>,
    #[serde(default)]
    pub primitive_color: Option<[u8; 4]>,
    #[serde(default)]
    pub env_color: Option<[u8; 4]>,
}
