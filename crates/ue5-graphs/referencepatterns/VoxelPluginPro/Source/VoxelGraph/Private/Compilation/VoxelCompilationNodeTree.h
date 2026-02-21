// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"

class FVoxelCompilationNode;
class FVoxelComputeNode;
class FVoxelFunctionCallComputeNode;
class FVoxelComputeNodeTree;
class FVoxelCreatedComputeNodes;

class FVoxelCompilationNodeTree
{
public:
	FVoxelCompilationNodeTree() = default;
	static TSharedRef<FVoxelCompilationNodeTree> Create(FVoxelCompilationNode* FirstNode);
	
	void ConvertToComputeNodeTree(
		EVoxelFunctionAxisDependencies FunctionDependencies,
		FVoxelComputeNodeTree& OutTree,
		TArray<FVoxelFunctionCallComputeNode*>& OutFunctionCallsToLink,
		TSet<FVoxelCompilationNode*>& OutUsedNodes,
		FVoxelCreatedComputeNodes& CreatedNodes) const;

private:
	TArray<FVoxelCompilationNodeTree> Children;

	TSet<FVoxelCompilationNode*> DataNodesSet;
	TArray<FVoxelCompilationNode*> SortedDataNodes;
	FVoxelCompilationNode* ExecNode = nullptr;

	FVoxelCompilationNodeTree(FVoxelCompilationNode* ExecNode);
	void RemoveAlreadyComputedNodes(const TSet<FVoxelCompilationNode*>& AlreadyComputedNodes = {});
	void BakeSortedDataNodes();

	friend class TArray<FVoxelCompilationNodeTree>;
};
