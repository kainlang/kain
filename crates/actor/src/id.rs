use serde::{Deserialize, Serialize};
use std::fmt;

/// Stable actor identifier used by interpreter and native runtime surfaces.
///
/// `0` is intentionally reserved as the invalid/null actor ID so the Rust
/// model lines up with the C runtime ABI.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
#[repr(transparent)]
pub struct ActorId(u64);

impl ActorId {
    pub const INVALID_RAW: u64 = 0;
    pub const FIRST_VALID_RAW: u64 = 1;

    pub fn new(raw: u64) -> Option<Self> {
        if raw == Self::INVALID_RAW {
            None
        } else {
            Some(Self(raw))
        }
    }

    pub const fn new_unchecked(raw: u64) -> Self {
        Self(raw)
    }

    pub const fn invalid() -> Self {
        Self(Self::INVALID_RAW)
    }

    pub const fn as_u64(self) -> u64 {
        self.0
    }

    pub const fn is_valid(self) -> bool {
        self.0 != Self::INVALID_RAW
    }
}

impl Default for ActorId {
    fn default() -> Self {
        Self::invalid()
    }
}

impl From<ActorId> for u64 {
    fn from(value: ActorId) -> Self {
        value.as_u64()
    }
}

impl fmt::Display for ActorId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Monotonic allocator for actor IDs.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ActorIdAllocator {
    next_raw: u64,
}

impl ActorIdAllocator {
    pub fn starting_at(next_raw: u64) -> Self {
        let next_raw = next_raw.max(ActorId::FIRST_VALID_RAW);
        Self { next_raw }
    }

    pub fn starting_after(last_raw: u64) -> Self {
        Self::starting_at(last_raw.saturating_add(1))
    }

    pub fn peek_next(&self) -> ActorId {
        ActorId::new_unchecked(self.next_raw.max(ActorId::FIRST_VALID_RAW))
    }

    pub fn allocate(&mut self) -> ActorId {
        let actor_id = self.peek_next();
        self.next_raw = self
            .next_raw
            .saturating_add(1)
            .max(ActorId::FIRST_VALID_RAW);
        actor_id
    }

    pub fn reserve_raw(&mut self, raw: u64) {
        if raw >= self.next_raw {
            self.next_raw = raw.saturating_add(1).max(ActorId::FIRST_VALID_RAW);
        }
    }
}

impl Default for ActorIdAllocator {
    fn default() -> Self {
        Self::starting_at(ActorId::FIRST_VALID_RAW)
    }
}
