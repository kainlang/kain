// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerDataDetails.h"
#include "KClonerData.h"
#include "KClonerAnimBakeUtils.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FKClonerDataDetails"

void FKClonerDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 1)
	{
		SelectedData = Cast<UKClonerData>(Objects[0].Get());
	}

	// Only show button if we have a valid selection
	if (!SelectedData.IsValid()) return;

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AnimTweak");
	
	Category.AddCustomRow(LOCTEXT("BakeRow", "Bake"))
		.WholeRowContent()
		.HAlign(HAlign_Center)
		[
			SNew(SButton)
			.OnClicked(this, &FKClonerDataDetails::OnBakeAnimationClicked)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
			{
				// Only enable if Tweak Mode is on and we have an animation
				return SelectedData.IsValid() && SelectedData->bAnimTweakMode && SelectedData->SourceAnimSequence != nullptr;
			})))
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BakeButton", "Bake Animation from Preset"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

FReply FKClonerDataDetails::OnBakeAnimationClicked()
{
	if (SelectedData.IsValid())
	{
		UKClonerAnimBakeUtils::BakeAnimSequenceFromData(SelectedData.Get());
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
