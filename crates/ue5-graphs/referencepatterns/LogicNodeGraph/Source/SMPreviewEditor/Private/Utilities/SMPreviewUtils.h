// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor.h"

class USMPreviewObject;
class USMInstance;
class USMBlueprint;
class FSMBlueprintEditor;
class FSMPreviewModeViewportClient;
class FSMAdvancedPreviewScene;
class AActor;
class FTabManager;
class SWindow;

// Helpers for managing previews.
class SMPREVIEWEDITOR_API FSMPreviewUtils
{
protected:
	static const FString PreviewPackagePrefix;
	static const FString PreviewPackageSimulationPrefix;

public:
	FORCEINLINE static FString GetPreviewPackagePrefix() { return PreviewPackagePrefix; }

	FORCEINLINE static FString GetPreviewSimulationPrefix() { return PreviewPackageSimulationPrefix; }
	
	/** Starts a preview state machine. */
	static USMInstance* StartSimulation(USMBlueprint* Blueprint);

	/** Stops everything related to a preview. */
	static void StopSimulation(USMBlueprint* Blueprint);

	/** Stops all running simulations. */
	static void StopAllSimulations();
	
	/** Tries to find a valid actor context if one is set or base context object. */
	static UObject* GetContextForPreview(USMBlueprint* Blueprint);

	/** Return the preview object from a blueprint editor. */
	static USMPreviewObject* GetPreviewObject(TWeakPtr<FSMBlueprintEditor> BlueprintEditor);
	
	/** Clones a world and prepares it for simulation. */
	static UWorld* DuplicateWorldForSimulation(const FString& PackageName, UWorld* OwningWorld);

	/** Retrieve the viewport client if one is set. */
	static TWeakPtr<FSMPreviewModeViewportClient> GetViewportClient(USMBlueprint* Blueprint);

	/** Checks the world for a given actor. Can also check by name. */
	static bool DoesWorldContainActor(UWorld* WorldToCheck, const AActor* CompareActor, bool bCheckName = false);

	/** Create a qualified name for an object's property. */
	static FString MakeFullObjectPropertyName(UObject* InObject, FProperty* InProperty);

	/**
	 * Notify the engine that no level viewport is selected. Our preview client acts as a level editor
	 * but isn't actually considered one. When we select actors containing a camera the real level tries to
	 * render them. We don't want this and when shutting down the simulation will cause a crash.
	 * The engine will automatically set this again when clicking in a real level viewport.
	 */
	FORCEINLINE static void DeselectEngineLevelEditor()
	{
		GCurrentLevelEditingViewportClient = nullptr;
	}

	static void BindDelegates();
	static void UnbindDelegates();
	
private:
	/** Prepares the appropriate world for preview. */
	static UWorld* PreparePreviewWorld(USMBlueprint* Blueprint);

	static void OnPackageDirtyFlagChanged(UPackage* Package, bool bWasDirty);
	static void OnMapChanged(const FString& MapName, bool bAsTemplate);
};

/**
 * Utilities to ensure both the main unreal world outliner is showing the correct level world,
 * and that any reference selectors both in the main world and our worlds are set to the correct world.
 */
class FSMPreviewOutlinerUtils
{
public:
	/**
	 * Perform a full refresh on the main level editor outliner.
	 * Needed because our world context requires a type of `Editor` and it might show up in the level editor.
	 *
	 * @return true if the world outliner is performing a refresh.
	 */
	static bool RefreshLevelEditorOutliner(FSMAdvancedPreviewScene* PreviewOwner);

	/** Checks if a tab manager belongs to us. */
	static bool DoesTabBelongToPreview(TSharedPtr<FTabManager> InTabManager, USMBlueprint* SMBlueprint);
};