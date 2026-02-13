// Copyright 2026 K-Studio. All Rights Reserved.

#include "SKClonerTimeline.h"
#include "KClonerPreviewScene.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void SKClonerTimeline::Construct(const FArguments& InArgs, TSharedPtr<FKClonerPreviewScene> InPreviewScene)
{
	PreviewScenePtr = InPreviewScene;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SHorizontalBox)
			// PLAY / PAUSE buttons
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString("Play"))
				.OnClicked(this, &SKClonerTimeline::OnPlayClicked)
			]
			// Pause Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString("Pause"))
				.OnClicked(this, &SKClonerTimeline::OnPauseClicked)
			]
			// current time in seconds
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					float Time = GetCurrentTime();
					return FText::Format(FText::FromString("Time: {0}"), FText::AsNumber(Time));
				})
			]
		]
		// Scrubber
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SSlider)
			.Value_Lambda([this]()
			{
				// slider is 0-1, so map it to 0-10s for the preview
				return GetCurrentTime() / 10.0f;
			})
			.OnValueChanged(this, &SKClonerTimeline::OnScrubbed)
		]
	];
}

FReply SKClonerTimeline::OnPlayClicked()
{
	if (TSharedPtr<FKClonerPreviewScene> PinnedScene = PreviewScenePtr.Pin())
	{
		PinnedScene->SetPlaying(true);
	}
	return FReply::Handled();
}

FReply SKClonerTimeline::OnPauseClicked()
{
	if (TSharedPtr<FKClonerPreviewScene> PinnedScene = PreviewScenePtr.Pin())
	{
		PinnedScene->SetPlaying(false);
	}
	return FReply::Handled();
}

float SKClonerTimeline::GetCurrentTime() const
{
	if (TSharedPtr<FKClonerPreviewScene> PinnedScene = PreviewScenePtr.Pin())
	{
		return PinnedScene->GetCurrentTime();
	}
	return 0.0f;
}

void SKClonerTimeline::OnScrubbed(float NewValue)
{
	if (TSharedPtr<FKClonerPreviewScene> PinnedScene = PreviewScenePtr.Pin())
	{
		PinnedScene->SetPlaying(false); // Pause when scrubbing
		PinnedScene->SetCurrentTime(NewValue * 10.0f);
	}
}
