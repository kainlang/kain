// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMStateMachine.h"

#include "SMGraphK2Node_StateMachineEntryNode.generated.h"

/** Created by compiler for nested state machine entry points. */
UCLASS(MinimalAPI)
class USMGraphK2Node_StateMachineEntryNode : public USMGraphK2Node_RuntimeNodeContainer
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FSMStateMachine StateMachineNode;

	virtual FSMNode_Base* GetRunTimeNode()  override { return &StateMachineNode; }

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	// ~UEdGraphNode
};
