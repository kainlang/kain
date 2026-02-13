// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

/**
 * AlphaGen Editor Module
 * 
 * Provides toolbar integration and editor widget for procedural alpha texture generation.
 * Ported from K-OS DCC alpha generation system.
 */
class FAlphaGenModule : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Get the module instance */
	static FAlphaGenModule& Get();
	
	/** Check if module is loaded */
	static bool IsAvailable();

private:
	/** Register the AlphaGen toolbar extension */
	void RegisterToolbarExtension();
	
	/** Unregister toolbar extension */
	void UnregisterToolbarExtension();
	
	/** Register Slate style set for icons */
	void RegisterStyleSet();
	
	/** Unregister Slate style set */
	void UnregisterStyleSet();
	
	/** Register the nomad tab spawner for the main widget */
	void RegisterTabSpawner();
	
	/** Unregister tab spawner */
	void UnregisterTabSpawner();
	
	/** Callback when toolbar button is clicked */
	void OnToolbarButtonClicked();
	
	/** Deferred initialization after engine is ready */
	void OnPostEngineInit();
	
	/** Spawn the AlphaGen widget tab */
	TSharedRef<class SDockTab> SpawnAlphaGenTab(const class FSpawnTabArgs& SpawnTabArgs);
	
	/** Handle to the registered toolbar extension */
	TSharedPtr<FExtender> ToolbarExtender;
	
	/** Tab identifier */
	static const FName AlphaGenTabName;
};
