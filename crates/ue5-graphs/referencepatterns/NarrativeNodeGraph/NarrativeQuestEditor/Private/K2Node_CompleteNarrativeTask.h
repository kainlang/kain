// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CompleteNarrativeTask.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class NARRATIVEQUESTEDITOR_API UK2Node_CompleteNarrativeTask : public UK2Node
{
	GENERATED_BODY()

public:

	//UEdGraphNode implementation
	virtual void ReconstructNode() override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//UEdGraphNode implementation

	//K2Node implementation
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual FText GetToolTipHeading() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//K2Node implementation

	UEdGraphPin* GetTargetPin();
	UEdGraphPin* GetActionPin();
	UEdGraphPin* GetArgumentPin();
	UEdGraphPin* GetUpdatedQuestPin();
	UEdGraphPin* GetQuantityPin();

	FNodeTextCache CachedArgumentPinFriendlyName;

};
