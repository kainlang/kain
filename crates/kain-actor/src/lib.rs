//! Kain actor system vocabulary.
//!
//! This crate owns actor identities, message contracts, mailbox policy,
//! supervision metadata, runtime snapshots, and model validation. `kain-core`
//! still owns the language AST and interpreter, while this crate gives the
//! actor pipeline a dedicated center of gravity that native runtimes, LLVM
//! lowering, and future scheduler implementations can share.

pub mod address;
pub mod behavior;
pub mod definition;
pub mod id;
pub mod lifecycle;
pub mod mailbox;
pub mod message;
pub mod native;
pub mod registry;
pub mod runtime;
pub mod scheduler;
pub mod supervision;
pub mod system;
pub mod validation;

pub use address::{ActorAddress, ActorAddressError, ActorName, ActorPath};
pub use behavior::{ActorBehaviorContract, ActorBehaviorKind, ActorCallbackSignature};
pub use definition::{
    ActorCapability, ActorDefinition, ActorHandlerSignature, ActorMethodSignature, ActorStateSlot,
};
pub use id::{ActorId, ActorIdAllocator};
pub use lifecycle::{
    ActorExitReason, ActorLifecyclePolicy, ActorState, RestartPolicy, SupervisionStrategy,
    DEFAULT_ASK_TIMEOUT_MS, DEFAULT_SHUTDOWN_GRACE_MS,
};
pub use mailbox::{MailboxCapacity, MailboxOverflowPolicy, MailboxPolicy, MailboxStats};
pub use message::{
    DeliverySemantics, MessageCatalog, MessageEnvelope, MessageName, MessageParameter,
    MessageReplyContract, MessageSignature,
};
pub use native::{NativeActorAbi, NativeActorLoweringContract};
pub use registry::{ActorNameBinding, ActorRegistryModel};
pub use runtime::{
    ActorRefMetadata, ActorRegistryEntry, ActorRuntimeCapabilities, ActorRuntimeEvent,
    ActorRuntimeModel, ActorRuntimeSnapshot, SendOutcome,
};
pub use scheduler::{ActorSchedulerPolicy, SchedulerLaneKind, SchedulerLanePolicy};
pub use supervision::{
    ChildKind, ChildSpec, LinkMode, MonitorSpec, RestartIntensity, SupervisorSpec,
};
pub use system::{ActorSystemDefinition, ActorSystemValidationError, ActorSystemValidationResult};
pub use validation::{
    validate_actor_definition, ActorModelValidator, ActorValidationError, ActorValidationResult,
};

#[cfg(test)]
mod tests;
