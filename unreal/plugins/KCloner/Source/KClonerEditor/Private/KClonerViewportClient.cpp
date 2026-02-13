// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerViewportClient.h"
#include "KClonerPreviewScene.h"
#include "AssetEditorModeManager.h"

FKClonerViewportClient::FKClonerViewportClient(TWeakPtr<SEditorViewport> InViewport, TSharedRef<FKClonerPreviewScene> InPreviewScene)
	: FEditorViewportClient(nullptr, &InPreviewScene.Get(), InViewport)
{
	// SetRealtime(true); // Moved to Tick or init
}

FKClonerViewportClient::~FKClonerViewportClient()
{
}

void FKClonerViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
}
