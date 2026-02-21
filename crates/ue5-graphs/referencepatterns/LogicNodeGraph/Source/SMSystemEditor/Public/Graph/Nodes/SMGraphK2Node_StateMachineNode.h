// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphK2Node_Base.h"

#include "EdGraph/EdGraphNodeUtils.h"

#include "SMGraphK2Node_StateMachineNode.generated.h"

class USMGraph;
class USMGraphK2;

UCLASS(MinimalAPI)
class USMGraphK2Node_StateMachineNode : public USMGraphK2Node_Base
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UK2Node Interface
	virtual void AllocateDefaultPins() override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	/** Limit blueprints this shows up in. */
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsNodePure() const override;
	/** Required to show up in BP right click context menu. */
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return false; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return false; }
	// ~USMGraphK2Node_Base

	FString GetStateMachineName() const;
	USMGraph* GetStateMachineGraph() const { return BoundGraph; }
	USMGraphK2* GetTopLevelStateMachineGraph() const;
	
protected:
	UPROPERTY()
	TObjectPtr<USMGraph> BoundGraph;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedFullTitle;
};
