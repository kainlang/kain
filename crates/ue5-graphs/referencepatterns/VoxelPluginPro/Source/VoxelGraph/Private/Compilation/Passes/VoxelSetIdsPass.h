// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

struct FVoxelSetPinsIdsPass
{
	VOXEL_PASS_BODY(FVoxelSetPinsIdsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, int32& Id);
};

struct FVoxelSetFunctionsIdsPass
{
	VOXEL_PASS_BODY(FVoxelSetFunctionsIdsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler);
};
