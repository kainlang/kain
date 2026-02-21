// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMRichTextPropertyLink.h"

#include "Widgets/Text/STextBlock.h"

void SSMRichTextPropertyLink::Construct(const FArguments& InArgs, const TSharedRef< FSMPropertyRun::FWidgetViewModel >& InViewModel)
{
	ViewModel = InViewModel;

	SBorder::Construct(SBorder::FArguments()
		.ContentScale(FVector2D(1.f, 1.f))
		.DesiredSizeScale(FVector2D(1.f, 1.f))
		.BorderBackgroundColor(InArgs._ButtonColor)
		.ForegroundColor(FLinearColor::Black)
		.BorderImage(this, &SSMRichTextPropertyLink::GetBorderImage)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(TAttribute<FMargin>(this, &SSMRichTextPropertyLink::GetCombinedPadding))
		.ShowEffectWhenDisabled(TAttribute<bool>(this, &SSMRichTextPropertyLink::GetShowDisabledEffect))
		[
			SNew(STextBlock)
			.Text(InArgs._Text)
			.TextStyle(InArgs._TextStyle)
		]
	);

	SetButtonStyle(InArgs._ButtonStyle);
}