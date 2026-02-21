// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGraphOutputs.h"
#include "VoxelCompilationPass.h"

struct FVoxelOptimizeForPermutationPass
{
	VOXEL_PASS_BODY(FVoxelOptimizeForPermutationPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, const FVoxelGraphPermutationArray& Permutation);
};

struct FVoxelReplaceCompileTimeConstantsPass
{
	VOXEL_PASS_BODY(FVoxelReplaceCompileTimeConstantsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, const TMap<FName, FString>& Constants);
};

// Remove all exec nodes with no setter or function call after it
struct FVoxelRemoveUnusedExecsPass
{
	VOXEL_PASS_BODY(FVoxelRemoveUnusedExecsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveUnusedNodesPass
{
	VOXEL_PASS_BODY(FVoxelRemoveUnusedNodesPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelDisconnectUnusedFlowMergePinsPass
{
	VOXEL_PASS_BODY(FVoxelDisconnectUnusedFlowMergePinsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveFlowMergeWithNoPinsPass
{
	VOXEL_PASS_BODY(FVoxelRemoveFlowMergeWithNoPinsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveFlowMergeWithSingleExecPinLinkedPass
{
	VOXEL_PASS_BODY(FVoxelRemoveFlowMergeWithSingleExecPinLinkedPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveConstantIfsPass
{
	VOXEL_PASS_BODY(FVoxelRemoveConstantIfsPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveConstantSwitchesPass
{
	VOXEL_PASS_BODY(FVoxelRemoveConstantSwitchesPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelRemoveIfsWithSameTargetPass
{
	VOXEL_PASS_BODY(FVoxelRemoveIfsWithSameTargetPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};

struct FVoxelReplaceConstantPureNodesPass
{
	VOXEL_PASS_BODY(FVoxelReplaceConstantPureNodesPass);
	
	static void Apply(FVoxelGraphCompiler& Compiler, bool& bChanged);
};
