// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSystemInfo.h"

FString FSMSystemInfo::GetBuildType() const
{
	return bIsMarketplace ? TEXT("Marketplace") : TEXT("Other");
}

FString FSMSystemInfo::ToString() const
{
	return FString::Printf(
		TEXT("Platform: %s\nEngine Version: %s\nPlugin Version: %s\nBuild Type: %s"),
		*OSVersion, *EngineVersion, *PluginVersion, *GetBuildType());
}
