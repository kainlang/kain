// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorConstructionConfiguration.h"

class USMBlueprint;
struct FSMConditionalCompileConfiguration;
struct FSMEditorStateMachine;

/**
 * Construction manager singleton for running construction scripts and building editor state machines.
 */
class ISMEditorConstructionManager
{
protected:
	ISMEditorConstructionManager() {}
	
public:
	virtual ~ISMEditorConstructionManager() = default;

	/** Retrieve the Editor Construction Manager singleton. */
	SMSYSTEMEDITOR_API static ISMEditorConstructionManager& Get();

	/** Enable or disable construction scripts. Cancels RunAllConstructionScriptsForBlueprint if false. */
	virtual void SetEnableConstructionScripts(const bool bEnable) = 0;

	/** Checks if the construction manager has construction scripts enabled. */
	virtual bool AreConstructionScriptsEnabled() const = 0;
	
	/** Checks if there are construction scripts for this frame. */
	virtual bool HasPendingConstructionScripts() const = 0;

	/**
	 * True if construction scripts are currently running for this frame.
	 * 
	 * @param ForBlueprint If construction scripts are running for the given blueprint. Null implies any.
	 * @return True if construction scripts are in progress.
	 */
	virtual bool IsRunningConstructionScripts(USMBlueprint* ForBlueprint = nullptr) const = 0;
	
	/**
	 * Frees all associated memory and resets the editor state machine map.
	 */
	virtual void CleanupAllEditorStateMachines() = 0;

	/**
	 *  Shutdown the editor instance and free node memory.
	 *
	 *  @param InBlueprint for the editor state machine.
	 */
	virtual void CleanupEditorStateMachine(USMBlueprint* InBlueprint) = 0;

	/**
	 * Runs all construction scripts for every node in a blueprint. This is executed on this frame
	 * and even during a compile.
	 *
	 * @param InBlueprint The blueprint to run all construction scripts for.
	 * @param bCleanupEditorStateMachine If the editor state machine should be cleaned up afterward. If this is false
	 * then CleanupEditorStateMachine must be called manually.
	 */
	virtual void RunAllConstructionScriptsForBlueprintImmediately(USMBlueprint* InBlueprint, bool bCleanupEditorStateMachine = true) = 0;

	/**
	 * Runs all construction scripts for every node in a blueprint. This is executed on the next frame.
	 *
	 * @param InObject The exact blueprint or the object belonging to the blueprint to run all construction scripts for.
	 * @param InConfiguration Provided configuration for the construction run.
	 */
	virtual void RunAllConstructionScriptsForBlueprint(UObject* InObject, const FSMConstructionConfiguration& InConfiguration = FSMConstructionConfiguration()) = 0;

	/**
	 * Signal a blueprint should run the conditional compile operation next tick. This won't occur if the blueprint is running construction scripts.
	 *
	 * @param InBlueprint The state machine blueprint to run conditional compile on.
	 * @param InConfiguration The configuration of the conditional compile.
	 */
	virtual void QueueBlueprintForConditionalCompile(USMBlueprint* InBlueprint, const FSMConditionalCompileConfiguration& InConfiguration = FSMConditionalCompileConfiguration()) = 0;

	/**
	 * Create or update a state machine for editor use.
	 *
	 * @param InBlueprint The blueprint owning the state machine.
	 * 
	 * @return The editor state machine created.
	 */
	virtual FSMEditorStateMachine& CreateEditorStateMachine(USMBlueprint* InBlueprint) = 0;

	/**
	 * Retrieve an existing editor state machine if one exists.
	 *
	 * @param InBlueprint The blueprint owning the state machine.
	 * @param OutEditorStateMachine The existing editor state machine if one exists. This may be invalidated
	 * if another editor state machine is added after retrieving this one.
	 *
	 * @return True if the state machine was found, false if it doesn't exist.
	 */
	virtual bool TryGetEditorStateMachine(USMBlueprint* InBlueprint, FSMEditorStateMachine& OutEditorStateMachine) = 0;

	/**
	 * Allow construction scripts to run on load.
	 *
	 * @param bAllow If true construction scripts may run on load, otherwise they will be skipped.
	 */
	virtual void SetAllowConstructionScriptsOnLoad(bool bAllow) = 0;

	/**
	 * @return True if construction scripts are allowed to run on load.
	 */
	virtual bool AreConstructionScriptsAllowedOnLoad() const = 0;

	/**
	 *  Signal that a blueprint should or shouldn't run its construction scripts when it is loaded.
	 *  This will stay in effect until removed. This setting is overruled by SetAllowConstructionScriptsOnLoad().
	 *
	 *  @param InPath The full path of the blueprint.
	 *  @param bValue True the blueprint is allowed to run construction scripts on load, false it is not.
	 */
	virtual void SetAllowConstructionScriptsOnLoadForBlueprint(const FString& InPath, bool bValue) = 0;
};
