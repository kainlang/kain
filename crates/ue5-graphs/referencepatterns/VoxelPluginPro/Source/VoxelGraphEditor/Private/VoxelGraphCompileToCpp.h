// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVoxelGraphGenerator;
class SWidget;

namespace FVoxelGraphCompileToCpp
{
	void Compile(UVoxelGraphGenerator* Generator, bool bIsAutomaticCompile);
}
