// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "SMGraphNode_RerouteNode.generated.h"

class FSMKismetCompilerContext;

/**
 * Editor cosmetic nodes to reroute a single transition. When connected through transitions only a single primary transition
 * graph is used. All other properties are copied from the primary transition. Does not impact run-time in anyway.
 */
UCLASS(MinimalAPI, HideCategories = (Class, Display), meta = (NoLogicDriverExport))
class USMGraphNode_RerouteNode : public USMGraphNode_StateNodeBase
{
public:
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool GetCanRenameNode() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual UEdGraphPin* GetPassThroughPin(const UEdGraphPin* FromPin) const override;
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override { OutInputPinIndex = 0;  OutOutputPinIndex = 1; return true; }
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override { return false; }
	virtual bool IsCompilerRelevant() const override { return false; }
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// ~UEdGraphNode
	
	// USMGraphNode_Base
	virtual void PreCompile(FSMKismetCompilerContext& CompilerContext) override;
	virtual void UpdateTime(float DeltaTime) override;
	virtual FName GetFriendlyNodeName() const override { return "Reroute Node"; }
	virtual FString GetNodeName() const override;
	virtual const FSlateBrush* GetNodeIcon() const override;
	virtual bool CanGoToLocalGraph() const override;
	virtual UClass* GetNodeClass() const override;
	virtual bool CanExistAtRuntime() const override { return false; }
	virtual TArray<TSubclassOf<USMGraphNode_Base>> GetAllowedDuplicateNodeNameTypes() const override;
	// ~USMGraphNode_Base

	// USMGraphNode_StateNodeBase
	virtual bool IsEndState(bool bCheckAnyState) const override;
	// ~USMGraphNode_StateNodeBase

	/** Attempt to return the primary transition this reroute node represents. */
	USMGraphNode_TransitionEdge* GetPrimaryTransition() const;

	/** Return all transitions and reroute nodes, both before, after, and including this transition. */
	void GetAllReroutedTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions, TArray<USMGraphNode_RerouteNode*>& OutRerouteNodes) const;

	/** Checks if this node has incoming and outgoing transitions. */
	bool IsThisRerouteValid() const;

	/** Checks if there are no incoming and no outgoing transitions. */
	bool IsRerouteEmpty() const;

	/** Break any connections to reroute nodes. */
	void BreakAllOutgoingReroutedConnections();

protected:
	virtual FLinearColor Internal_GetBackgroundColor() const override;
};
