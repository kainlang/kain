// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Construction/ISMEditorConstructionManager.h"
#include "Construction/SMEditorConstructionConfiguration.h"
#include "Construction/SMEditorStateMachine.h"

#include "TickableEditorObject.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class USMBlueprint;
class USMGraph;
struct FSMStateMachine;

/**
 * Construction manager singleton for running construction scripts and building editor state machines.
 */
class FSMEditorConstructionManager : public ISMEditorConstructionManager, public FTickableEditorObject
{
public:
	FSMEditorConstructionManager(FSMEditorConstructionManager const&) = delete;
	void operator=(FSMEditorConstructionManager const&) = delete;

	/** Cancels RunAllConstructionScriptsForBlueprint if true. */
	UE_DEPRECATED(5.1, "Do not access directly. Use `SetEnableConstructionScripts()` or `AreConstructionScriptsEnabled()`.")
	bool bDisableConstructionScripts = false;
	
private:
	FSMEditorConstructionManager() {}

public:
	virtual ~FSMEditorConstructionManager() override;

	/** Access the Construction Manager. */
	UE_DEPRECATED(5.1, "Do not access directly from the private header. Use the public ISMEditorConstructionManager::Get() accessor instead.")
	SMSYSTEMEDITOR_API static FSMEditorConstructionManager* GetInstance();

	// FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual TStatId GetStatId() const override;
	// ~FTickableEditorObject

	// ISMEditorConstructionManager
	virtual void SetEnableConstructionScripts(const bool bEnable) override;
	virtual bool AreConstructionScriptsEnabled() const override;
	virtual bool HasPendingConstructionScripts() const override;
	virtual bool IsRunningConstructionScripts(USMBlueprint* ForBlueprint = nullptr) const override;
	virtual void CleanupAllEditorStateMachines() override;
	virtual void CleanupEditorStateMachine(USMBlueprint* InBlueprint) override;
	virtual void RunAllConstructionScriptsForBlueprintImmediately(USMBlueprint* InBlueprint, bool bCleanupEditorStateMachine = true) override;
	virtual void RunAllConstructionScriptsForBlueprint(UObject* InObject, const FSMConstructionConfiguration& InConfiguration = FSMConstructionConfiguration()) override;
	virtual void QueueBlueprintForConditionalCompile(USMBlueprint* InBlueprint, const FSMConditionalCompileConfiguration& InConfiguration = FSMConditionalCompileConfiguration()) override;
	virtual FSMEditorStateMachine& CreateEditorStateMachine(USMBlueprint* InBlueprint) override;
	virtual bool TryGetEditorStateMachine(USMBlueprint* InBlueprint, FSMEditorStateMachine& OutEditorStateMachine) override;
	virtual void SetAllowConstructionScriptsOnLoad(bool bAllow) override;
	virtual bool AreConstructionScriptsAllowedOnLoad() const override;
	virtual void SetAllowConstructionScriptsOnLoadForBlueprint(const FString& InPath, bool bValue) override;
	// ~ISMEditorConstructionManager
	
private:
	/**
	 * Recursively build out a state machine from an editor graph. This is executed this frame.
	 *
	 * @param InGraph A state machine editor graph.
	 * @param StateMachineOut The outgoing state machine being assembled. This should be the root.
	 * @param EditorStateMachineInOut Heap memory will be initialized here. This memory MUST be freed manually to prevent a memory leak.
	 */
	void ConstructEditorStateMachine(USMGraph* InGraph, FSMStateMachine& StateMachineOut, FSMEditorStateMachine& EditorStateMachineInOut);
	
	/** Configure the initial root FSM for a state machine blueprint. */
	void SetupRootStateMachine(FSMStateMachine& StateMachineInOut, const USMBlueprint* InBlueprint) const;

	/** If the given blueprint qualifies for conditional compile. */
	bool CanConditionallyCompileBlueprint(USMBlueprint* InBlueprint) const;

	/**
	 * Assemble editor state machines and run construction scripts this frame.
	 *
	 * @param InBlueprint The blueprint to run all construction scripts for.
	 * @param InConfigurationData Configuration data to apply.
	 *
	 * @return True if construction scripts ran.
	 */
	bool RunAllConstructionScriptsForBlueprint_Internal(USMBlueprint* InBlueprint, const FSMConstructionConfiguration& InConfigurationData);

	/**
	 * Conditionally compile the blueprint this frame if possible.
	 *
	 * @param InBlueprint The blueprint to run all construction scripts for.
	 * @param InConfiguration Configuration data to apply.
	 */
	void ConditionalCompileBlueprint_Internal(USMBlueprint* InBlueprint, const FSMConditionalCompileConfiguration& InConfiguration);

private:
	/** Loaded blueprints mapped to their editor state machine. */
	TMap<TWeakObjectPtr<USMBlueprint>, FSMEditorStateMachine> EditorStateMachines;

	/** All blueprints waiting to have their construction scripts run. */
	TMap<TWeakObjectPtr<USMBlueprint>, FSMConstructionConfiguration> BlueprintsPendingConstruction;

	/** All blueprints in process of being constructed for a frame. */
	TSet<TWeakObjectPtr<USMBlueprint>> BlueprintsBeingConstructed;

	/** Blueprints which should run a conditional compile. */
	TMap<TWeakObjectPtr<USMBlueprint>, FSMConditionalCompileConfiguration> BlueprintsToConditionallyCompile;

	/** The path of blueprints which shouldn't have their construction scripts run on load. */
	TSet<FString> BlueprintsToSkipConstructionScriptsOnLoad;

	/** Disable or enable construction scripts to run if triggered during load. */
	bool bAllowConstructionScriptsOnLoad = true;
};
