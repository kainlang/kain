// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"

class FKClonerPreviewScene;
class SKClonerViewport;

class FKClonerViewportClient : public FEditorViewportClient
{
public:
	FKClonerViewportClient(TWeakPtr<SEditorViewport> InViewport, TSharedRef<FKClonerPreviewScene> InPreviewScene);
	virtual ~FKClonerViewportClient();

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) override;
	// End of FEditorViewportClient interface
};
