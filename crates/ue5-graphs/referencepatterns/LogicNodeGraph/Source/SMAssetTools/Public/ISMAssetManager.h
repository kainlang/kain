// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#include "AssetRegistry/ARFilter.h"

class ISMAssetManager
{
public:
	virtual ~ISMAssetManager() {}

	/** Arguments for creating a new state machine blueprint asset. */
	struct FCreateStateMachineBlueprintArgs
	{
		/** [Required] Name to use for the asset. It will automatically be adjusted for collisions. */
		FName Name;

		/** [Optional] Parent class of the blueprint. When not set the default SMInstance is used. */
		TSubclassOf<USMInstance> ParentClass;

		/** [Optional] Relative path of the new asset. When empty the game directory is used. */
		FString Path;
	};

	/**
	 * Create a new state machine blueprint asset.
	 *
	 * @param InArgs Arguments to configure the new state machine blueprint.
	 *
	 * @return The newly created blueprint if successful.
	 */
	virtual USMBlueprint* CreateStateMachineBlueprint(const FCreateStateMachineBlueprintArgs& InArgs) = 0;

	/**
	 * Update the blueprint's CDO with new values.
	 *
	 * @param InBlueprint The blueprint to update. Accepts any type of blueprint.
	 * @param InNewClassDefaults An object instance to copy properties from. This class should match the CDO class.
	 */
	virtual void PopulateClassDefaults(UBlueprint* InBlueprint, UObject* InNewClassDefaults) = 0;

	/**
	 * Arguments for compiling blueprints. By default they are limited to state machines, but could be configured
	 * for any kind of blueprint.
	 */
	struct FCompileBlueprintArgs
	{
		/** [Required] The filter used to locate the assets to compile. */
		FARFilter AssetFilter;

		/** [Optional] Save the blueprints afterward. */
		bool bSave = false;

		/** [Optional] Display a warning message before starting the process. */
		bool bShowWarningMessage = false;

		/** [Optional] Custom warning title to display if any. */
		FText CustomWarningTitle;

		/** [Optional] Custom warning message to display if any. */
		FText CustomWarningMessage;

		FCompileBlueprintArgs()
		{
			AssetFilter.bRecursiveClasses = true;
			AssetFilter.ClassPaths.Add(USMBlueprint::StaticClass()->GetClassPathName());
		}
	};

	DECLARE_DELEGATE(FOnCompileBlueprintsCompletedSignature);

	/**
	 * Compile all blueprints. This will load all required assets asynchronously and then compile a blueprint each tick.
	 *
	 * @param InArgs Configure the blueprints to compile.
	 * @param InOnCompileBlueprintsCompletedDelegate Delegate to call when the process has completed.
	 */
	virtual void CompileBlueprints(const FCompileBlueprintArgs& InArgs,
		const FOnCompileBlueprintsCompletedSignature& InOnCompileBlueprintsCompletedDelegate = FOnCompileBlueprintsCompletedSignature()) = 0;

	/**
	 * Cancel CompileBlueprints().
	 */
	virtual void CancelCompileBlueprints() = 0;

	/**
	 * Are blueprints currently loading or compiling from CompileBlueprints()?
	 */
	virtual bool IsCompilingBlueprints() const = 0;

	/**
	 * Return the current compile percentage of blueprints compiling. The first 0.5 is load percent, the next 0.5
	 * is compile percent.
	 */
	virtual float GetCompileBlueprintsPercent() const = 0;
};