// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMGraphK2Node_StateReadNodes.generated.h"

class USMGraphNode_StateNodeBase;

UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode : public USMGraphK2Node_RuntimeNodeReference
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	virtual bool IsNodePure() const override { return true; }
	virtual FText GetMenuCategory() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	//~ UEdGraphNode

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return true; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return true; }
	// ~UEdGraphNode

	/** Returns either the current state or the FromState of a transition. */
	virtual FString GetMostRecentStateName() const;

	/** Returns the current transition name. Only valid if in a transition graph. */
	virtual FString GetTransitionName() const;

	virtual USMGraphNode_StateNodeBase* GetMostRecentState() const;
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_HasStateUpdated : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_TimeInState : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_CanEvaluate : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_CanEvaluateFromEvent : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_GetStateInformation : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference
};

UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_GetTransitionInformation : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference
};

UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_GetStateMachineReference : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void PostPasteNode() override;
	virtual bool HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	TSubclassOf<UObject> GetStateMachineReferenceClass() const;

	/** The class type this is referencing. The output pin will be dynamic cast to this.
	 * When force replacing references this can cause warnings, but is present in other UE4 blueprints. 
	 */
	UPROPERTY()
	TSubclassOf<UObject> ReferencedObject;
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateMachineReadNode : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	// ~UEdGraphNode
};


UCLASS(MinimalAPI, meta = (Keywords="Has,Reached"))
class USMGraphK2Node_StateMachineReadNode_InEndState : public USMGraphK2Node_StateMachineReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UEdGraphNode
};

UCLASS(MinimalAPI)
class USMGraphK2Node_StateReadNode_GetNodeInstance : public USMGraphK2Node_StateReadNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void PostPasteNode() override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// ~UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	void AllocatePinsForType(TSubclassOf<UObject> TargetType);

	UEdGraphPin* GetInstancePinChecked() const;

	bool RequiresInstance() const { return !bCanCreateNodeInstanceOnDemand || NodeInstanceIndex >= 0; }
	
	/**
	 * The class type this is referencing. The output pin will be dynamic cast to this.
	 * When force replacing references this can cause warnings, but is present in other UE4 blueprints. 
	 */
	UPROPERTY()
	TSubclassOf<UObject> ReferencedObject;

	/** The guid of a specific node instance. Used for stack state instances. */
	UPROPERTY()
	FGuid NodeInstanceGuid;

	UPROPERTY()
	int32 NodeInstanceIndex;

	/**
	 * When true an instance is not required during run-time (function access) and will be created on demand.
	 * When false the instance is assumed to always be created (struct access) and will not create on demand.
	 * No effect when used with state stack instances.
	 */
	UPROPERTY()
	bool bCanCreateNodeInstanceOnDemand;

	/** Wire nodes related to the node instance. Will determine whether the instance should be allowed to be created on demand or not. */
	static void CreateAndWireExpandedNodes(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class, FSMKismetCompilerContext& CompilerContext,
		USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty, class UK2Node_DynamicCast** CastOutputNode);

private:
	/** Wire nodes related to retrieving the node instance. A node instance is not required for this call and will be created on demand. */
	static void CreateAndWireExpandedNodesWithFunction(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class, FSMKismetCompilerContext& CompilerContext,
		USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty, class UK2Node_DynamicCast** CastOutputNode);

	/** Wire nodes related to retrieving the node instance. A node instance is required at the time of this call there will be run-time errors if the instance is invalid. */
	static void CreateAndWireExpandedNodesWithStruct(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class, FSMKismetCompilerContext& CompilerContext,
		USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty, class UK2Node_DynamicCast** CastOutputNode);
};