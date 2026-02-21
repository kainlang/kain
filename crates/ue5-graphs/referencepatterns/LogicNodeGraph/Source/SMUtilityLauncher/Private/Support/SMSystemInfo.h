// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

struct FSMSystemInfo
{
	FString OSVersion;
	FString EngineVersion;
	FString PluginVersion;

	bool bIsMarketplace = false;
	bool bIsAuthenticated = false;

	FString GetBuildType() const;
	FString ToString() const;
};
