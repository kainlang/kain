// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2Node_FunctionNode.h"

#include "SMGraphK2Node_FunctionNodes_NodeInstance.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_FunctionNode_NodeInstance : public USMGraphK2Node_FunctionNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetTooltipText() const override;
	virtual void AllocateDefaultPins() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog) override;
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode
	/** Creates a function node and wires execution pins. The self pin can be null and will be used from the auto created cast node. */
	virtual bool ExpandAndWireStandardFunction(UFunction* Function, UEdGraphPin* SelfPin, FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_FunctionNode

	/** Return the function name to expect. Such as 'OnStateBegin'. */
	virtual FName GetInstanceRuntimeFunctionName() const { return NAME_None; }

	/** Return the appropriate node instance class to use based on the compile status. */
	virtual UClass* GetNodeInstanceClass() const;

protected:
	UPROPERTY()
	TSubclassOf<UObject> NodeInstanceClass;
};
