// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "SMGraphNode_AnyStateNode.generated.h"

/**
 * Nodes without a graph that just serve to transfer their transitions to all other USMGraphNode_StateNodeBase in a single SMGraph.
 */
UCLASS(MinimalAPI)
class USMGraphNode_AnyStateNode : public USMGraphNode_StateNodeBase
{
public:
	GENERATED_UCLASS_BODY()

	/**
	 * Define a query to limit the number of states impacted by this Any State node.
	 * Add tags to each state's AnyStateTags. Only valid in the editor.
	 */
	UPROPERTY(EditAnywhere, Category = "Any State")
	FGameplayTagQuery AnyStateTagQuery;

	/** Chose the color of the Any State, will override any tag colors. */
	UPROPERTY(EditAnywhere, Category = "Any State", meta = (EditCondition = "bOverrideColor", DisplayAfter = "bOverrideColor"))
	FLinearColor AnyStateColor;

	/** Manually choose a color for this Any State. */
	UPROPERTY(EditAnywhere, Category = "Any State")
	uint8 bOverrideColor: 1;
	
	/**
	 * Allows the initial transitions to evaluate even when the active state is an initial state of this node.
	 * Default behavior prevents this.
	 */
	UPROPERTY(EditAnywhere, Category = "Any State")
	uint8 bAllowInitialReentry: 1;
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void OnRenameNode(const FString& NewName) override;
	// ~UEdGraphNode
	
	// USMGraphNode_Base
	virtual FName GetFriendlyNodeName() const override { return "Any State"; }
	virtual void ResetCachedValues() override;
	virtual FString GetNodeName() const override { return GetStateName(); }
	virtual void SetNodeName_Direct(const FString& InNewName) override;
	virtual bool CanExistAtRuntime() const override { return false; }
	virtual UClass* GetNodeClass() const override;
	// ~USMGraphNode_Base

	// USMGraphNode_StateNodeBase
	virtual FString GetStateName() const override;
	// ~USMGraphNode_StateNodeBase

	/** The color is based on the hash of the query, the editor settings, or a custom color. */
	FLinearColor GetAnyStateColor() const;

protected:
	virtual FLinearColor Internal_GetBackgroundColor() const override;
	
private:
	UPROPERTY()
	FText NodeName;

	mutable TOptional<FLinearColor> CachedColor;
};
