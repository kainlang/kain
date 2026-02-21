// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelDisconnectRangeAnalysisConstantsPass
{
	VOXEL_PASS_BODY(FVoxelDisconnectRangeAnalysisConstantsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};

struct FVoxelRemoveAllSeedNodesPass
{
	VOXEL_PASS_BODY(FVoxelRemoveAllSeedNodesPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};
