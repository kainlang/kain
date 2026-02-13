// Copyright 2026 K-Studio. All Rights Reserved.

#include "SKClonerViewport.h"
#include "KClonerEditor.h"
#include "KClonerPreviewScene.h"
#include "KClonerViewportClient.h"

void SKClonerViewport::Construct(const FArguments& InArgs, TSharedPtr<FKClonerEditor> InEditor, TSharedPtr<FKClonerPreviewScene> InPreviewScene)
{
	EditorPtr = InEditor;
	PreviewScenePtr = InPreviewScene;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SKClonerViewport::~SKClonerViewport()
{
	if (Client)
	{
		Client->Viewport = nullptr;
		Client = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SKClonerViewport::MakeEditorViewportClient()
{
	TSharedPtr<FKClonerPreviewScene> PinnedScene = PreviewScenePtr.Pin();
	check(PinnedScene.IsValid());

	// Pass nullptr for the viewport widget initially. SEditorViewport::Construct will assign it later.
	TSharedRef<FKClonerViewportClient> NewClient = MakeShareable(new FKClonerViewportClient(nullptr, PinnedScene.ToSharedRef()));
	
	// Enable Realtime to ensure animation plays without camera movement
	NewClient->SetRealtime(true);
	
	return NewClient;
}
