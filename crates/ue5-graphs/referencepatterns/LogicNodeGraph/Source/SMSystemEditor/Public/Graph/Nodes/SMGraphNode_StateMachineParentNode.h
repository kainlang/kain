// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMGraphNode_StateMachineStateNode.h"

#include "SMInstance.h"

#include "SMGraphNode_StateMachineParentNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphNode_StateMachineParentNode final : public USMGraphNode_StateMachineStateNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Parent State Machine", NoClear, meta = (NoResetToDefault))
	TSubclassOf<USMInstance> ParentClass;

	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	// ~UEdGraphNode

	// USMGraphNode_StateMachineStateNode
	virtual void CreateBoundGraph() override;
	virtual void UpdateEditState() override;
	virtual bool SupportsPropertyGraphs() const override { return false; }
	virtual bool ReferenceStateMachine(USMBlueprint* OtherStateMachine) override { return false; }
	virtual void InitStateMachineReferenceTemplate(bool bInitialLoad) override {}
	// ~USMGraphNode_StateMachineStateNode

	void SetParentIfNull();

	/** Builds all nested graphs of parents that have been expanded. Only valid during compile after ExpandParentNodes() */
	TSet<USMGraph*> GetAllNestedExpandedParents() const;

	/** A cloned graph of the parent. Only valid during compile after ExpandParentNodes(). */
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<USMGraph> ExpandedGraph;
	
protected:
	// USMGraphNode_StateNodeBase
	virtual FLinearColor Internal_GetBackgroundColor() const override;
	// ~USMGraphNode_StateNodeBase
};
