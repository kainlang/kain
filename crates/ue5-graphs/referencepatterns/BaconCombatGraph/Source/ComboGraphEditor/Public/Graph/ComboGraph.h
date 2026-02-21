// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "ComboGraph.generated.h"

class UComboGraphNodeData;
class UComboNodeBase;

UCLASS()
class COMBOGRAPHEDITOR_API UComboGraph : public UEdGraph
{
    GENERATED_BODY()

public:
	UEdGraphPin* FindPinById(const FGuid& PinId) const;
	UComboNodeBase* FindNodeByData(const UComboGraphNodeData* NodeData) const;
};