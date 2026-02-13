// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FKClonerPreviewScene;

class SKClonerTimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKClonerTimeline) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FKClonerPreviewScene> InPreviewScene);

private:
	FReply OnPlayClicked();
	FReply OnPauseClicked();
	float GetCurrentTime() const;
	void OnScrubbed(float NewValue);

	TWeakPtr<FKClonerPreviewScene> PreviewScenePtr;
};
