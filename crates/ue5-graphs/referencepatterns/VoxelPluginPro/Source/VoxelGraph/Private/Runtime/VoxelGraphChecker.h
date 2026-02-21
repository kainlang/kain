// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FVoxelGraphErrorReporter;
class FVoxelGraph;

namespace FVoxelGraphChecker
{
	void CheckGraph(FVoxelGraphErrorReporter& ErrorReporter, const FVoxelGraph& Graph);
}
