// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_NodeInstance.h"

#include "SMGraphK2Node_FunctionNodes_ConduitInstance.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_ConduitInstance_Base : public USMGraphK2Node_FunctionNode_NodeInstance
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode
};

UCLASS(MinimalAPI)
class USMGraphK2Node_ConduitInstance_CanEnterTransition : public USMGraphK2Node_ConduitInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsNodePure() const override { return true; }
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override;
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};

UCLASS(MinimalAPI)
class USMGraphK2Node_ConduitInstance_OnConduitEntered : public USMGraphK2Node_ConduitInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override;
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};

UCLASS(MinimalAPI)
class USMGraphK2Node_ConduitInstance_OnConduitInitialized : public USMGraphK2Node_ConduitInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override;
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};

UCLASS(MinimalAPI)
class USMGraphK2Node_ConduitInstance_OnConduitShutdown : public USMGraphK2Node_ConduitInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override;
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};