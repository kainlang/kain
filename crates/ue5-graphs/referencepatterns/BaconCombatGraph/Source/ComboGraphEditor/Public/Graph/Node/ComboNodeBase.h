// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComboNodeBase.generated.h"

#define UNINITIALIZED_PIN_NAME "Uninitialized"
#define DEFAULT_PIN_CATEGORY "DefaultCategory"
#define INPUT_PIN_NAME "Input"
#define OUTPUT_PIN_NAME "Output"
#define DEFAULT_PIN_NAME "Default"

class UComboGraphNodeData;
class UEdGraphSchema;

UCLASS(Abstract)
class COMBOGRAPHEDITOR_API UComboNodeBase : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UComboNodeBase();
	
	virtual void GetNodeContextMenuActions(class UToolMenu* menu, class UGraphNodeContextMenuContext* context) const override;
	virtual UEdGraphPin* CreateCustomPin(EEdGraphPinDirection direction, FName name, FName Subcategory = DEFAULT_PIN_CATEGORY);
	int GetPinCount(EEdGraphPinDirection Direction);
	void ReservePins(EEdGraphPinDirection Direction, int Count);

	virtual FText GetNodeCreationText() const;
	
	virtual FLinearColor GetPinColor() const;
	virtual FString GetNodePopUpMessage() const;
	virtual FLinearColor GetNodePopUpColor() const;
	virtual FLinearColor GetNodeBodyTintColor() const override;

	// For copy & paste node between graphs
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;

	// Called from associated Slate widget to update time
	virtual void UpdateTime(float DeltaTime);
	void SetNodeActive(bool bActive);
	bool IsNodeActive() const;

	virtual TSharedRef<SWidget> CreateMiddleNodeContent() const;
	virtual TSharedRef<SWidget> CreateBottomNodeContent() const;

	// NEED TO IMPLEMENT
	virtual void CreateDefaultPins();
	virtual bool ValidateNode();
	virtual bool InitPinConnection(UEdGraphPin* OtherNodePin);
	virtual void InitNodeData(UObject* Outer);
	virtual void SetNodeData(UComboGraphNodeData* NodeData);
	virtual UComboGraphNodeData* GetNodeData() const; 
	virtual void RebuildNode();

protected:
	bool bIsNodeActive = false;
	float CurrentActiveTime; 
};