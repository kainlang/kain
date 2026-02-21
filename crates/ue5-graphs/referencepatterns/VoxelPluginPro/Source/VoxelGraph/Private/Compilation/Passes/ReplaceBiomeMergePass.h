// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelReplaceBiomeMergePass
{
	VOXEL_PASS_BODY(FVoxelReplaceBiomeMergePass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};

