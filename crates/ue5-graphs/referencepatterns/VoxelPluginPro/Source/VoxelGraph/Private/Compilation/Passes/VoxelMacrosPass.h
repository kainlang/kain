// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelGraphInlineMacrosPass
{
	VOXEL_PASS_BODY(FVoxelGraphInlineMacrosPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool bClearMessages);
};

struct FVoxelGraphReplaceMacroInputOutputsPass
{
	VOXEL_PASS_BODY(FVoxelGraphReplaceMacroInputOutputsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};

