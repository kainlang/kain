/// The full data-driven IR for a Blueprint asset.
///
/// This is the single source of truth — both the C++ factory
/// generator and the binary .uasset writer consume this same IR.
///
/// Example:
/// ```ignore
/// let bp = BlueprintDef::new("BP_Player", "/Game/MyPlugin/Blueprints", "/Script/MyPlugin.APlayerBase")
///     .add_component(
///         ComponentDef::new("StaticMeshComponent", "Mesh")
///             .with_default(PropertyDef::soft_object("StaticMesh", "/Game/Meshes/SM_Player.SM_Player"))
///             .with_default(PropertyDef::bool("bCastShadow", true))
///     )
///     .add_default(PropertyDef::float("MaxWalkSpeed", 600.0))
///     .add_event(EventGraphNode::begin_play(vec![
///         KismetCall::function("InitializeAbilitySystem"),
///     ]));
/// ```
use serde::{Deserialize, Serialize};

// ─── Property Values ─────────────────────────────────────────────────────────
// Re-exported from the shared ue5-asset-utils crate (single source of truth).
pub use ue5_asset_utils::{KainEngineTarget, PropertyDef, PropertyValue};

// ─── Component Tree (SCS) ────────────────────────────────────────────────────

/// A component attached to a Blueprint via the SimpleConstructionScript.
/// Maps to a `USCS_Node` in the .uasset.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentDef {
    /// UE5 component class — must be an engine class or a KAIN-generated class.
    /// e.g. "StaticMeshComponent", "CapsuleComponent", "SkeletalMeshComponent"
    pub class_name: String,

    /// Blueprint variable name (how it appears in the BP editor).
    pub variable_name: String,

    /// Parent component variable name, or None to attach to the root.
    pub attach_parent: Option<String>,

    /// Default property overrides on this component's CDO.
    pub defaults: Vec<PropertyDef>,
}

impl ComponentDef {
    pub fn new(class_name: impl Into<String>, variable_name: impl Into<String>) -> Self {
        Self {
            class_name: class_name.into(),
            variable_name: variable_name.into(),
            attach_parent: None,
            defaults: Vec::new(),
        }
    }

    pub fn with_parent(mut self, parent: impl Into<String>) -> Self {
        self.attach_parent = Some(parent.into());
        self
    }

    pub fn with_default(mut self, prop: PropertyDef) -> Self {
        self.defaults.push(prop);
        self
    }
}

// ─── Event Graph (Kismet) ─────────────────────────────────────────────────────

/// A simple function call node in the event graph.
/// Phase 1: supports calling C++ functions exposed to Blueprints.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KismetCall {
    /// Function name to call (must be UFUNCTION-exposed).
    pub function_name: String,
    /// Optional target object (None = self).
    pub target: Option<String>,
    /// Whether this is a pure function (no exec pins).
    pub is_pure: bool,
}

impl KismetCall {
    pub fn function(name: impl Into<String>) -> Self {
        Self { function_name: name.into(), target: None, is_pure: false }
    }

    pub fn on(mut self, target: impl Into<String>) -> Self {
        self.target = Some(target.into());
        self
    }

    pub fn pure(mut self) -> Self {
        self.is_pure = true;
        self
    }
}

/// A node in the Blueprint event graph.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum EventGraphNode {
    /// Event BeginPlay → sequence of calls
    BeginPlay { calls: Vec<KismetCall> },
    /// Event Tick → sequence of calls (marks material as dynamic)
    Tick { calls: Vec<KismetCall> },
    /// Custom event with a name
    CustomEvent { event_name: String, calls: Vec<KismetCall> },
}

impl EventGraphNode {
    pub fn begin_play(calls: Vec<KismetCall>) -> Self {
        Self::BeginPlay { calls }
    }
    pub fn tick(calls: Vec<KismetCall>) -> Self {
        Self::Tick { calls }
    }
    pub fn custom(event_name: impl Into<String>, calls: Vec<KismetCall>) -> Self {
        Self::CustomEvent { event_name: event_name.into(), calls }
    }
}

// ─── Blueprint Definition (Root IR) ──────────────────────────────────────────

/// The complete data-driven description of a Blueprint asset KAIN will generate.
/// Consumed by both:
///   - `BlueprintBinaryWriter`  → writes real .uasset bytes (no editor needed)
///   - `BlueprintFactoryWriter` → writes C++ factory code (editor-startup fallback)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlueprintDef {
    /// Asset name without extension. e.g. "BP_Player"
    pub name: String,

    /// Content browser path. e.g. "/Game/MyPlugin/Blueprints"
    pub package_path: String,

    /// Fully-qualified parent C++ class path.
    /// e.g. "/Script/MyPlugin.APlayerBase"
    pub parent_class: String,

    /// Component tree (SimpleConstructionScript nodes).
    pub components: Vec<ComponentDef>,

    /// ClassDefaultObject property overrides.
    pub defaults: Vec<PropertyDef>,

    /// Event graph nodes (BeginPlay, Tick, custom events).
    pub event_graph: Vec<EventGraphNode>,

    /// UE5 engine version to target. Affects binary format.
    /// Defaults to UE5.3.
    pub engine_version: BlueprintEngineVersion,
}

/// Which UE5 version to target when writing binary .uasset files.
///
/// This is a thin type alias to [`KainEngineTarget`] allowing Blueprint IR
/// to carry version information without a direct dependency on ue5-asset-utils
/// internals. All serialization uses [`KainEngineTarget::as_serializer_version`].
pub type BlueprintEngineVersion = KainEngineTarget;

/// Convenience constructor aliases so existing `BlueprintEngineVersion::Ue5_x`
/// call sites still compile without changes.

impl BlueprintDef {
    pub fn new(
        name: impl Into<String>,
        package_path: impl Into<String>,
        parent_class: impl Into<String>,
    ) -> Self {
        Self {
            name: name.into(),
            package_path: package_path.into(),
            parent_class: parent_class.into(),
            components: Vec::new(),
            defaults: Vec::new(),
            event_graph: Vec::new(),
            engine_version: BlueprintEngineVersion::default(),
        }
    }

    /// Full asset path — used in import tables of other assets.
    /// e.g. "/Game/MyPlugin/Blueprints/BP_Player"
    pub fn asset_path(&self) -> String {
        format!("{}/{}", self.package_path, self.name)
    }

    /// Full object path including class suffix.
    /// e.g. "/Game/MyPlugin/Blueprints/BP_Player.BP_Player_C"
    pub fn generated_class_path(&self) -> String {
        format!("{}/{}.{}_C", self.package_path, self.name, self.name)
    }

    pub fn with_engine_version(mut self, v: BlueprintEngineVersion) -> Self {
        self.engine_version = v;
        self
    }

    pub fn add_component(mut self, comp: ComponentDef) -> Self {
        self.components.push(comp);
        self
    }

    pub fn add_default(mut self, prop: PropertyDef) -> Self {
        self.defaults.push(prop);
        self
    }

    pub fn add_event(mut self, node: EventGraphNode) -> Self {
        self.event_graph.push(node);
        self
    }
}
