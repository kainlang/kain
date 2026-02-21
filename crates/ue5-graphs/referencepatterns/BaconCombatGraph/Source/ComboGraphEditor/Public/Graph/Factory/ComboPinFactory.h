// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"

struct COMBOGRAPHEDITOR_API FComboPinFactory : public FGraphPanelPinFactory 
{
public:
	virtual ~FComboPinFactory();
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* Pin) const override;
};

class COMBOGRAPHEDITOR_API SComboGraphPin : public SGraphPin 
{
public:
	SLATE_BEGIN_ARGS(SComboGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	virtual FSlateColor GetPinColor() const override;

protected:
	FSlateColor PinColor;
};