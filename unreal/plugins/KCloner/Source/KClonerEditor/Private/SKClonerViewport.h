// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FKClonerPreviewScene;
class FKClonerEditor;

class SKClonerViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SKClonerViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FKClonerEditor> InEditor, TSharedPtr<FKClonerPreviewScene> InPreviewScene);
	
	virtual ~SKClonerViewport();

protected:
	// SEditorViewport interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	// End of SEditorViewport interface

private:
	TWeakPtr<FKClonerEditor> EditorPtr;
	TWeakPtr<FKClonerPreviewScene> PreviewScenePtr;
};
