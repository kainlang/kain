// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/Node/ComboNodeBase.h" 
#include "ComboInputNode.generated.h"

class UComboInputNodeData;

UCLASS()
class COMBOGRAPHEDITOR_API UComboInputNode : public UComboNodeBase
{
	GENERATED_BODY()

public:
	virtual FText GetNodeCreationText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	virtual void CreateDefaultPins() override;
	virtual TSharedRef<SWidget> CreateBottomNodeContent() const override;

	// NEED TO IMPLEMENT
	virtual void InitNodeData(UObject* Outer) override;
	virtual void SetNodeData(UComboGraphNodeData* InNodeData) override;
	virtual UComboGraphNodeData* GetNodeData() const override; 
	virtual void RebuildNode() override;

protected:
	UPROPERTY()
	UComboInputNodeData* NodeData;
};