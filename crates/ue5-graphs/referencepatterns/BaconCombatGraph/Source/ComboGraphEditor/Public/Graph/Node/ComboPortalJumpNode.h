// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/Node/ComboNodeBase.h"
#include "ComboPortalJumpNode.generated.h"

class UComboJumpNodeData;

UCLASS()
class COMBOGRAPHEDITOR_API UComboPortalJumpNode : public UComboNodeBase
{
	GENERATED_BODY()

public:
	virtual FText GetNodeCreationText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void CreateDefaultPins() override;

	// NEED TO IMPLEMENT
	virtual void InitNodeData(UObject* Outer) override;
	virtual void SetNodeData(UComboGraphNodeData* InNodeData) override;
	virtual UComboGraphNodeData* GetNodeData() const override; 
	virtual void RebuildNode() override;

protected:
	UPROPERTY()
	UComboJumpNodeData* NodeData;
};