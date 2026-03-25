# KAIN Actor Runtime Ownership and Lifetime Rules

This document defines the ownership, lifetime, and thread-safety rules for the KAIN native actor runtime structures defined in `kain_runtime_actor.h`.

## Core Principles

1. **Single Owner**: Each runtime resource has exactly one owner responsible for its lifecycle
2. **Explicit Cleanup**: All resources are explicitly freed during actor termination
3. **Thread Safety**: Concurrent access is protected by appropriate synchronization primitives
4. **Fail-Safe**: Invalid operations return structured diagnostics rather than causing undefined behavior

## Structure Ownership and Lifetime

### KainActorState_Internal

**Owner**: Actor runtime system (global actor table)

**Lifetime**: 
- Created during `kain_actor_spawn()`
- Lives through all actor state transitions
- Destroyed during actor termination cleanup after TERMINATED/FAILED state

**Thread Safety**:
- Protected by global actor table lock for state transitions
- Individual fields may have additional synchronization (e.g., mailbox)

**Cleanup Responsibilities**:
- Free mailbox and all queued messages
- Remove all monitor relationships
- Remove all link relationships
- Free user_data if destructor provided
- Close thread handle/join thread
- Remove from actor registry if registered
- Free the state structure itself

### KainActorMailbox

**Owner**: The actor's `KainActorState_Internal` structure

**Lifetime**:
- Created during actor spawn
- Lives until actor termination cleanup
- Closed during shutdown to prevent new messages

**Thread Safety**:
- Fully thread-safe for concurrent sends from multiple actors
- Only the owning actor can receive (single consumer)
- Uses mutex + condition variables for synchronization

**Ownership Rules**:
- Mailbox owns all `MessageNode` structures in its queue
- Message data (`KainActorMessage.data`) ownership transfers to receiver on `kain_actor_receive()`
- Sender must not access message data after successful send
- Receiver must free message data when done

**Backpressure**:
- Bounded mailboxes (capacity > 0): senders block when full
- Unbounded mailboxes (capacity = 0): senders never block on capacity
- Closed mailboxes: senders receive KAIN_DIAG_CODE_ACTOR_MAILBOX_CLOSED error

### KainActorMessage

**Owner**: Varies by lifecycle stage

**Ownership Transfer**:
1. **Before send**: Sender owns the message and its data
2. **During send**: Ownership transfers to mailbox
3. **After receive**: Receiver owns the message data
4. **Cleanup**: Receiver must free data when done

**Lifetime**:
- Created by sender before `kain_actor_send()`
- Queued in mailbox as `MessageNode`
- Delivered to receiver via `kain_actor_receive()`
- Freed by receiver after processing

**Data Ownership**:
- `data` pointer: owned by message owner
- `data_size`: informational, used for validation
- `type_tag`: copied, no ownership concerns
- `sender_id`: copied, no ownership concerns

### KainActorMonitor

**Owner**: The monitoring actor's state (in `monitors` linked list)

**Lifetime**:
- Created during `kain_actor_monitor()`
- Lives until explicitly removed or either actor terminates
- Destroyed during actor cleanup or explicit unmonitor

**Thread Safety**:
- Protected by global actor table lock
- Monitor list modifications require lock

**Notification Semantics**:
- When monitored actor exits, notification message sent to monitor's mailbox
- Notification includes monitored actor ID and exit reason
- Monitor is automatically removed after notification sent

### KainActorLink

**Owner**: Shared between both linked actors' states

**Lifetime**:
- Created during `kain_actor_link()`
- Lives until explicitly unlinked or either actor terminates
- Destroyed during `kain_actor_unlink()` or actor cleanup

**Thread Safety**:
- Protected by global actor table lock
- Link list modifications require lock

**Termination Semantics**:
- If either actor exits abnormally (CRASHED, KILLED), the other is terminated
- Normal exits (NORMAL, SHUTDOWN) do not propagate
- Link is removed before propagating termination to prevent cycles

### KainActorSupervisor

**Owner**: The child actor's state

**Lifetime**:
- Created during actor spawn if `supervisor_id` provided
- Lives for the lifetime of the child actor
- Used during child exit to apply restart policy

**Thread Safety**:
- Protected by child actor's state lock
- Accessed during spawn and termination

**Restart Semantics**:
- PERMANENT: always restart on any exit
- TEMPORARY: never restart
- TRANSIENT: restart only on abnormal exit (CRASHED, KILLED)
- Restart count and timing tracked to prevent restart storms

### KainActorSchedulerNode

**Owner**: Scheduler's ready queue

**Lifetime**:
- Transient: created when actor becomes runnable
- Destroyed when actor is scheduled for execution
- Actor may be enqueued/dequeued many times

**Thread Safety**:
- Protected by scheduler queue lock
- Only scheduler modifies queue

**Scheduling Rules**:
- Actors enqueued when messages arrive or after yield
- Actors dequeued for execution by worker threads
- Fair scheduling: round-robin or priority-based

### KainActorHandle

**Owner**: Caller (lightweight reference)

**Lifetime**:
- Created by caller (typically just stores actor ID)
- Valid as long as referenced actor exists
- Does not own actor state

**Thread Safety**:
- Read-only reference, no synchronization needed
- Operations validate actor existence before proceeding

**Validity**:
- Handle becomes invalid when actor terminates
- Operations on invalid handles return KAIN_DIAG_CODE_ACTOR_NOT_FOUND

## Actor Lifecycle State Machine

```
UNINITIALIZED
    ↓ (spawn)
INITIALIZING
    ↓ (bootstrap starts)
RUNNING
    ↓ (shutdown request)
SHUTTING_DOWN
    ↓ (cleanup complete)
TERMINATED (normal) or FAILED (abnormal)
```

**State Transition Rules**:
- UNINITIALIZED → INITIALIZING: during spawn, before thread starts
- INITIALIZING → RUNNING: when bootstrap function begins execution
- RUNNING → SUSPENDED: when actor blocks on mailbox or external resource
- SUSPENDED → RUNNING: when actor becomes runnable again
- RUNNING → SHUTTING_DOWN: on shutdown request or supervisor decision
- SHUTTING_DOWN → TERMINATED: on normal exit
- SHUTTING_DOWN → FAILED: on abnormal exit
- Any state → FAILED: on crash or kill

## Message Passing Ownership Rules

### Sending Messages

```c
// Sender creates and owns message
KainActorMessage msg;
msg.type_tag = MY_MESSAGE_TYPE;
msg.data = malloc(sizeof(MyData));
msg.data_size = sizeof(MyData);
msg.sender_id = my_actor_id;

// Ownership transfers to mailbox on successful send
int result = kain_actor_send(target_id, &msg, &diag);

// After successful send, sender MUST NOT access msg.data
// If send fails, sender still owns msg.data and must free it
if (result != 0) {
    free(msg.data);  // Sender still owns on failure
}
```

### Receiving Messages

```c
// Receiver gets ownership of message data
KainActorMessage msg;
int result = kain_actor_receive(mailbox, &msg, &diag);

if (result == 0) {
    // Receiver now owns msg.data
    process_message(&msg);
    
    // Receiver MUST free msg.data when done
    free(msg.data);
}
```

## Synchronization Primitives

### Windows (Win32)

- **Mutex**: `CRITICAL_SECTION` for lightweight locks
- **Condition Variables**: `HANDLE` events for wait/signal
- **Thread**: `HANDLE` for thread management

### POSIX (Linux/macOS)

- **Mutex**: `pthread_mutex_t` for locks
- **Condition Variables**: `pthread_cond_t` for wait/signal
- **Thread**: `pthread_t` for thread management

## Error Handling and Diagnostics

All actor runtime operations that can fail accept a `KainDiagnostic*` parameter:

- **Success**: Return 0 or valid ID, diagnostic unchanged
- **Failure**: Return non-zero or INVALID_ID, diagnostic populated with:
  - Subsystem: KAIN_DIAG_SUBSYSTEM_ACTOR
  - Severity: ERROR or FATAL
  - Code: Specific error code (3000-3999 range)
  - Message: Human-readable error description
  - Detail: Additional context if available

## Memory Management Rules

1. **Actor State**: Allocated by runtime, freed during termination cleanup
2. **Mailbox Messages**: Allocated by sender, freed by receiver or during mailbox cleanup
3. **Monitor/Link Nodes**: Allocated during relationship creation, freed during removal or cleanup
4. **User Data**: Ownership defined by spawn config, freed by destructor if provided
5. **Diagnostic Strings**: Stack-allocated or static, no dynamic allocation

## Concurrency Guarantees

1. **Mailbox Send**: Multiple actors can send to same mailbox concurrently
2. **Mailbox Receive**: Only owning actor can receive (single consumer)
3. **State Transitions**: Protected by global actor table lock
4. **Monitor/Link Operations**: Protected by global actor table lock
5. **Registry Operations**: Protected by registry lock

## Cleanup Order During Termination

1. Set actor state to SHUTTING_DOWN
2. Close mailbox (prevent new messages)
3. Wait for bootstrap function to return
4. Process remaining mailbox messages (optional, policy-dependent)
5. Notify monitors of exit
6. Propagate to linked actors if abnormal exit
7. Notify supervisor for restart decision
8. Remove from registry if registered
9. Free all monitor nodes
10. Free all link nodes
11. Free all mailbox messages
12. Destroy mailbox synchronization primitives
13. Free user_data if destructor provided
14. Close/join thread
15. Set state to TERMINATED or FAILED
16. Free actor state structure
17. Remove from global actor table

## Best Practices

1. **Always check return values**: All operations can fail
2. **Always populate diagnostics**: Provide context for failures
3. **Free message data**: Receivers must free after processing
4. **Handle mailbox full**: Implement backpressure or retry logic
5. **Validate actor IDs**: Check for KAIN_ACTOR_ID_INVALID
6. **Use bounded mailboxes**: Prevent memory exhaustion
7. **Implement graceful shutdown**: Handle SHUTTING_DOWN state
8. **Test supervision trees**: Verify restart policies work correctly
9. **Monitor critical actors**: Use monitors for failure detection
10. **Document message types**: Define type_tag constants clearly

## Future Considerations

- **Priority scheduling**: Add priority field to scheduler nodes
- **Message priorities**: Support urgent/normal/low priority messages
- **Selective receive**: Allow actors to receive specific message types
- **Mailbox inspection**: APIs to query mailbox state without receiving
- **Actor groups**: Broadcast messages to actor groups
- **Distributed actors**: Remote actor references and message passing
- **Hot code reload**: Update actor code without losing state
