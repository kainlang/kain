// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Blueprints/UI/SMNewAssetDialogueOption.h"

class FSMNewAssetOptions
{
public:
	static void Initialize();
	static void Shutdown();

	static void OnGetNewAssetDialogOptions(TArray<FSMNewAssetDialogOption>& OutOptions);
};
