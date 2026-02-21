// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelCompilationPass.h"

class FVoxelComputeNode;
class FVoxelDataComputeNode;
class FVoxelSeedComputeNode;
class FVoxelCreatedComputeNodes;

struct FVoxelMarkDependenciesPass
{
	VOXEL_PASS_BODY(FVoxelMarkDependenciesPass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};

struct FVoxelDebugDependenciesPass
{
	VOXEL_PASS_BODY(FVoxelDebugDependenciesPass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};

struct FVoxelGetSortedConstantsAndRemoveConstantsPass
{
	VOXEL_PASS_BODY(FVoxelGetSortedConstantsAndRemoveConstantsPass);

	static void Apply(
		FVoxelGraphCompiler& Compiler, 
		FVoxelCreatedComputeNodes& CreatedNodes,
		TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& ConstantNodes, 
		TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& SeedNodes);
};
