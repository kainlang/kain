// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "ComboNodeActions.generated.h"

class UComboGraphNodeData;
class UEdGraphNode;

USTRUCT()
struct FNewComboNodeAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()

public:
	FNewComboNodeAction() : FEdGraphSchemaAction() {}
	FNewComboNodeAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) {}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	static TSharedPtr<FNewComboNodeAction> GetNewNodeAction(TSubclassOf<UComboGraphNodeData> NodeClass, FText Category, FText Description);

public:
	TSubclassOf<UComboGraphNodeData> NodeClass;
};