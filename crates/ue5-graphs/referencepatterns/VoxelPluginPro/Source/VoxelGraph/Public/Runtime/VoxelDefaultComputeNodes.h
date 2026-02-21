// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelGraph.h"

using FVoxelFunctionInitComputeNode = TVoxelExecComputeNodeHelper<EVoxelComputeNodeExecType::FunctionInit>;

class FVoxelFunctionCallComputeNode final : public FVoxelExecComputeNode
{
public:
	const int32 FunctionId;
	const EVoxelFunctionAxisDependencies FunctionDependencies;

	FVoxelFunctionCallComputeNode(int32 FunctionId, EVoxelFunctionAxisDependencies FunctionDependencies, const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelExecComputeNode(EVoxelComputeNodeExecType::FunctionCall, Node, CompilationNode)
		, FunctionId(FunctionId)
		, FunctionDependencies(FunctionDependencies)
	{
		check(FunctionId >= 0);
	}

	inline void SetFunctions(const FVoxelGraphFunctions& InFunctions)
	{
		check(!Functions.IsValid() && InFunctions.IsValid());
		Functions = InFunctions;
		Function = &Functions.Get(FunctionDependencies);
	}
	inline const FVoxelGraphFunction* GetFunction() const { return Function; }

private:
	FVoxelGraphFunctions Functions;
	const FVoxelGraphFunction* Function = nullptr;
};
