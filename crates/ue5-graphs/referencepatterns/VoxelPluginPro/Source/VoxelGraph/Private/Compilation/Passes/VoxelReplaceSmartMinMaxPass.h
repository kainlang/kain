// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelReplaceSmartMinMaxPass
{
	VOXEL_PASS_BODY(FVoxelReplaceSmartMinMaxPass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};

