use serde::{Deserialize, Serialize};

pub const DEFAULT_MAILBOX_CAPACITY: usize = 1024;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum MailboxCapacity {
    Unbounded,
    Bounded(usize),
}

impl MailboxCapacity {
    pub const fn default_runtime() -> Self {
        Self::Bounded(DEFAULT_MAILBOX_CAPACITY)
    }

    pub const fn is_valid(self) -> bool {
        match self {
            Self::Unbounded => true,
            Self::Bounded(capacity) => capacity > 0,
        }
    }

    pub const fn limit(self) -> Option<usize> {
        match self {
            Self::Unbounded => None,
            Self::Bounded(capacity) => Some(capacity),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum MailboxOverflowPolicy {
    Block,
    DropNewest,
    DropOldest,
    FailSender,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MailboxPolicy {
    pub capacity: MailboxCapacity,
    pub overflow: MailboxOverflowPolicy,
    pub priority_messages: bool,
}

impl Default for MailboxPolicy {
    fn default() -> Self {
        Self {
            capacity: MailboxCapacity::default_runtime(),
            overflow: MailboxOverflowPolicy::Block,
            priority_messages: false,
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct MailboxStats {
    pub queued: usize,
    pub received: u64,
    pub delivered: u64,
    pub dropped: u64,
}

impl MailboxStats {
    pub const fn is_empty(self) -> bool {
        self.queued == 0
    }
}
