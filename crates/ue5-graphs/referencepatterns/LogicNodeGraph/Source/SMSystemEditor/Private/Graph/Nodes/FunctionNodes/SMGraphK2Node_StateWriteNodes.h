// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMGraphK2Node_StateWriteNodes.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_StateWriteNode : public USMGraphK2Node_RuntimeNodeReference
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	virtual bool IsNodePure() const override { return false; }
	virtual FText GetMenuCategory() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	//~ UEdGraphNode

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return true; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return true; }
	virtual UEdGraphPin* GetInputPin() const override;
	// ~UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateWriteNode_CanEvaluate : public USMGraphK2Node_StateWriteNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateWriteNode_CanEvaluateFromEvent : public USMGraphK2Node_StateWriteNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	//~ UEdGraphNode
};


UCLASS(MinimalAPI)
class USMGraphK2Node_StateWriteNode_TransitionEventReturn : public USMGraphK2Node_StateWriteNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	bool bEventTriggersUpdate_DEPRECATED;

	/** Use the settings of the owning transition node to determine event updates. */
	UPROPERTY(EditAnywhere, Category = "Transition Event")
	bool bUseOwningTransitionSettings;
	
	/**
	 * If the event should trigger a targeted update of the state machine limited to this
	 * transition and destination state.
	 * 
	 * This can efficiently allow state machines with tick disabled to update. This
	 * won't evaluate parallel or super state transitions.
	 */
	UPROPERTY(EditAnywhere, Category = "Transition Event", meta = (DisplayName = "Targeted Update", EditCondition = "!bUseOwningTransitionSettings", EditConditionHides))
	bool bEventTriggersTargetedUpdate;

	/**
	 * If the event should trigger a full update of the state machine. Setting this will be applied
	 * after 'Targeted Update'. A full update consists of evaluating transitions top down from the
	 * root state machine, as well as running OnStateUpdate if necessary.
	 *
	 * This is a legacy setting. To maintain old legacy behavior enable this setting and
	 * disable 'Targeted Update'.
	 */
	UPROPERTY(EditAnywhere, Category = "Transition Event", meta = (DisplayName = "Full Update", EditCondition = "!bUseOwningTransitionSettings", EditConditionHides))
	bool bEventTriggersFullUpdate;

	// UObject
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UObject
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool DrawNodeAsExit() const override { return true; }
	virtual FText GetTooltipText() const override;
	//~ UEdGraphNode

	// USMGraphK2Node_RuntimeNodeReference
	virtual bool HandlesOwnExpansion() const override { return true; }
	virtual void CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty) override;
	// ~USMGraphK2Node_RuntimeNodeReference

protected:
	/** Use the owning transition's event settings if allowed. */
	void UpdateEventSettingsFromTransition();
};
