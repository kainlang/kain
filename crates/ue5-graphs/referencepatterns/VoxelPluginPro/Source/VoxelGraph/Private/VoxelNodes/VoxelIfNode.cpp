// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelIfNode.h"
#include "VoxelNodes/VoxelNodeColors.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"

UVoxelNode_If::UVoxelNode_If()
{
	SetInputs(EC::Exec, EC::Boolean);
	SetOutputs(
		{ "True", EC::Exec, "Branch used if condition is true" },
		{ "False", EC::Exec, "Branch used if condition is false" });
	SetColor(FVoxelNodeColors::ExecNode);
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_If::GetCompilationNode() const
{
	return MakeShared<FVoxelIfCompilationNode>(*this);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_If::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new FVoxelIfComputeNode(*this, InCompilationNode));
}

