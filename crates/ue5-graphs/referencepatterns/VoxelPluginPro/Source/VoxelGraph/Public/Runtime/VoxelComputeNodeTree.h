// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"

struct FVoxelNodeType;
class FVoxelCppConstructor;
struct FVoxelGeneratorInit;
struct FVoxelContext;
struct FVoxelContextRange;
class FVoxelComputeNode;
class FVoxelGraphFunction;
class FVoxelDataComputeNode;
class FVoxelSeedComputeNode;
class FVoxelExecComputeNode;
class UVoxelNode;
struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;
struct FVoxelVariableAccessInfo;

/**
 * Tree of the compute nodes. Used to interpret the graph at runtime and to compile it
 */
class FVoxelComputeNodeTree
{
public:
	FVoxelComputeNodeTree() = default;

	// Runtime
	void Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;

	template<typename T>
	const FVoxelGraphFunction* Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, T& Recorder) const;
	template<typename T>
	const FVoxelGraphFunction* ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const;

	// Compilation
	void GetNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void InitCpp(FVoxelCppConstructor& Constructor) const;
	void ComputeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs);
	void ComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs);

	const TArray<FVoxelDataComputeNode*>& GetDataNodes() const { return DataNodes; }
	const TArray<FVoxelSeedComputeNode*>& GetSeedNodes() const { return SeedNodes; }
	const FVoxelExecComputeNode* GetExecNode() const { return ExecNode; }
	const TArray<FVoxelComputeNodeTree>& GetChildren() const { return Children; }

private:
	TArray<FVoxelDataComputeNode*> DataNodes;
	TArray<TVoxelSharedRef<FVoxelDataComputeNode>> DataNodesRefs;

	TArray<FVoxelSeedComputeNode*> SeedNodes;
	TArray<TVoxelSharedRef<FVoxelSeedComputeNode>> SeedNodesRefs;

	FVoxelExecComputeNode* ExecNode = nullptr;
	TVoxelSharedPtr<FVoxelExecComputeNode> ExecNodeRef;

	TArray<FVoxelComputeNodeTree> Children;

	friend class FVoxelCompilationNodeTree;
};
