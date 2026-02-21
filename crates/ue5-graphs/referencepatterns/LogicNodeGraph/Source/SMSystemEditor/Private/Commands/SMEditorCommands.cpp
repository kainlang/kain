// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMEditorCommands.h"

#define LOCTEXT_NAMESPACE "SMEditorCommands"

void FSMEditorCommands::RegisterCommands()
{
	UI_COMMAND(GoToGraph, "Go to Local Graph", "Open the graph this node contains within this blueprint", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GoToNodeBlueprint, "Open Node Blueprint", "Edit the blueprint for this node if one is set", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GoToPropertyBlueprint, "Open Property Blueprint", "Edit the blueprint for this property which could be the node blueprint or a state stack blueprint", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GoToTransitionStackBlueprint, "Open Stack Blueprint", "Edit the blueprint for this transition stack element", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(CutAndMergeStates, "Cut and Merge States", "Combine and stack multiple states into this state destroying the originals\nRequires selecting all desired states including the destination state which is the node this menu is opened for", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CopyAndMergeStates, "Copy and Merge States", "Combine and stack multiple states into this state preserving the originals\nRequires selecting all desired states including the destination state which is the node this menu is opened for", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(CreateSelfTransition, "Link to Self", "Creates a transition within the same state", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CollapseToStateMachine, "Collapse to State Machine", "Creates a nested state machine from existing nodes", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertToStateMachineReference, "Convert to State Machine Reference", "Save the selected state machine to disk and reference it", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ChangeStateMachineReference, "Change State Machine Reference", "Change the reference to another state machine", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(JumpToStateMachineReference, "Open State Machine Reference", "Open the state machine reference", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EnableIntermediateGraph, "Enable Intermediate Graph", "Use a graph to manage the state machine entry points", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DisableIntermediateGraph, "Disable Intermediate Graph", "Let the system manage the entry points", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ReplaceWithStateMachine, "State Machine", "Destructively replace the selected node with an empty state machine", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReplaceWithStateMachineParent, "State Machine Parent", "Destructively replace the selected node with the state machine parent", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReplaceWithStateMachineReference, "State Machine Reference", "Destructively replace the selected node with an invalid state machine reference\nSet the reference after using 'Change State Machine Reference'",
		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReplaceWithState, "State", "Destructively replace the selected node with an empty state", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReplaceWithConduit, "Conduit", "Destructively replace the selected node with a conduit", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(GoToPropertyGraph, "Go to Property Graph", "Open the graph for this property", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetGraphProperty, "Reset Property", "Reset the value of the selected graph property to the default", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertPropertyToGraphEdit, "Convert to Graph Edit", "Use the graph to edit the property", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RevertPropertyToNodeEdit, "Revert to Node Edit", "Use the node to edit the property", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(StartSimulateStateMachine, "Simulate", "Simulate the state machine in the editor\nRequires a context actor spawned in", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopSimulateStateMachine, "Stop", "Stop the simulation of a state machine in the editor", EUserInterfaceActionType::Button, FInputChord());
}

const FSMEditorCommands& FSMEditorCommands::Get()
{
	return TCommands<FSMEditorCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
