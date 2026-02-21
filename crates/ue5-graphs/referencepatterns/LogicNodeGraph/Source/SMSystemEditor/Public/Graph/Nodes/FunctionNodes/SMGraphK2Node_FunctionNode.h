// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMGraphK2Node_FunctionNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_FunctionNode : public USMGraphK2Node_RuntimeNodeReference
{
public:
	GENERATED_UCLASS_BODY()
	
	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	virtual bool IsNodePure() const override { return false; }
	virtual FText GetMenuCategory() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	//~ UEdGraphNode

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return true; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return true; }
	virtual UEdGraphPin* GetInputPin() const override;
	// ~UEdGraphNode

	virtual bool ExpandAndWireStandardFunction(UFunction* Function, UEdGraphPin* SelfPin, FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty);
};
