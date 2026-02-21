// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

class UEdGraphNode;
class USMEditorInstance;
struct FSMNode_Base;

struct FSMEditorStateMachine
{
	/** The sm instance used during editor time. */
	USMEditorInstance* StateMachineEditorInstance;
	
	/** Storage for all editor runtime nodes. This memory is manually managed! */
	TArray<FSMNode_Base*> EditorInstanceNodeStorage;

	/** Created runtime nodes mapped to their graph node. */
	TMap<FSMNode_Base*, UEdGraphNode*> RuntimeNodeToGraphNode;
};
