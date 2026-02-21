// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "SMStateMachine.h"

#include "SMGraphNode_StateMachineEntryNode.generated.h"

/** Created for normal state machine UEdGraphs. */
UCLASS(MinimalAPI, HideCategories = (Class, Display))
class USMGraphNode_StateMachineEntryNode : public USMGraphNode_StateNodeBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FSMStateMachine StateMachineNode;

	/** Allow more than one initial state. Setting this to false will clear all but one initial state. */
	UPROPERTY(EditAnywhere, Category = "Parallel States")
	bool bAllowParallelEntryStates;
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UEdGraphNode

	// USMGraphNode_Base
	virtual UClass* GetNodeClass() const override;
	virtual bool CanRunConstructionScripts() const override { return false; }
	virtual bool CanExistAtRuntime() const override { return false; }
	// ~USMGraphNode_Base
};
