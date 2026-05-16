# Actor Monitor and Link Implementation

## Overview

Task 6.3 implements monitor and link semantics for the KAIN native actor runtime, enabling exit reason propagation and crash containment as specified in Requirement 6.3.

## Implementation Summary

### Monitor Semantics

**Purpose**: Allow actors to observe the lifecycle of other actors without affecting their execution.

**Key Features**:
- Unidirectional relationship (A monitors B, but B doesn't monitor A)
- Notifications sent for ALL exit reasons (normal and abnormal)
- Exit reason encoded in message type_tag: `0xDEAD0000 | exit_reason`
- Multiple actors can monitor the same actor
- Idempotent registration (duplicate monitors are no-ops)
- Automatic cleanup when either actor terminates

**APIs**:
- `kain_actor_monitor(monitor_id, monitored_id, diag)` - Register monitoring
- `kain_actor_demonitor(monitor_id, monitored_id, diag)` - Remove monitoring

**Implementation**:
- Monitor records stored in linked list on monitoring actor
- `kain_actor_notify_monitors()` iterates all actors to find monitors
- Sends message with type_tag encoding exit reason
- Called from actor exit path (both thread-per-actor and scheduler modes)

### Link Semantics

**Purpose**: Provide crash containment by propagating abnormal exits between linked actors.

**Key Features**:
- Bidirectional relationship (if A links to B, then B is linked to A)
- Propagation ONLY on abnormal exit (not KAIN_ACTOR_EXIT_NORMAL)
- Abnormal exits: KILLED, CRASHED, SUPERVISOR_ESCALATION
- Linked actor terminated with KAIN_ACTOR_EXIT_KILLED
- Idempotent registration (duplicate links are no-ops)
- Automatic cleanup when either actor terminates

**APIs**:
- `kain_actor_link(actor_a, actor_b, diag)` - Create bidirectional link
- `kain_actor_unlink(actor_a, actor_b, diag)` - Remove bidirectional link

**Implementation**:
- Link records stored on BOTH actors (bidirectional)
- `kain_actor_propagate_links()` terminates all linked actors
- Checks actor state to avoid double-termination
- Called from actor exit path only on abnormal exit
- Unlink removes from both actors' link lists

### Exit Reason Structures

**KainActorExitReason enum**:
- `KAIN_ACTOR_EXIT_NORMAL` (0) - Clean shutdown
- `KAIN_ACTOR_EXIT_SHUTDOWN` - Graceful shutdown requested
- `KAIN_ACTOR_EXIT_KILLED` - Forcefully terminated
- `KAIN_ACTOR_EXIT_CRASHED` - Abnormal termination
- `KAIN_ACTOR_EXIT_SUPERVISOR_ESCALATION` - Supervisor-initiated termination

### Crash Containment Behavior

**Monitor Behavior**:
- Monitors receive notification but are NOT terminated
- Monitoring actor can decide how to handle the notification
- Useful for supervision trees and health monitoring

**Link Behavior**:
- Links cause immediate termination of linked actors on abnormal exit
- Prevents cascading failures by containing crashes
- Useful for tightly-coupled actors that cannot function independently

**Integration Points**:
1. `kain_actor_thread_proc()` - Thread-per-actor mode exit handling
2. `kain_scheduler_worker_thread()` - Scheduler mode exit handling
3. `kain_actor_kill()` - Forced termination path
4. `kain_actor_cleanup()` - Resource cleanup including monitors/links

## Testing

A test suite is provided in `runtime/native/tests/test_actor_monitor_link.c` that validates:

1. **Monitor notification on normal exit** - Verifies monitors receive notifications
2. **Link propagation on crash** - Verifies linked actors are terminated on abnormal exit
3. **Demonitor functionality** - Verifies monitor removal prevents notifications

## Requirements Satisfied

This implementation satisfies **Requirement 6.3**:

> WHEN actors are linked or monitored, THEN the System SHALL propagate exit reasons according to defined monitor/link semantics

**Acceptance Criteria Met**:
- ✅ Monitor registration and exit propagation semantics implemented
- ✅ Link registration and crash-containment behavior implemented
- ✅ Exit reason structures defined (KainActorExitReason enum)
- ✅ Exit reasons encoded in monitor notification messages
- ✅ Bidirectional link semantics with proper cleanup
- ✅ Crash containment prevents cascading failures

## Files Modified

1. `runtime/native/include/actor.h`
   - Added `kain_actor_demonitor()` API declaration
   - Enhanced documentation for monitor/link semantics
   - Documented exit reason encoding in messages

2. `runtime/native/src/core/actor.c`
   - Enhanced `kain_actor_monitor()` with idempotency check
   - Implemented `kain_actor_demonitor()` function
   - Enhanced `kain_actor_link()` for bidirectional storage
   - Enhanced `kain_actor_unlink()` for bidirectional removal
   - Enhanced `kain_actor_notify_monitors()` to encode exit reason
   - Enhanced `kain_actor_propagate_links()` with state checking

3. `runtime/native/tests/test_actor_monitor_link.c` (new)
   - Comprehensive test suite for monitor/link functionality

## Design Notes

### Why Bidirectional Link Storage?

Links are stored on both actors to ensure:
- Fast lookup during exit propagation (no global scan needed)
- Proper cleanup when either actor terminates
- Symmetric behavior regardless of which actor exits first

### Why Encode Exit Reason in type_tag?

The exit reason is encoded in the message type_tag (`0xDEAD0000 | exit_reason`) because:
- Avoids allocating separate payload for simple notification
- Makes monitor messages easily identifiable
- Allows monitoring actor to extract exit reason without parsing payload
- Follows common actor pattern of using special message tags for system messages

### Why Only Propagate Links on Abnormal Exit?

Links propagate only on abnormal exit because:
- Normal exit indicates successful completion, not failure
- Linked actors should only terminate each other on crashes
- Allows graceful shutdown of actor groups without cascading kills
- Matches Erlang/OTP link semantics (industry standard)

## Future Enhancements

Potential improvements for future tasks:

1. **Link sets** - Optimize link storage for actors with many links
2. **Monitor references** - Return monitor reference for easier demonitor
3. **Selective monitoring** - Monitor only specific exit reasons
4. **Link groups** - Efficiently link multiple actors at once
5. **Trapping exits** - Allow actors to trap link exits instead of terminating
