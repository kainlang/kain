// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVoxelDataAssetData;

namespace MagicaVox
{
	bool ImportToAsset(const FString& Filename, FVoxelDataAssetData& Asset, bool bUsePalette);
}
