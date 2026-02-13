//! Slate Widget Registry
//!
//! A data-driven database of all Slate widget classes, their properties,
//! events (with native delegate types), and slot configurations.
//! Loaded from `unreal/metadata/widget_registry.json` which is generated
//! by the corpus extractor scanning UE5 engine and plugin headers.
//!
//! This replaces hardcoded delegate mappings in slate.rs with queries
//! against real data extracted from actual Slate widget headers.

use std::collections::HashMap;
use serde::{Deserialize, Serialize};

// ═══════════════════════════════════════════════════════════════════
// Schema Types (mirrors widget_registry.json structure)
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetProperty {
    #[serde(rename = "type")]
    pub prop_type: String,
    #[serde(default)]
    pub kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetEvent {
    pub delegate_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetSlot {
    pub name: String,
    #[serde(default)]
    pub kind: String,
    #[serde(default)]
    pub slot_class: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetInfo {
    pub name: String,
    #[serde(default)]
    pub parent: String,
    #[serde(default)]
    pub header: String,
    #[serde(default)]
    pub properties: HashMap<String, WidgetProperty>,
    #[serde(default)]
    pub events: HashMap<String, WidgetEvent>,
    #[serde(default)]
    pub slots: Vec<WidgetSlot>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DelegateInfo {
    pub name: String,
    #[serde(default)]
    pub return_type: String,
    #[serde(default)]
    pub params: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetRegistryData {
    #[serde(default)]
    pub widgets: HashMap<String, WidgetInfo>,
    #[serde(default)]
    pub delegates: HashMap<String, DelegateInfo>,
}

// ═══════════════════════════════════════════════════════════════════
// Widget Registry — the queryable runtime database
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone)]
pub struct WidgetRegistry {
    pub widgets: HashMap<String, WidgetInfo>,
    pub delegates: HashMap<String, DelegateInfo>,
    /// Reverse map: event name -> native delegate type (across all widgets)
    /// Built from scanning all widget events to find the most common delegate
    /// type for each event name (e.g. "OnClicked" -> "FOnClicked")
    event_delegate_map: HashMap<String, String>,
}

impl Default for WidgetRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl WidgetRegistry {
    pub fn new() -> Self {
        Self {
            widgets: HashMap::new(),
            delegates: HashMap::new(),
            event_delegate_map: HashMap::new(),
        }
    }

    /// Load widget registry from JSON data
    pub fn load(&mut self, json_data: &str) -> Result<(), String> {
        let data: WidgetRegistryData = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse widget registry: {}", e))?;

        self.widgets = data.widgets;
        self.delegates = data.delegates;
        self.rebuild_event_map();
        Ok(())
    }

    /// Rebuild the event→delegate reverse map from all widgets
    fn rebuild_event_map(&mut self) {
        self.event_delegate_map.clear();
        // Count frequency of each (event_name, delegate_type) pair
        let mut freq: HashMap<String, HashMap<String, usize>> = HashMap::new();

        for widget in self.widgets.values() {
            for (event_name, event_info) in &widget.events {
                *freq.entry(event_name.clone())
                    .or_default()
                    .entry(event_info.delegate_type.clone())
                    .or_insert(0) += 1;
            }
        }

        // For each event name, pick the most common delegate type
        for (event_name, type_counts) in freq {
            if let Some((best_type, _)) = type_counts.into_iter().max_by_key(|(_, count)| *count) {
                self.event_delegate_map.insert(event_name, best_type);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API
    // ═══════════════════════════════════════════════════════════════

    /// Get widget info by name (e.g. "SButton")
    pub fn get_widget(&self, name: &str) -> Option<&WidgetInfo> {
        self.widgets.get(name)
    }

    /// Get the native delegate type for a specific widget's event.
    /// This is the most precise query — it tells you exactly what
    /// delegate type SSlider::OnValueChanged expects.
    /// Example: get_event_delegate("SSlider", "OnValueChanged") -> Some("FOnFloatValueChanged")
    pub fn get_event_delegate(&self, widget_name: &str, event_name: &str) -> Option<&str> {
        self.widgets.get(widget_name)?
            .events.get(event_name)
            .map(|e| e.delegate_type.as_str())
    }

    /// Get the native delegate type for an event name across all widgets.
    /// Falls back to the most common delegate type for that event name.
    /// Example: get_event_delegate_any("OnClicked") -> Some("FOnClicked")
    pub fn get_event_delegate_any(&self, event_name: &str) -> Option<&str> {
        self.event_delegate_map.get(event_name).map(|s| s.as_str())
    }

    /// Get the property type for a specific widget property.
    /// Example: get_property_type("SSlider", "MinValue") -> Some("float")
    pub fn get_property_type(&self, widget_name: &str, property_name: &str) -> Option<&str> {
        self.widgets.get(widget_name)?
            .properties.get(property_name)
            .map(|p| p.prop_type.as_str())
    }

    /// Get the include header for a widget.
    /// Example: get_widget_header("SButton") -> Some("Widgets/Input/SButton.h")
    pub fn get_widget_header(&self, widget_name: &str) -> Option<&str> {
        self.widgets.get(widget_name)
            .map(|w| w.header.as_str())
            .filter(|h| !h.is_empty())
    }

    /// Check if a widget has a default content slot ([] syntax)
    pub fn has_default_slot(&self, widget_name: &str) -> bool {
        self.widgets.get(widget_name)
            .map_or(false, |w| w.slots.iter().any(|s| s.kind == "default"))
    }

    /// Check if a widget supports multi-child slots (+Slot() syntax)
    pub fn has_multi_slot(&self, widget_name: &str) -> bool {
        self.widgets.get(widget_name)
            .map_or(false, |w| w.slots.iter().any(|s| s.kind == "multi"))
    }

    /// Get delegate info by name
    pub fn get_delegate(&self, name: &str) -> Option<&DelegateInfo> {
        self.delegates.get(name)
    }

    /// Check if a widget name is known
    pub fn is_known_widget(&self, name: &str) -> bool {
        self.widgets.contains_key(name)
    }

    /// Get the parent widget class
    pub fn get_parent(&self, widget_name: &str) -> Option<&str> {
        self.widgets.get(widget_name)
            .map(|w| w.parent.as_str())
            .filter(|p| !p.is_empty())
    }

    /// Get all event names for a widget
    pub fn get_widget_events(&self, widget_name: &str) -> Vec<&str> {
        self.widgets.get(widget_name)
            .map_or(Vec::new(), |w| w.events.keys().map(|k| k.as_str()).collect())
    }

    /// Get all property names for a widget
    pub fn get_widget_properties(&self, widget_name: &str) -> Vec<&str> {
        self.widgets.get(widget_name)
            .map_or(Vec::new(), |w| w.properties.keys().map(|k| k.as_str()).collect())
    }

    /// Get count stats
    pub fn stats(&self) -> (usize, usize, usize) {
        let total_events: usize = self.widgets.values().map(|w| w.events.len()).sum();
        (self.widgets.len(), total_events, self.delegates.len())
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_registry_json() -> &'static str {
        r#"{
            "widgets": {
                "SButton": {
                    "name": "SButton",
                    "parent": "SBorder",
                    "header": "Widgets/Input/SButton.h",
                    "properties": {
                        "HAlign": { "type": "EHorizontalAlignment", "kind": "argument" },
                        "ContentPadding": { "type": "FMargin", "kind": "attribute" }
                    },
                    "events": {
                        "OnClicked": { "delegate_type": "FOnClicked" },
                        "OnPressed": { "delegate_type": "FSimpleDelegate" },
                        "OnReleased": { "delegate_type": "FSimpleDelegate" }
                    },
                    "slots": [{ "name": "Content", "kind": "default" }]
                },
                "SSlider": {
                    "name": "SSlider",
                    "parent": "SLeafWidget",
                    "header": "Widgets/Input/SSlider.h",
                    "properties": {
                        "Value": { "type": "float", "kind": "attribute" },
                        "MinValue": { "type": "float", "kind": "attribute" },
                        "MaxValue": { "type": "float", "kind": "attribute" }
                    },
                    "events": {
                        "OnValueChanged": { "delegate_type": "FOnFloatValueChanged" }
                    },
                    "slots": []
                },
                "SCheckBox": {
                    "name": "SCheckBox",
                    "parent": "SCompoundWidget",
                    "header": "Widgets/Input/SCheckBox.h",
                    "properties": {},
                    "events": {
                        "OnCheckStateChanged": { "delegate_type": "FOnCheckStateChanged" }
                    },
                    "slots": []
                }
            },
            "delegates": {
                "FOnClicked": {
                    "name": "FOnClicked",
                    "return_type": "FReply",
                    "params": []
                },
                "FOnFloatValueChanged": {
                    "name": "FOnFloatValueChanged",
                    "return_type": "void",
                    "params": ["float"]
                }
            }
        }"#
    }

    #[test]
    fn test_load_and_query() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        assert_eq!(reg.widgets.len(), 3);
        assert_eq!(reg.delegates.len(), 2);
    }

    #[test]
    fn test_event_delegate_lookup() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        // Widget-specific lookup
        assert_eq!(reg.get_event_delegate("SButton", "OnClicked"), Some("FOnClicked"));
        assert_eq!(reg.get_event_delegate("SSlider", "OnValueChanged"), Some("FOnFloatValueChanged"));
        assert_eq!(reg.get_event_delegate("SCheckBox", "OnCheckStateChanged"), Some("FOnCheckStateChanged"));

        // Global event name lookup
        assert_eq!(reg.get_event_delegate_any("OnClicked"), Some("FOnClicked"));
        assert_eq!(reg.get_event_delegate_any("OnValueChanged"), Some("FOnFloatValueChanged"));
    }

    #[test]
    fn test_property_type() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        assert_eq!(reg.get_property_type("SSlider", "MinValue"), Some("float"));
        assert_eq!(reg.get_property_type("SSlider", "Value"), Some("float"));
        assert_eq!(reg.get_property_type("SButton", "ContentPadding"), Some("FMargin"));
    }

    #[test]
    fn test_slot_detection() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        assert!(reg.has_default_slot("SButton"));
        assert!(!reg.has_default_slot("SSlider"));
        assert!(!reg.has_multi_slot("SButton"));
    }

    #[test]
    fn test_widget_header() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        assert_eq!(reg.get_widget_header("SButton"), Some("Widgets/Input/SButton.h"));
        assert_eq!(reg.get_widget_header("SSlider"), Some("Widgets/Input/SSlider.h"));
    }

    #[test]
    fn test_parent_lookup() {
        let mut reg = WidgetRegistry::new();
        reg.load(sample_registry_json()).unwrap();

        assert_eq!(reg.get_parent("SButton"), Some("SBorder"));
        assert_eq!(reg.get_parent("SSlider"), Some("SLeafWidget"));
    }
}
