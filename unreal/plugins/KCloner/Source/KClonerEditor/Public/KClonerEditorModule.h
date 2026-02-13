// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FKClonerEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterAssetTools();
	void UnregisterAssetTools();
	
	void RegisterLevelEditorExtensions();
	void UnregisterLevelEditorExtensions();
	
	TSharedRef<class FExtender> OnExtendLevelEditorMenu(const TSharedRef<class FUICommandList> CommandList, const TArray<AActor*> SelectedActors);

	TArray<TSharedPtr<class IAssetTypeActions>> RegisteredAssetTypeActions;
	FDelegateHandle LevelEditorMenuExtenderHandle;
};
