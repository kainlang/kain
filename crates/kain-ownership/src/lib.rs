//! Portable ownership-state semantics for Kain memory regions.
//!
//! `kain-core` owns syntax, typing, and interpreter behavior. This crate owns
//! the shared model that future parser/runtime/codegen work can consume for
//! `collapse`, `observe`, and `decay` without inventing lane-specific rules.

use serde::{Deserialize, Serialize};
use thiserror::Error;

pub const OWNERSHIP_CAPABILITY: &str = "memory.ownership";
pub const COLLAPSE_KEYWORD: &str = "collapse";
pub const OBSERVE_KEYWORD: &str = "observe";
pub const DECAY_KEYWORD: &str = "decay";
pub const SHARE_KEYWORD: &str = "share";

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum OwnershipRegionKind {
    LocalAlloca,
    HeapAllocation,
    RcObject,
    WorldState,
    EntangledAuthority,
    EntangledMirror,
    ImportedPointer,
}

impl OwnershipRegionKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::LocalAlloca => "local_alloca",
            Self::HeapAllocation => "heap_allocation",
            Self::RcObject => "rc_object",
            Self::WorldState => "world_state",
            Self::EntangledAuthority => "entangled_authority",
            Self::EntangledMirror => "entangled_mirror",
            Self::ImportedPointer => "imported_pointer",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(transparent)]
pub struct ActiveObserverCount(u32);

impl ActiveObserverCount {
    pub fn new(readers: u32) -> Result<Self, OwnershipTransitionError> {
        if readers == 0 {
            return Err(OwnershipTransitionError::ZeroObserverCount);
        }
        Ok(Self(readers))
    }

    pub fn get(self) -> u32 {
        self.0
    }

    fn increment(self) -> Result<Self, OwnershipTransitionError> {
        self.0
            .checked_add(1)
            .ok_or(OwnershipTransitionError::ObserverCountOverflow)
            .map(Self)
    }

    fn decrement(self) -> Option<Self> {
        self.0
            .checked_sub(1)
            .and_then(|next| if next == 0 { None } else { Some(Self(next)) })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OwnershipState {
    Idle,
    Observed(ActiveObserverCount),
    Collapsed,
    Shared,
    Decayed,
}

impl OwnershipState {
    pub fn observed(readers: u32) -> Result<Self, OwnershipTransitionError> {
        ActiveObserverCount::new(readers).map(Self::Observed)
    }

    pub fn observer_count(self) -> u32 {
        match self {
            Self::Observed(readers) => readers.get(),
            _ => 0,
        }
    }

    pub fn can_observe(self) -> bool {
        matches!(self, Self::Idle | Self::Observed(_))
    }

    pub fn can_collapse(self) -> bool {
        matches!(self, Self::Idle)
    }

    pub fn can_share(self) -> bool {
        matches!(self, Self::Idle)
    }

    pub fn can_decay(self) -> bool {
        matches!(self, Self::Idle)
    }

    pub fn is_terminal(self) -> bool {
        matches!(self, Self::Decayed)
    }

    pub fn apply(self, transition: OwnershipTransition) -> Result<Self, OwnershipTransitionError> {
        match transition {
            OwnershipTransition::BeginObserve => self.begin_observe(),
            OwnershipTransition::EndObserve => self.end_observe(),
            OwnershipTransition::BeginCollapse => self.begin_collapse(),
            OwnershipTransition::EndCollapse => self.end_collapse(),
            OwnershipTransition::BeginShare => self.begin_share(),
            OwnershipTransition::EndShare => self.end_share(),
            OwnershipTransition::Decay => self.decay(),
        }
    }

    fn begin_observe(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Idle => Ok(Self::Observed(ActiveObserverCount::new(1)?)),
            Self::Observed(readers) => Ok(Self::Observed(readers.increment()?)),
            Self::Collapsed => Err(OwnershipTransitionError::CannotObserveCollapsed),
            Self::Shared => Err(OwnershipTransitionError::CannotObserveShared),
            Self::Decayed => Err(OwnershipTransitionError::CannotObserveDecayed),
        }
    }

    fn end_observe(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Observed(readers) => Ok(readers
                .decrement()
                .map(Self::Observed)
                .unwrap_or(Self::Idle)),
            Self::Idle | Self::Collapsed | Self::Shared | Self::Decayed => {
                Err(OwnershipTransitionError::CannotEndObserveWhenNotObserved)
            }
        }
    }

    fn begin_collapse(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Idle => Ok(Self::Collapsed),
            Self::Observed(readers) => Err(OwnershipTransitionError::CannotCollapseWhileObserved {
                readers: readers.get(),
            }),
            Self::Collapsed => Err(OwnershipTransitionError::CannotCollapseAlreadyCollapsed),
            Self::Shared => Err(OwnershipTransitionError::CannotCollapseWhileShared),
            Self::Decayed => Err(OwnershipTransitionError::CannotCollapseDecayed),
        }
    }

    fn end_collapse(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Collapsed => Ok(Self::Idle),
            Self::Idle | Self::Observed(_) | Self::Shared | Self::Decayed => {
                Err(OwnershipTransitionError::CannotEndCollapseWhenNotCollapsed)
            }
        }
    }

    fn begin_share(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Idle => Ok(Self::Shared),
            Self::Observed(readers) => Err(OwnershipTransitionError::CannotShareWhileObserved {
                readers: readers.get(),
            }),
            Self::Collapsed => Err(OwnershipTransitionError::CannotShareWhileCollapsed),
            Self::Shared => Err(OwnershipTransitionError::CannotShareAlreadyShared),
            Self::Decayed => Err(OwnershipTransitionError::CannotShareDecayed),
        }
    }

    fn end_share(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Shared => Ok(Self::Idle),
            Self::Idle | Self::Observed(_) | Self::Collapsed | Self::Decayed => {
                Err(OwnershipTransitionError::CannotEndShareWhenNotShared)
            }
        }
    }

    fn decay(self) -> Result<Self, OwnershipTransitionError> {
        match self {
            Self::Idle => Ok(Self::Decayed),
            Self::Observed(readers) => Err(OwnershipTransitionError::CannotDecayWhileObserved {
                readers: readers.get(),
            }),
            Self::Collapsed => Err(OwnershipTransitionError::CannotDecayWhileCollapsed),
            Self::Shared => Err(OwnershipTransitionError::CannotDecayWhileShared),
            Self::Decayed => Err(OwnershipTransitionError::CannotDecayAlreadyDecayed),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OwnershipTransition {
    BeginObserve,
    EndObserve,
    BeginCollapse,
    EndCollapse,
    BeginShare,
    EndShare,
    Decay,
}

#[derive(Debug, Error, Clone, PartialEq, Eq)]
pub enum OwnershipTransitionError {
    #[error("observed ownership state requires at least one observer")]
    ZeroObserverCount,
    #[error("observer count overflowed")]
    ObserverCountOverflow,
    #[error("cannot observe a collapsed ownership region")]
    CannotObserveCollapsed,
    #[error("cannot observe a shared ownership region")]
    CannotObserveShared,
    #[error("cannot observe a decayed ownership region")]
    CannotObserveDecayed,
    #[error("cannot end observe when the region is not observed")]
    CannotEndObserveWhenNotObserved,
    #[error("cannot collapse an observed ownership region with {readers} active observers")]
    CannotCollapseWhileObserved { readers: u32 },
    #[error("cannot collapse an already collapsed ownership region")]
    CannotCollapseAlreadyCollapsed,
    #[error("cannot collapse a shared ownership region")]
    CannotCollapseWhileShared,
    #[error("cannot collapse a decayed ownership region")]
    CannotCollapseDecayed,
    #[error("cannot end collapse when the region is not collapsed")]
    CannotEndCollapseWhenNotCollapsed,
    #[error("cannot share an observed ownership region with {readers} active observers")]
    CannotShareWhileObserved { readers: u32 },
    #[error("cannot share a collapsed ownership region")]
    CannotShareWhileCollapsed,
    #[error("cannot share an already shared ownership region")]
    CannotShareAlreadyShared,
    #[error("cannot share a decayed ownership region")]
    CannotShareDecayed,
    #[error("cannot end share when the region is not shared")]
    CannotEndShareWhenNotShared,
    #[error("cannot decay an observed ownership region with {readers} active observers")]
    CannotDecayWhileObserved { readers: u32 },
    #[error("cannot decay a collapsed ownership region")]
    CannotDecayWhileCollapsed,
    #[error("cannot decay a shared ownership region")]
    CannotDecayWhileShared,
    #[error("cannot decay an already decayed ownership region")]
    CannotDecayAlreadyDecayed,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ObserveMode {
    ReadonlyBorrow,
    Snapshot,
    EpochRead,
    Unsupported,
}

impl ObserveMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::ReadonlyBorrow => "readonly_borrow",
            Self::Snapshot => "snapshot",
            Self::EpochRead => "epoch_read",
            Self::Unsupported => "unsupported",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CollapseMode {
    ScopedNoAlias,
    ExclusiveToken,
    GraphExclusive,
    Unsupported,
}

impl CollapseMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::ScopedNoAlias => "scoped_noalias",
            Self::ExclusiveToken => "exclusive_token",
            Self::GraphExclusive => "graph_exclusive",
            Self::Unsupported => "unsupported",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ShareMode {
    AtomicSeqCst,
    Unsupported,
}

impl ShareMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::AtomicSeqCst => "atomic_seqcst",
            Self::Unsupported => "unsupported",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DecayMode {
    LifetimeEnd,
    FreeHeap,
    ReleaseStrong,
    Unsupported,
}

impl DecayMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::LifetimeEnd => "lifetime_end",
            Self::FreeHeap => "free_heap",
            Self::ReleaseStrong => "release_strong",
            Self::Unsupported => "unsupported",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct OwnershipPolicy {
    pub region_kind: OwnershipRegionKind,
    pub observe_mode: ObserveMode,
    pub collapse_mode: CollapseMode,
    pub share_mode: ShareMode,
    pub decay_mode: DecayMode,
}

impl OwnershipPolicy {
    pub const fn new(
        region_kind: OwnershipRegionKind,
        observe_mode: ObserveMode,
        collapse_mode: CollapseMode,
        share_mode: ShareMode,
        decay_mode: DecayMode,
    ) -> Self {
        Self {
            region_kind,
            observe_mode,
            collapse_mode,
            share_mode,
            decay_mode,
        }
    }

    pub fn for_region(region_kind: OwnershipRegionKind) -> Self {
        OWNERSHIP_POLICY_TABLE
            .iter()
            .copied()
            .find(|policy| policy.region_kind == region_kind)
            .expect("every ownership region kind must have a policy entry")
    }

    pub fn supports_observe(self) -> bool {
        self.observe_mode != ObserveMode::Unsupported
    }

    pub fn supports_collapse(self) -> bool {
        self.collapse_mode != CollapseMode::Unsupported
    }

    pub fn supports_share(self) -> bool {
        self.share_mode != ShareMode::Unsupported
    }

    pub fn supports_decay(self) -> bool {
        self.decay_mode != DecayMode::Unsupported
    }

    pub fn lowering_hints(self) -> OwnershipLoweringHints {
        OwnershipLoweringHints {
            emits_readonly: self.observe_mode == ObserveMode::ReadonlyBorrow,
            emits_noalias: self.collapse_mode == CollapseMode::ScopedNoAlias,
            emits_lifetime_end: self.decay_mode == DecayMode::LifetimeEnd,
            requires_runtime_guard: matches!(
                self.observe_mode,
                ObserveMode::Snapshot | ObserveMode::EpochRead
            ) || matches!(
                self.collapse_mode,
                CollapseMode::ExclusiveToken | CollapseMode::GraphExclusive
            ) || matches!(
                self.share_mode,
                ShareMode::AtomicSeqCst
            ) || matches!(
                self.decay_mode,
                DecayMode::FreeHeap | DecayMode::ReleaseStrong
            ),
            requires_snapshot: self.observe_mode == ObserveMode::Snapshot,
            may_release_or_free: matches!(
                self.decay_mode,
                DecayMode::FreeHeap | DecayMode::ReleaseStrong
            ),
        }
    }
}

pub const OWNERSHIP_POLICY_TABLE: &[OwnershipPolicy] = &[
    OwnershipPolicy::new(
        OwnershipRegionKind::LocalAlloca,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ScopedNoAlias,
        ShareMode::Unsupported,
        DecayMode::LifetimeEnd,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::HeapAllocation,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ScopedNoAlias,
        ShareMode::AtomicSeqCst,
        DecayMode::FreeHeap,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::RcObject,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ExclusiveToken,
        ShareMode::Unsupported,
        DecayMode::ReleaseStrong,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::WorldState,
        ObserveMode::Snapshot,
        CollapseMode::GraphExclusive,
        ShareMode::Unsupported,
        DecayMode::Unsupported,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::EntangledAuthority,
        ObserveMode::Snapshot,
        CollapseMode::GraphExclusive,
        ShareMode::Unsupported,
        DecayMode::Unsupported,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::EntangledMirror,
        ObserveMode::Snapshot,
        CollapseMode::Unsupported,
        ShareMode::Unsupported,
        DecayMode::Unsupported,
    ),
    OwnershipPolicy::new(
        OwnershipRegionKind::ImportedPointer,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ScopedNoAlias,
        ShareMode::AtomicSeqCst,
        DecayMode::LifetimeEnd,
    ),
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub struct OwnershipLoweringHints {
    pub emits_readonly: bool,
    pub emits_noalias: bool,
    pub emits_lifetime_end: bool,
    pub requires_runtime_guard: bool,
    pub requires_snapshot: bool,
    pub may_release_or_free: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct OwnershipRegionDescriptor {
    pub id: String,
    pub kind: OwnershipRegionKind,
    pub state: OwnershipState,
    pub policy: OwnershipPolicy,
}

impl OwnershipRegionDescriptor {
    pub fn new(id: impl Into<String>, kind: OwnershipRegionKind) -> Self {
        Self {
            id: id.into(),
            kind,
            state: OwnershipState::Idle,
            policy: OwnershipPolicy::for_region(kind),
        }
    }

    pub fn apply(
        &mut self,
        transition: OwnershipTransition,
    ) -> Result<OwnershipState, OwnershipTransitionError> {
        self.state = self.state.apply(transition)?;
        Ok(self.state)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn observe_counts_are_nested_and_return_to_idle() {
        let state = OwnershipState::Idle
            .apply(OwnershipTransition::BeginObserve)
            .expect("first observe")
            .apply(OwnershipTransition::BeginObserve)
            .expect("second observe");

        assert_eq!(state.observer_count(), 2);

        let state = state
            .apply(OwnershipTransition::EndObserve)
            .expect("end one observe");
        assert_eq!(state.observer_count(), 1);

        let state = state
            .apply(OwnershipTransition::EndObserve)
            .expect("end final observe");
        assert_eq!(state, OwnershipState::Idle);
    }

    #[test]
    fn collapse_requires_an_idle_region() {
        let observed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginObserve)
            .expect("observe");

        assert_eq!(
            observed.apply(OwnershipTransition::BeginCollapse),
            Err(OwnershipTransitionError::CannotCollapseWhileObserved { readers: 1 })
        );

        let collapsed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginCollapse)
            .expect("collapse");
        assert_eq!(collapsed, OwnershipState::Collapsed);
    }

    #[test]
    fn decay_is_terminal_and_rejects_live_capabilities() {
        let observed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginObserve)
            .expect("observe");
        assert_eq!(
            observed.apply(OwnershipTransition::Decay),
            Err(OwnershipTransitionError::CannotDecayWhileObserved { readers: 1 })
        );

        let collapsed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginCollapse)
            .expect("collapse");
        assert_eq!(
            collapsed.apply(OwnershipTransition::Decay),
            Err(OwnershipTransitionError::CannotDecayWhileCollapsed)
        );

        let shared = OwnershipState::Idle
            .apply(OwnershipTransition::BeginShare)
            .expect("share");
        assert_eq!(
            shared.apply(OwnershipTransition::Decay),
            Err(OwnershipTransitionError::CannotDecayWhileShared)
        );

        let decayed = OwnershipState::Idle
            .apply(OwnershipTransition::Decay)
            .expect("decay");
        assert!(decayed.is_terminal());
        assert_eq!(
            decayed.apply(OwnershipTransition::BeginObserve),
            Err(OwnershipTransitionError::CannotObserveDecayed)
        );
    }

    #[test]
    fn policy_table_is_conservative_for_worlds_and_entangle() {
        let world = OwnershipPolicy::for_region(OwnershipRegionKind::WorldState);
        assert_eq!(world.observe_mode, ObserveMode::Snapshot);
        assert_eq!(world.collapse_mode, CollapseMode::GraphExclusive);
        assert!(!world.supports_decay());

        let mirror = OwnershipPolicy::for_region(OwnershipRegionKind::EntangledMirror);
        assert_eq!(mirror.observe_mode, ObserveMode::Snapshot);
        assert!(!mirror.supports_collapse());
        assert!(!mirror.supports_decay());
    }

    #[test]
    fn imported_pointers_are_borrowable_but_not_heap_freed() {
        let imported = OwnershipPolicy::for_region(OwnershipRegionKind::ImportedPointer);
        assert_eq!(imported.observe_mode, ObserveMode::ReadonlyBorrow);
        assert_eq!(imported.collapse_mode, CollapseMode::ScopedNoAlias);
        assert_eq!(imported.share_mode, ShareMode::AtomicSeqCst);
        assert_eq!(imported.decay_mode, DecayMode::LifetimeEnd);
    }

    #[test]
    fn share_requires_an_idle_region_and_balances_back_to_idle() {
        let observed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginObserve)
            .expect("observe");
        assert_eq!(
            observed.apply(OwnershipTransition::BeginShare),
            Err(OwnershipTransitionError::CannotShareWhileObserved { readers: 1 })
        );

        let collapsed = OwnershipState::Idle
            .apply(OwnershipTransition::BeginCollapse)
            .expect("collapse");
        assert_eq!(
            collapsed.apply(OwnershipTransition::BeginShare),
            Err(OwnershipTransitionError::CannotShareWhileCollapsed)
        );

        let shared = OwnershipState::Idle
            .apply(OwnershipTransition::BeginShare)
            .expect("share");
        assert_eq!(shared, OwnershipState::Shared);
        assert_eq!(
            shared.apply(OwnershipTransition::EndShare),
            Ok(OwnershipState::Idle)
        );
    }

    #[test]
    fn lowering_hints_match_region_policy() {
        let local = OwnershipPolicy::for_region(OwnershipRegionKind::LocalAlloca).lowering_hints();
        assert!(local.emits_readonly);
        assert!(local.emits_noalias);
        assert!(local.emits_lifetime_end);
        assert!(!local.requires_snapshot);

        let world = OwnershipPolicy::for_region(OwnershipRegionKind::WorldState).lowering_hints();
        assert!(world.requires_runtime_guard);
        assert!(world.requires_snapshot);
        assert!(!world.emits_noalias);
    }

    #[test]
    fn region_descriptor_applies_state_transitions() {
        let mut region =
            OwnershipRegionDescriptor::new("buffer", OwnershipRegionKind::HeapAllocation);
        assert_eq!(
            region.apply(OwnershipTransition::BeginCollapse),
            Ok(OwnershipState::Collapsed)
        );
        assert_eq!(
            region.apply(OwnershipTransition::EndCollapse),
            Ok(OwnershipState::Idle)
        );
        assert_eq!(
            region.apply(OwnershipTransition::Decay),
            Ok(OwnershipState::Decayed)
        );
    }
}
