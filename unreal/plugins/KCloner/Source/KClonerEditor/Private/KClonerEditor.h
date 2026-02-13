// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UKClonerData;

class FKClonerEditor : public FAssetEditorToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

	void InitKClonerEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UKClonerData* InClonerData);

	// FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	// End of FAssetEditorToolkit interface

private:
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Timeline(const FSpawnTabArgs& Args);
	
	/** Callback when properties change */
	void OnPropertiesChanged(const FPropertyChangedEvent& Event);

	/** Bake Action */
	void OnBakeAnim();
	void ExtendToolbar();

	UKClonerData* ClonerData;
	TSharedPtr<class FKClonerPreviewScene> PreviewScene;
	TSharedPtr<class SKClonerViewport> ViewportWidget;
	TSharedPtr<class SKClonerTimeline> TimelineWidget;
};
