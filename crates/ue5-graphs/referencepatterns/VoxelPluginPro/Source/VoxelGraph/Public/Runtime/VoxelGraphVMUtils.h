// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelGraphGlobals.h"
#include "VoxelMaterialBuilder.h"
#include "Runtime/VoxelNodeType.h"

struct FVoxelGraphVMInitBuffers
{
	FVoxelGraphSeed* RESTRICT const Variables;

	explicit FVoxelGraphVMInitBuffers(FVoxelGraphSeed* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};

struct FVoxelGraphVMOutputBuffers
{
	FVoxelNodeType Buffer[MAX_VOXELGRAPH_OUTPUTS];
	FVoxelMaterialBuilder MaterialBuilder;
};

struct FVoxelGraphVMComputeBuffers
{
	FVoxelNodeType* RESTRICT const Variables;
	FVoxelNodeType FunctionInputsOutputs[MAX_VOXELFUNCTION_ARGS];
	FVoxelGraphVMOutputBuffers GraphOutputs;

	explicit FVoxelGraphVMComputeBuffers(FVoxelNodeType* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};

struct FVoxelGraphVMRangeOutputBuffers
{
	FVoxelNodeRangeType Buffer[MAX_VOXELGRAPH_OUTPUTS];
};

struct FVoxelGraphVMComputeRangeBuffers
{
	FVoxelNodeRangeType* RESTRICT const Variables;
	FVoxelNodeRangeType FunctionInputsOutputs[MAX_VOXELFUNCTION_ARGS];
	FVoxelGraphVMRangeOutputBuffers GraphOutputs;

	explicit FVoxelGraphVMComputeRangeBuffers(FVoxelNodeRangeType* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};
