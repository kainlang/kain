// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "SMGraphNode_LinkStateNode.generated.h"

class FSMKismetCompilerContext;

/**
 * Nodes without a graph that just serve to transfer their transitions to the state they reference.
 */
UCLASS(MinimalAPI, HideCategories = (Class, Display))
class USMGraphNode_LinkStateNode : public USMGraphNode_StateNodeBase
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "Linked State")
	FString LinkedStateName;

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	// ~UEdGraphNode
	
	// USMGraphNode_Base
	virtual void PreCompile(FSMKismetCompilerContext& CompilerContext) override;
	virtual FName GetFriendlyNodeName() const override { return "Link State"; }
	virtual const FSlateBrush* GetNodeIcon() const override;
	virtual void ResetCachedValues() override;
	virtual bool CanExistAtRuntime() const override { return false; }
	// ~USMGraphNode_Base

	// USMGraphNode_StateNodeBase
	virtual FString GetStateName() const override;
	virtual bool IsEndState(bool bCheckAnyState) const override;
	// ~USMGraphNode_StateNodeBase

	/** Reference another state. */
	SMSYSTEMEDITOR_API void LinkToState(const FString& InStateName);

	/** Return all possible states that can be referenced by this node. */
	SMSYSTEMEDITOR_API void GetAvailableStatesToLink(TArray<USMGraphNode_StateNodeBase*>& OutStates) const;

	/** Find the referenced state object. If null the state doesn't exist within the owning graph. */
	SMSYSTEMEDITOR_API USMGraphNode_StateNodeBase* GetLinkedStateFromName(const FString& InName) const;

	/** The actual state this node is linking to. */
	FORCEINLINE USMGraphNode_StateNodeBase* GetLinkedState() const { return LinkedState; }

	/** Checks if the current referenced state object is valid for this node. */
	SMSYSTEMEDITOR_API bool IsLinkedStateValid() const;

	/** The color of the reference node. */
	FLinearColor GetStateColor() const;

	/** Called when the state this node links to is renamed. */
	void OnLinkedStateRenamed();

protected:
	virtual FLinearColor Internal_GetBackgroundColor() const override;

private:
	mutable TOptional<FLinearColor> CachedColor;

	UPROPERTY()
	TObjectPtr<USMGraphNode_StateNodeBase> LinkedState;
};
