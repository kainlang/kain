// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"
#include "VoxelMinimal.h"

class FVoxelComputeNode;
class FVoxelComputeNodeTree;
class FVoxelCppConstructor;

struct FVoxelGeneratorInit;

struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;

struct FVoxelContext;
struct FVoxelContextRange;

enum class EVoxelFunctionType : uint8
{
	Init,
	Compute
};

struct FVoxelGraphFunctionInfo
{
	const int32 FunctionId;
	const EVoxelFunctionType FunctionType;
	const EVoxelFunctionAxisDependencies Dependencies;

	FVoxelGraphFunctionInfo(int32 FunctionId, EVoxelFunctionType FunctionType, EVoxelFunctionAxisDependencies Dependencies)
		: FunctionId(FunctionId)
		, FunctionType(FunctionType)
		, Dependencies(Dependencies)
	{
	}

	FString GetFunctionName() const;
};

class FVoxelGraphFunction
{
public:
	const int32 FunctionId;
	const EVoxelFunctionAxisDependencies Dependencies;

	FVoxelGraphFunction(
		const TVoxelSharedRef<FVoxelComputeNodeTree>& Tree, 
		const TVoxelSharedRef<FVoxelComputeNode>& FunctionInit,
		int32 FunctionId,
		EVoxelFunctionAxisDependencies Dependencies);

	void Call(FVoxelCppConstructor& Constructor, const TArray<FString>& Args, EVoxelFunctionType FunctionType) const;

	bool IsUsedForInit() const;
	bool IsUsedForCompute(FVoxelCppConstructor& Constructor) const;

	inline const FVoxelComputeNodeTree& GetTree() const
	{
		return *Tree;
	}

public:
	void Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;
	
	template<typename T>
	void Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, T& Recorder) const;
	template<typename T>
	void ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const;

public:
	void GetNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void DeclareInitFunction(FVoxelCppConstructor& Constructor) const;
	void DeclareComputeFunction(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const;

private:
	const TVoxelSharedRef<FVoxelComputeNodeTree> Tree;
	const TVoxelSharedRef<FVoxelComputeNode> FunctionInit;

	void DeclareFunction(FVoxelCppConstructor& Constructor, EVoxelFunctionType Type) const;	
};
