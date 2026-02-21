// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMTransition.h"

#include "SMGraphK2Node_TransitionResultNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionResultNode : public USMGraphK2Node_RuntimeNodeContainer
{
	GENERATED_UCLASS_BODY()

	static const FName EvalPinName;
	
	UPROPERTY(EditAnywhere, Category = "State Machines")
	FSMTransition TransitionNode;

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

	virtual bool IsNodePure() const override { return true; }
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode

	// USMGraphK2Node_RootNode
	virtual bool IsCompatibleWithInstanceGraphNodeClass(TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const override;
	virtual UEdGraphPin* GetCorrectEntryPin() const override;
	virtual UEdGraphPin* GetCorrectNodeInstanceOutputPin(USMGraphK2Node_FunctionNode_NodeInstance* InInstance) const override;
	virtual bool IsConsideredForEntryConnection() const override { return true; }
	// ~USMGraphK2Node_RootNode
	
	virtual FSMNode_Base* GetRunTimeNode() override { return &TransitionNode; }
	SMSYSTEMEDITOR_API UEdGraphPin* GetTransitionEvaluationPin() const;
};
