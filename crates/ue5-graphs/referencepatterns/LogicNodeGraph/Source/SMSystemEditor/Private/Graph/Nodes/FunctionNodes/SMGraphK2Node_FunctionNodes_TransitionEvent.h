// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNode.h"
#include "Graph/Nodes/FunctionNodes/SMTransitionEventTypes.h"

#include "Engine/MemberReference.h"

#include "SMGraphK2Node_FunctionNodes_TransitionEvent.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_FunctionNode_TransitionEvent : public USMGraphK2Node_FunctionNode
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY()
	FName DelegatePropertyName;
	
	UPROPERTY()
	TSubclassOf<UObject> DelegateOwnerClass;

	UPROPERTY()
	TEnumAsByte<ESMDelegateOwner> DelegateOwnerInstance;
	
	UPROPERTY()
	FMemberReference EventReference;

	/** Transition class of the transition edge. */
	UPROPERTY()
	TSubclassOf<class USMTransitionInstance> TransitionClass;
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual bool HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const override;
	virtual void ReconstructNode() override;
	//~ UEdGraphNode

	// USMGraphK2Node_Base
	virtual void PostCompileValidate(FCompilerResultsLog& MessageLog) override;
	// ~USMGraphK2Node_Base
	
	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	void SetEventReferenceFromDelegate(FMulticastDelegateProperty* Delegate, ESMDelegateOwner InstanceType);
	
	UFunction* GetDelegateFunction() const;
	void UpdateNodeFromFunction();
};