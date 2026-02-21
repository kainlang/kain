// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMUnrealTypeDefs.h"
#include "Framework/Commands/Commands.h"

class FSMEditorCommands : public TCommands<FSMEditorCommands>
{
public:
	/** Constructor */
	FSMEditorCommands()
		: TCommands<FSMEditorCommands>(TEXT("SMEditor"), NSLOCTEXT("Contexts", "SMEditor", "Logic Driver State Machine Editor"),
			NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
	{
	}

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMEditorCommands& Get();
	// ~TCommand

	/** Go to the graph for this node. */
	TSharedPtr<FUICommandInfo> GoToGraph;

	/** Go to the blueprint for this node. */
	TSharedPtr<FUICommandInfo> GoToNodeBlueprint;
	
	/** Create transition from Node A - Node A. */
	TSharedPtr<FUICommandInfo> CreateSelfTransition;

	/** Create a nested state machine from existing nodes. */
	TSharedPtr<FUICommandInfo> CollapseToStateMachine;

	/** Combine multiple states into the same state stack. */
	TSharedPtr<FUICommandInfo> CutAndMergeStates;

	/** Combine multiple states into the same state stack. */
	TSharedPtr<FUICommandInfo> CopyAndMergeStates;
	
	/** Converts a nested state machine to a referenced state machine. */
	TSharedPtr<FUICommandInfo> ConvertToStateMachineReference;

	/** Replace the reference a state machine points to. */
	TSharedPtr<FUICommandInfo> ChangeStateMachineReference;

	/** Open the reference blueprint. */
	TSharedPtr<FUICommandInfo> JumpToStateMachineReference;

	/** Signals to use an intermediate graph. */
	TSharedPtr<FUICommandInfo> EnableIntermediateGraph;

	/** Signals to stop using an intermediate graph. */
	TSharedPtr<FUICommandInfo> DisableIntermediateGraph;

	/** Replace node with state machine. */
	TSharedPtr<FUICommandInfo> ReplaceWithStateMachine;
	
	/** Replace node with state machine reference. */
	TSharedPtr<FUICommandInfo> ReplaceWithStateMachineReference;

	/** Replace node with state machine parent. */
	TSharedPtr<FUICommandInfo> ReplaceWithStateMachineParent;
	
	/** Replace node with state. */
	TSharedPtr<FUICommandInfo> ReplaceWithState;

	/** Replace node with state conduit. */
	TSharedPtr<FUICommandInfo> ReplaceWithConduit;

	/** Clear a graph property. */
	TSharedPtr<FUICommandInfo> ResetGraphProperty;

	/** Go to a graph property. */
	TSharedPtr<FUICommandInfo> GoToPropertyGraph;

	/** Go to the blueprint for this property. Could be the node blueprint or a state stack blueprint. */
	TSharedPtr<FUICommandInfo> GoToPropertyBlueprint;

	/** Go to the blueprint for this transition stack element. */
	TSharedPtr<FUICommandInfo> GoToTransitionStackBlueprint;
	
	/** Use the graph to edit. */
	TSharedPtr<FUICommandInfo> ConvertPropertyToGraphEdit;

	/** Use the node to edit. */
	TSharedPtr<FUICommandInfo> RevertPropertyToNodeEdit;

	/** Start simulation of state machine during preview*/
	TSharedPtr<FUICommandInfo> StartSimulateStateMachine;

	/** Stop simulation of state machine during preview*/
	TSharedPtr<FUICommandInfo> StopSimulateStateMachine;
};
