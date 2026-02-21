// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVoxelDataAssetData;

namespace RawVox
{
	bool ImportToAsset(const FString& File, FVoxelDataAssetData& Asset);
}

