// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"

#include "SMGraph.generated.h"

class USMGraphK2Node_StateMachineNode;
class USMGraphK2Node_StateMachineEntryNode;
class USMGraphNode_StateMachineEntryNode;
class USMGraphNode_StateMachineStateNode;
struct FSMNode_Base;

UCLASS()
class SMSYSTEMEDITOR_API USMGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraph Interface
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	virtual void NotifyGraphChanged() override;
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action) override;
	//~ End UEdGraph Interface

	USMGraphNode_StateMachineEntryNode* GetEntryNode() const;

	/** When referencing from top level or state graph. */
	USMGraphK2Node_StateMachineNode* GetOwningStateMachineK2Node() const;

	/** When referencing from a nested definition. */
	USMGraphNode_StateMachineStateNode* GetOwningStateMachineNodeWhenNested() const;

	/** Checks the graph node owning this graph and returns the runtime state. */
	virtual FSMNode_Base* GetRuntimeNode() const;

	/** Checks if the entry node is connected to any state. */
	bool HasAnyLogicConnections() const;

public:
	// Set by compiler -- stored on consolidated graph.
	UPROPERTY(Transient)
	TObjectPtr<USMGraphK2Node_StateMachineEntryNode> GeneratedContainerNode;
	
	// Entry node within the state machine
	UPROPERTY()
	TObjectPtr<USMGraphNode_StateMachineEntryNode> EntryNode;
};
