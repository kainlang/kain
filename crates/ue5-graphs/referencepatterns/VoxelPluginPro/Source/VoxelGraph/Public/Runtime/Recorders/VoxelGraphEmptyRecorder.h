// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelNodeType.h"

struct FVoxelContext;
struct FVoxelContextRange;

class FVoxelComputeNode;
class FVoxelDataComputeNode;
class FVoxelExecComputeNode;

class FVoxelGraphEmptyRecorder
{
public:
	struct FScope {};
	
	FORCEINLINE FScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context)
	{
		return {};
	}
	FORCEINLINE FScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputBuffer Inputs, const FVoxelContext& Context)
	{
		return {};
	}
};

class FVoxelGraphEmptyRangeRecorder
{
public:
	struct FScope {};
	
	FORCEINLINE FScope MakeDataScope(FVoxelDataComputeNode* Node, FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context)
	{
		return {};
	}
	FORCEINLINE FScope MakeIfScope(FVoxelExecComputeNode* ExecNode)
	{
		return {};
	}
	FORCEINLINE FScope MakeSetterScope(FVoxelExecComputeNode* ExecNode, FVoxelNodeInputRangeBuffer Inputs, const FVoxelContextRange& Context)
	{
		return {};
	}
};
