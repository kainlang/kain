// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelReplaceFlowMergePass
{
	VOXEL_PASS_BODY(FVoxelReplaceFlowMergePass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};

// Make sure ifs nodes have multiple childs
struct FVoxelFixMultipleOutputsExecPass
{
	VOXEL_PASS_BODY(FVoxelFixMultipleOutputsExecPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};
