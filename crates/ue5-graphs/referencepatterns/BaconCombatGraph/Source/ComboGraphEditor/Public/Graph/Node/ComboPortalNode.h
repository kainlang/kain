// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/Node/ComboNodeBase.h"
#include "ComboPortalNode.generated.h"

class UComboPortalNodeData;

UCLASS()
class COMBOGRAPHEDITOR_API UComboPortalNode : public UComboNodeBase
{
	GENERATED_BODY()

public:
	virtual FText GetNodeCreationText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	virtual void CreateDefaultPins() override;

	// NEED TO IMPLEMENT
	virtual void InitNodeData(UObject* Outer) override;
	virtual void SetNodeData(UComboGraphNodeData* InNodeData) override;
	virtual UComboGraphNodeData* GetNodeData() const override; 
	virtual void RebuildNode() override;

protected:
	UPROPERTY()
	UComboPortalNodeData* NodeData; 
};
