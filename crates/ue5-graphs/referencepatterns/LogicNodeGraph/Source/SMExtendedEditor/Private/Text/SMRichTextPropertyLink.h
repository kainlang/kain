// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMRunTypes.h"

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SHyperlink.h"

class FWidgetViewModel;

#if WITH_FANCY_TEXT

class SSMRichTextPropertyLink : public SButton
{
public:
	SLATE_BEGIN_ARGS(SSMRichTextPropertyLink)
		: _Text()
		, _ButtonStyle(&FCoreStyle::Get().GetWidgetStyle< FButtonStyle >("Button"))
		, _TextStyle()
		, _OnPressed()
	{}
	SLATE_ATTRIBUTE(FText, Text)
		/** The visual style of the button */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		/** The text style of the button */
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)

		/** Color of the button */
		SLATE_ARGUMENT(FLinearColor, ButtonColor)

		SLATE_EVENT(FSimpleDelegate, OnPressed)
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, const TSharedRef< FSMPropertyRun::FWidgetViewModel >& InViewModel);

private:
	TSharedPtr< FSMPropertyRun::FWidgetViewModel > ViewModel;
};


#endif //WITH_FANCY_TEXT
