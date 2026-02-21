// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphK2Node_FunctionNodes_ConduitInstance.h"

#include "SMTransitionInstance.h"

#include "SMGraphK2Node_FunctionNodes_TransitionInstance.generated.h"

class USMGraphNode_TransitionEdge;

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionInstance_Base : public USMGraphK2Node_FunctionNode_NodeInstance
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode
};

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionInstance_CanEnterTransition : public USMGraphK2Node_TransitionInstance_Base
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
	virtual FName GetInstanceRuntimeFunctionName() const override { return GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, CanEnterTransition); }
	// ~USMGraphK2Node_FunctionNode_NodeInstance
	
	/** Retrieve the boolean result pin. */
	SMSYSTEMEDITOR_API UEdGraphPin* GetReturnValuePinChecked() const;
};

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionStackInstance_CanEnterTransition final : public USMGraphK2Node_TransitionInstance_CanEnterTransition
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	virtual void PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual UClass* GetNodeInstanceClass() const override;
	// ~USMGraphK2Node_FunctionNode_NodeInstance

	/** Return the owning transition edge from the graph. */
	USMGraphNode_TransitionEdge* GetTransitionEdge() const;
	
	/** Set the node stack guid this node represents. */
	void SetNodeStackGuid(const FGuid& InGuid) { TransitionStackTemplateGuid = InGuid; }

	/** Return the stack guid. */
	const FGuid& GetNodeStackGuid() const { return TransitionStackTemplateGuid; }
	
private:
	/** The guid of the node stack. */
	UPROPERTY()
	FGuid TransitionStackTemplateGuid;

	/** Index in the stack, calculated on pre compile. */
	UPROPERTY()
	int32 StackIndex = 0;
};

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionInstance_OnTransitionTaken : public USMGraphK2Node_TransitionInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override { return GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, OnTransitionEntered); }
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionInstance_OnTransitionInitialized : public USMGraphK2Node_TransitionInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override { return GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, OnTransitionInitialized); }
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionInstance_OnTransitionShutdown : public USMGraphK2Node_TransitionInstance_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// USMGraphK2Node_FunctionNode_NodeInstance
	virtual FName GetInstanceRuntimeFunctionName() const override { return GET_FUNCTION_NAME_CHECKED(USMTransitionInstance, OnTransitionShutdown); }
	// ~USMGraphK2Node_FunctionNode_NodeInstance
};
