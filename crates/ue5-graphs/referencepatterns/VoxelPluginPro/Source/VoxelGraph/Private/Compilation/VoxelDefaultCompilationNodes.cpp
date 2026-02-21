// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Runtime/VoxelDefaultComputeNodes.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"

FVoxelFunctionInitCompilationNode::FVoxelFunctionInitCompilationNode(const FVoxelFunctionSeparatorCompilationNode& Separator)
	: TVoxelCompilationNode(Separator.Node, TArray<EVoxelPinCategory>(), Separator.GetOutputPinCategories())
{
	Dependencies = Separator.Dependencies;
	SourceNodes = Separator.SourceNodes;
	FunctionId = Separator.FunctionId;

	for (int32 Index = 0; Index < GetOutputCountWithoutExecs() ; Index++)
	{
		SetOutputId(Index, Separator.GetOutputId(Index));
	}
}

TVoxelSharedPtr<FVoxelComputeNode> FVoxelFunctionInitCompilationNode::GetComputeNode() const
{
	return MakeVoxelShared<FVoxelFunctionInitComputeNode>(Node, *this);
}

FVoxelFunctionCallCompilationNode::FVoxelFunctionCallCompilationNode(const FVoxelFunctionSeparatorCompilationNode& Separator)
	: TVoxelCompilationNode(Separator.Node, Separator.GetInputPinCategories(), TArray<EVoxelPinCategory>())
{
	Dependencies = Separator.Dependencies;
	SourceNodes = Separator.SourceNodes;
	FunctionId = Separator.FunctionId;

	for (int32 Index = 0; Index < GetInputCountWithoutExecs(); Index++)
	{
		SetInputId(Index, Separator.GetInputId(Index));
	}
}

inline EVoxelFunctionAxisDependencies GetCalledFunctionDependencies(EVoxelAxisDependencies NodeDependencies, EVoxelFunctionAxisDependencies FunctionDependencies)
{
	switch (NodeDependencies)
	{
	case EVoxelAxisDependencies::Constant:
	case EVoxelAxisDependencies::X:
		return FunctionDependencies;
	case EVoxelAxisDependencies::XY:
		switch (FunctionDependencies)
		{
		case EVoxelFunctionAxisDependencies::XYWithCache:
		case EVoxelFunctionAxisDependencies::XYWithoutCache:
			return EVoxelFunctionAxisDependencies::XYWithoutCache;
		case EVoxelFunctionAxisDependencies::XYZWithCache:
			return EVoxelFunctionAxisDependencies::XYZWithCache;
		case EVoxelFunctionAxisDependencies::XYZWithoutCache:
			return EVoxelFunctionAxisDependencies::XYZWithoutCache;
		// Can't have X if we are XY, must fail AreNodeDependenciesInferiorOrEqual
		case EVoxelFunctionAxisDependencies::X:
		default:
			check(false);
			return EVoxelFunctionAxisDependencies::X;
		}
	case EVoxelAxisDependencies::XYZ:
		switch (FunctionDependencies)
		{
		case EVoxelFunctionAxisDependencies::XYZWithCache:
		case EVoxelFunctionAxisDependencies::XYZWithoutCache:
			return EVoxelFunctionAxisDependencies::XYZWithoutCache;
		// Can't have X or XY if we are XYZ, must fail AreNodeDependenciesInferiorOrEqual
		case EVoxelFunctionAxisDependencies::X:
		case EVoxelFunctionAxisDependencies::XYWithCache:
		case EVoxelFunctionAxisDependencies::XYWithoutCache:
		default:
			check(false);
			return EVoxelFunctionAxisDependencies::X;
		}
	default:
		check(false);
		return EVoxelFunctionAxisDependencies::X;
	}
}

TVoxelSharedPtr<FVoxelComputeNode> FVoxelFunctionCallCompilationNode::GetComputeNode(EVoxelFunctionAxisDependencies FunctionDependencies) const
{
	EVoxelFunctionAxisDependencies CalledFunctionDependencies = GetCalledFunctionDependencies(FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(Dependencies), FunctionDependencies);
	return MakeVoxelShared<FVoxelFunctionCallComputeNode>(FunctionId, CalledFunctionDependencies, Node, *this);
}
