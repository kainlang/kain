// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
struct FPluginDescriptor;

#define LD_PLUGIN_VERSION_CONSTRUCTION_SCRIPTS "2.5.0"

// Helpers for managing version updates.
class SMSYSTEMEDITOR_API FSMVersionUtils
{
public:
	struct FVersion
	{
		int32 Major = 0;
		int32 Minor = 0;
		int32 Patch = 0;
		/** Not always included. This isn't used in comparisons, only for informational purposes. */
		FString Commit;

		FVersion(const FString& InVersionName)
		{
			ParseVersion(InVersionName);
		}

		void ParseVersion(const FString& InVersionName)
		{
			TArray<FString> VersionArray;
			const int32 ArraySize = InVersionName.ParseIntoArray(VersionArray, TEXT("."));

			if (ArraySize > 0)
			{
				Major = FCString::Atoi(*VersionArray[0]);
			}
			if (ArraySize > 1)
			{
				Minor = FCString::Atoi(*VersionArray[1]);
			}
			if (ArraySize > 2)
			{
				Patch = FCString::Atoi(*VersionArray[2]);
			}
			if (ArraySize > 3)
			{
				Commit = VersionArray[3];
			}
		}
		
		bool operator==(const FVersion& RHS) const
		{
			return Major == RHS.Major && Minor == RHS.Minor && Patch == RHS.Patch;
		}
		
		bool operator<(const FVersion& RHS) const
		{
			if (Major < RHS.Major)
			{
				return true;
			}
			if (Major > RHS.Major)
			{
				return false;
			}
			if (Minor < RHS.Minor)
			{
				return true;
			}
			if (Minor > RHS.Minor)
			{
				return false;
			}
			return Patch < RHS.Patch;
		}

		bool operator>=(const FVersion& RHS) const
		{
			if (Major < RHS.Major)
			{
				return false;
			}
			if (Major > RHS.Major)
			{
				return true;
			}
			if (Minor < RHS.Minor)
			{
				return false;
			}
			if (Minor > RHS.Minor)
			{
				return true;
			}
			return Patch >= RHS.Patch;
		}

		/**
		 * Produce a friendly string of the full version.
		 * 
		 * @param bIncludeCommit Include the commit on the end if it is set.
		 * @return The fully formatted string "Major.Minor.Patch.Commit".
		 */
		FString ToString(const bool bIncludeCommit) const
		{
			TArray<FString> VersionStrArray = { FString::FromInt(Major), FString::FromInt(Minor), FString::FromInt(Patch) };
			if (bIncludeCommit && Commit.Len() > 0)
			{
				VersionStrArray.Add(Commit);
			}
			
			return FString::Join(VersionStrArray, TEXT("."));
		}
	};
	
	/**
	 * State machine blueprints are saved with this version number.
	 * On plugin load this version is checked against the asset version.
	 */
	static int32 GetCurrentBlueprintVersion();

	/**
	 * Node blueprints are saved with this version number.
	 * On plugin load this version is checked against the asset version.
	 */
	static int32 GetCurrentBlueprintNodeVersion();

	/**
	 * Return the version of the currently loaded plugin.
	 */
	static int32 GetCurrentPluginVersion();
	
	/** Check all SM blueprints and update to a new version if necessary. */
	static void UpdateBlueprintsToNewVersion();

	/**
	 * Handle project specific updates.
	 * @param PreviousVersionName The previously installed plugin version.
	 */
	static void UpdateProjectToNewVersion(const FString& PreviousVersionName);
	
	/** Checks if the state machine needs an update. */
	static bool IsStateMachineUpToDate(int32 CompareVersion);

	/** Checks if the state machine node needs an update. */
	static bool IsStateMachineNodeUpToDate(int32 CompareVersion);
	
	/** Checks if a Logic Driver blueprint is up to date. */
	static bool IsAssetUpToDate(UBlueprint* Blueprint);

	/** Checks if the state machine is from a new plugin version than installed. */
	static bool IsStateMachineFromNewerPluginVersion(int32 AssetVersion, int32 PluginVersion);

	/** Checks if the state machine node is from a new plugin version than installed. */
	static bool IsStateMachineNodeFromNewerPluginVersion(int32 AssetVersion, int32 PluginVersion);
	
	/** Sets the version tag of the asset. */
	static void SetToLatestVersion(UBlueprint* Blueprint);

private:
	static void DismissWrongVersionNotification();
};

