//! Central Unreal Engine version authority for KAIN.
//!
//! ## Problem
//!
//! The vendored `unreal_asset` serializer has a hard ceiling at the highest
//! `EngineVersion` variant it knows about. Adding new engine versions to KAIN
//! should not require touching every `Asset::new_empty(VER_UE5_2)` call site
//! spread across every crate.
//!
//! ## Solution
//!
//! `KainEngineTarget` is the **single source of truth** for version handling:
//!   - KAIN code (IR, CLI, config) works with `KainEngineTarget` values.
//!   - Only `KainEngineTarget::as_serializer_version()` ever touches
//!     the raw `EngineVersion` enum from `unreal_asset_base`.
//!
//! ## Upgrade Path
//!
//! When Epic releases a new engine version *and* the vendored `unreal_asset`
//! library is updated to support it:
//!   1. Add the new variant to `KainEngineTarget`.
//!   2. Update **only** `as_serializer_version()` to return the new variant.
//!   3. Nothing else changes.
//!
//! Until the library catches up, new versions map to the highest known safe
//! binary format (currently `VER_UE5_2`), which UE5.3+ accepts due to Epic's
//! backwards format compatibility guarantee.

use serde::{Deserialize, Serialize};
use unreal_asset_base::engine_version::EngineVersion;

// ---------------------------------------------------------------------------
// KainEngineTarget — public version enum
// ---------------------------------------------------------------------------

/// The UE5 engine version KAIN is generating assets *for*.
///
/// Decoupled from `unreal_asset_base::engine_version::EngineVersion` so that:
/// - KAIN can express intent (e.g. "targeting 5.5") even before the vendored
///   serializer knows about that version.
/// - Version upgrading is a single-file change.
///
/// # Serializer Ceiling
///
/// The vendored `unreal_asset` library is updated asynchronously. The mapping
/// returned by [`KainEngineTarget::as_serializer_version`] reflects the
/// **highest safe binary format** the library currently supports.
/// UE5.x (x >= 3) reliably loads assets written in the 5.2 format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum KainEngineTarget {
    /// Unreal Engine 5.0
    Ue5_0,
    /// Unreal Engine 5.1
    Ue5_1,
    /// Unreal Engine 5.2
    Ue5_2,
    /// Unreal Engine 5.3
    Ue5_3,
    /// Unreal Engine 5.4
    Ue5_4,
    /// Unreal Engine 5.5
    Ue5_5,
    /// Unreal Engine 5.6
    Ue5_6,
    /// Unreal Engine 5.7
    Ue5_7,
}

impl Default for KainEngineTarget {
    /// Defaults to UE 5.4 — a stable, widely deployed version.
    fn default() -> Self {
        Self::Ue5_4
    }
}

impl KainEngineTarget {
    /// The highest `KainEngineTarget` the current KAIN toolchain supports.
    pub const MAX_SUPPORTED: KainEngineTarget = KainEngineTarget::Ue5_7;

    /// Convert this target to the `EngineVersion` the `unreal_asset` serializer
    /// will use when writing `.uasset` bytes.
    ///
    /// # Compatibility Notes
    ///
    /// - UE 5.0 → serialized as `VER_UE5_0`
    /// - UE 5.1 → serialized as `VER_UE5_1`
    /// - UE 5.2 → serialized as `VER_UE5_2`
    /// - UE 5.3–5.7 → serialized as `VER_UE5_2` (highest known format in the
    ///   vendored library). This is **safe**: Epic guarantees that newer engine
    ///   versions can load assets written by any older compatible format.
    ///
    /// When the vendored `unreal_asset` library is updated to expose
    /// `VER_UE5_3` etc., update *only* this function.
    pub fn as_serializer_version(self) -> EngineVersion {
        // ── Version dispatch table ───────────────────────────────────────────
        // This is the ONLY place in the entire KAIN codebase that maps
        // KainEngineTarget → the raw EngineVersion enum.
        match self {
            KainEngineTarget::Ue5_0 => EngineVersion::VER_UE5_0,
            KainEngineTarget::Ue5_1 => EngineVersion::VER_UE5_1,
            // 5.2+ all serialize at the 5.2 binary format for now.
            // TODO: update each arm below when the vendored library is extended
            // e.g.: KainEngineTarget::Ue5_3 => EngineVersion::VER_UE5_3,
            KainEngineTarget::Ue5_2
            | KainEngineTarget::Ue5_3
            | KainEngineTarget::Ue5_4
            | KainEngineTarget::Ue5_5
            | KainEngineTarget::Ue5_6
            | KainEngineTarget::Ue5_7 => EngineVersion::VER_UE5_2,
        }
    }

    /// Returns the human-readable version string, e.g. `"5.4"`.
    pub fn as_str(self) -> &'static str {
        match self {
            KainEngineTarget::Ue5_0 => "5.0",
            KainEngineTarget::Ue5_1 => "5.1",
            KainEngineTarget::Ue5_2 => "5.2",
            KainEngineTarget::Ue5_3 => "5.3",
            KainEngineTarget::Ue5_4 => "5.4",
            KainEngineTarget::Ue5_5 => "5.5",
            KainEngineTarget::Ue5_6 => "5.6",
            KainEngineTarget::Ue5_7 => "5.7",
        }
    }

    /// Parse from a version string like `"5.4"`.
    pub fn from_str(s: &str) -> Option<Self> {
        match s.trim() {
            "5.0" => Some(Self::Ue5_0),
            "5.1" => Some(Self::Ue5_1),
            "5.2" => Some(Self::Ue5_2),
            "5.3" => Some(Self::Ue5_3),
            "5.4" => Some(Self::Ue5_4),
            "5.5" => Some(Self::Ue5_5),
            "5.6" => Some(Self::Ue5_6),
            "5.7" => Some(Self::Ue5_7),
            _ => None,
        }
    }

    /// The effective binary format version written to disk.
    /// Useful for logging/diagnostics when `self != self.serializer_ceiling()`.
    pub fn serializer_ceiling(self) -> KainEngineTarget {
        match self {
            KainEngineTarget::Ue5_0 => KainEngineTarget::Ue5_0,
            KainEngineTarget::Ue5_1 => KainEngineTarget::Ue5_1,
            // Everything at or above 5.2 currently serializes at 5.2 format
            _ => KainEngineTarget::Ue5_2,
        }
    }

    /// True if the target version is higher than what the serializer natively
    /// knows about (i.e. the file will be written in a backwards-compat format).
    pub fn is_above_serializer_ceiling(self) -> bool {
        self > KainEngineTarget::Ue5_2
    }
}

impl std::fmt::Display for KainEngineTarget {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.as_str())
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_all_versions_map_to_valid_serializer_version() {
        let all = [
            KainEngineTarget::Ue5_0,
            KainEngineTarget::Ue5_1,
            KainEngineTarget::Ue5_2,
            KainEngineTarget::Ue5_3,
            KainEngineTarget::Ue5_4,
            KainEngineTarget::Ue5_5,
            KainEngineTarget::Ue5_6,
            KainEngineTarget::Ue5_7,
        ];
        for v in all {
            let sv = v.as_serializer_version();
            // Must not be UNKNOWN
            assert_ne!(sv, EngineVersion::UNKNOWN, "{v} mapped to UNKNOWN");
        }
    }

    #[test]
    fn test_ue5_0_maps_distinctly() {
        assert_eq!(KainEngineTarget::Ue5_0.as_serializer_version(), EngineVersion::VER_UE5_0);
        assert_eq!(KainEngineTarget::Ue5_1.as_serializer_version(), EngineVersion::VER_UE5_1);
        assert_eq!(KainEngineTarget::Ue5_2.as_serializer_version(), EngineVersion::VER_UE5_2);
    }

    #[test]
    fn test_ue5_3_through_5_7_map_to_ue5_2_format() {
        for v in [
            KainEngineTarget::Ue5_3,
            KainEngineTarget::Ue5_4,
            KainEngineTarget::Ue5_5,
            KainEngineTarget::Ue5_6,
            KainEngineTarget::Ue5_7,
        ] {
            assert_eq!(
                v.as_serializer_version(),
                EngineVersion::VER_UE5_2,
                "{v} should serialize at VER_UE5_2 format until library is updated"
            );
        }
    }

    #[test]
    fn test_is_above_serializer_ceiling() {
        assert!(!KainEngineTarget::Ue5_2.is_above_serializer_ceiling());
        assert!(KainEngineTarget::Ue5_3.is_above_serializer_ceiling());
        assert!(KainEngineTarget::Ue5_7.is_above_serializer_ceiling());
    }

    #[test]
    fn test_round_trip_str() {
        let versions = ["5.0", "5.1", "5.2", "5.3", "5.4", "5.5", "5.6", "5.7"];
        for s in versions {
            let parsed = KainEngineTarget::from_str(s).expect(s);
            assert_eq!(parsed.as_str(), s);
        }
    }

    #[test]
    fn test_default_is_stable_version() {
        // Default should be a well-tested stable version, not the bleeding edge
        let d = KainEngineTarget::default();
        assert!(d >= KainEngineTarget::Ue5_3, "default should be at least 5.3");
        assert!(d <= KainEngineTarget::Ue5_5, "default should not be experimental edge");
    }

    #[test]
    fn test_serializer_ceiling() {
        assert_eq!(KainEngineTarget::Ue5_0.serializer_ceiling(), KainEngineTarget::Ue5_0);
        assert_eq!(KainEngineTarget::Ue5_2.serializer_ceiling(), KainEngineTarget::Ue5_2);
        assert_eq!(KainEngineTarget::Ue5_5.serializer_ceiling(), KainEngineTarget::Ue5_2);
        assert_eq!(KainEngineTarget::Ue5_7.serializer_ceiling(), KainEngineTarget::Ue5_2);
    }
}
