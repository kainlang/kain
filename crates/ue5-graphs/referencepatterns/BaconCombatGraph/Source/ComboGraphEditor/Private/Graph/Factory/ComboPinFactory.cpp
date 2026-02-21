// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Factory/ComboPinFactory.h" 
#include "Graph/Node/ComboNodeBase.h"
 
FComboPinFactory::~FComboPinFactory()
{
}

TSharedPtr<SGraphPin> FComboPinFactory::CreatePin(UEdGraphPin* Pin) const 
{ 
	if (Pin == nullptr)
	{
		return nullptr;
	}

	UComboNodeBase* Node = Cast<UComboNodeBase>(Pin->GetOwningNodeUnchecked());
	if (!IsValid(Node))
	{
		return nullptr;
	}

	return SNew(SComboGraphPin, Pin);
}

void SComboGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	PinColor = FSlateColor(FLinearColor::White);
	if (InGraphPinObj != nullptr)
	{
		UComboNodeBase* Node = Cast<UComboNodeBase>(InGraphPinObj->GetOwningNodeUnchecked());
		if (IsValid(Node))
		{
			PinColor = Node->GetPinColor();
		}
	}

	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

FSlateColor SComboGraphPin::GetPinColor() const
{
	return PinColor;
}
