// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/Node/ComboNodeBase.h"
#include "ComboRootNode.generated.h"

class UComboRootNodeData;

UCLASS()
class COMBOGRAPHEDITOR_API UComboRootNode : public UComboNodeBase
{
	GENERATED_BODY()

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FLinearColor GetPinColor() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual void CreateDefaultPins() override;
	
	// NEED TO IMPLEMENT
	virtual void InitNodeData(UObject* Outer) override;
	virtual void SetNodeData(UComboGraphNodeData* InNodeData) override;
	virtual UComboGraphNodeData* GetNodeData() const override;
 
public:
	UPROPERTY()
	UComboRootNodeData* NodeData;
};
