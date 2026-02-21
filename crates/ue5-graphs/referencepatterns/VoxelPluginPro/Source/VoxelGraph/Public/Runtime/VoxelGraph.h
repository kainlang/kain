// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelContainers/VoxelStaticArray.h"
#include "VoxelAxisDependencies.h"

class FVoxelGraphFunction;
struct FVoxelNodeType;
struct FVoxelGeneratorInit;
struct FVoxelContext;
struct FVoxelContextRange;
class FVoxelCppConstructor;
struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;
class FVoxelDataComputeNode;
class FVoxelSeedComputeNode;
class FVoxelComputeNode;
class FVoxelVariable;

struct FVoxelGraphFunctions
{
	int32 FunctionId = -1;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionX;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYWithCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYWithoutCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYZWithCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYZWithoutCache;

	FVoxelGraphFunctions() = default;
	FVoxelGraphFunctions(
		int32 FunctionId,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionX,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithoutCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithoutCache);

	const FVoxelGraphFunction& Get(EVoxelFunctionAxisDependencies Dependencies) const
	{
		switch (Dependencies)
		{
		case EVoxelFunctionAxisDependencies::X:
			return *FunctionX.Get();
		case EVoxelFunctionAxisDependencies::XYWithCache:
			return *FunctionXYWithCache.Get();
		case EVoxelFunctionAxisDependencies::XYWithoutCache:
			return *FunctionXYWithoutCache.Get();
		case EVoxelFunctionAxisDependencies::XYZWithCache:
			return *FunctionXYZWithCache.Get();
		case EVoxelFunctionAxisDependencies::XYZWithoutCache:
			return *FunctionXYZWithoutCache.Get();
		default:
			check(false);
			return *FunctionX.Get();
		}
	}
	bool IsValid() const
	{
		return FunctionX.IsValid();
	}
	TVoxelStaticArray<FVoxelGraphFunction*, 5> Iterate() const
	{
		return
		{
			FunctionX.Get(),
			FunctionXYWithCache.Get(),
			FunctionXYWithoutCache.Get(),
			FunctionXYZWithCache.Get(),
			FunctionXYZWithoutCache.Get(),
		};
	}
};

class VOXELGRAPH_API FVoxelGraph : public TVoxelSharedFromThis<FVoxelGraph>
{
public:
	FVoxelGraph(
		const FString& Name,
		const TArray<FVoxelGraphFunctions>& Functions,
		const FVoxelGraphFunctions& FirstFunctions,
		const TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& ConstantComputeNodes,
		const TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& SeedComputeNodes,
		int32 VariablesBufferSize);

	const FString Name;
	const TArray<FVoxelGraphFunctions> AllFunctions;
	const FVoxelGraphFunctions FirstFunctions;
	const TArray<TVoxelSharedRef<FVoxelDataComputeNode>> ConstantComputeNodes;
	const TArray<TVoxelSharedRef<FVoxelSeedComputeNode>> SeedComputeNodes;
	const int32 VariablesBufferSize = 0;

public:
	void Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;
	
	void ComputeConstants(FVoxelGraphVMComputeBuffers& Buffers) const;
	template<typename T>
	void Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, EVoxelFunctionAxisDependencies Dependencies, T& Recorder) const;

	void ComputeRangeConstants(FVoxelGraphVMComputeRangeBuffers& Buffers) const;
	template<typename T>
	void ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const;
	
public:
	void GetSeedNodes(TSet<FVoxelComputeNode*>& Nodes) const;
	void GetConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const;
	void GetNotConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const;
	void GetAllNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void Init(FVoxelCppConstructor& Constructor) const;
	void ComputeConstants(FVoxelCppConstructor& Constructor) const;
	void Compute(FVoxelCppConstructor& Constructor, EVoxelFunctionAxisDependencies Dependencies) const;

	void DeclareInitFunctions(FVoxelCppConstructor& Constructor) const;
	void DeclareComputeFunctions(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const;
};
