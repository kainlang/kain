// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_NodeInstance.h"

#include "SMGraphK2Node_FunctionNodes_StateInstance.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_StateInstance_Base : public USMGraphK2Node_FunctionNode_NodeInstance
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode
};

UCLASS(MinimalAPI)
class USMGraphK2Node_StateInstance_Begin : public USMGraphK2Node_StateInstance_Base
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
class USMGraphK2Node_StateInstance_Update : public USMGraphK2Node_StateInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
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
class USMGraphK2Node_StateInstance_End : public USMGraphK2Node_StateInstance_Base
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
class USMGraphK2Node_StateInstance_StateMachineStart : public USMGraphK2Node_StateInstance_Base
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
class USMGraphK2Node_StateInstance_StateMachineStop : public USMGraphK2Node_StateInstance_Base
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
class USMGraphK2Node_StateInstance_OnStateInitialized : public USMGraphK2Node_StateInstance_Base
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
class USMGraphK2Node_StateInstance_OnStateShutdown : public USMGraphK2Node_StateInstance_Base
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
