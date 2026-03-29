# QuestMaster — Feature Checklist

## Graph Editor Features

- [ ] StartQuestNode - Entry point with quest metadata (quest_id, name, description, category, priority, auto_start)
- [ ] ObjectiveNode - Define objectives (objective_id, description, target_count, optional, hidden, conditions)
- [ ] BranchNode - Multi-condition branching (up to 4 conditions, AND/OR logic, invert)
- [ ] RewardNode - Rewards (XP, gold, items, reputation, custom_rewards, show_notification)
- [ ] CompleteQuestNode - Quest completion (success_state, trigger_event, event_name, save_state)
- [ ] FailQuestNode - Quest failure (failure_reason, allow_retry, trigger_event, save_state)
- [ ] ConditionNode - Single condition check (condition_type, target, comparison_value, invert, success/failure branches)
- [ ] ParallelNode - Simultaneous objectives (branch1-4_enabled, wait_for_all, timeout, on_complete)
- [ ] SequenceNode - Ordered objectives (objective_order, enforce_order, allow_skip, current_index)
- [ ] TimerNode - Countdown timer (duration, show_ui, timer_text, can_pause, success/failure branches)
- [ ] EventNode - Gameplay events (event_name, parameters, delay, broadcast_to_all, target_actor_tag)
- [ ] DialogueNode - Dialogue integration (dialogue_graph_id, wait_for_completion, pass_variables)
- [ ] TeleportNode - Player teleportation (target_location, target_rotation, fade_out, fade_in, fade_duration)
- [ ] SpawnNode - Actor spawning (actor_class, spawn_location, spawn_rotation, attach_to_player, socket_name)
- [ ] CommentNode - Designer notes (comment_text, comment_color, font_size, background_opacity)

## Graph Runtime Features

- [ ] StartQuestNodeData - Execute quest start with instance creation
- [ ] ObjectiveNodeData - Track objective progress with current_count and target_count
- [ ] BranchNodeData - Evaluate multiple conditions with AND/OR logic
- [ ] RewardNodeData - Apply rewards (XP, gold, items, reputation) to player
- [ ] CompleteQuestNodeData - Trigger quest completion with events
- [ ] FailQuestNodeData - Trigger quest failure with reason
- [ ] ConditionNodeData - Evaluate single condition and return success/failure pin
- [ ] ParallelNodeData - Track multiple branches and 