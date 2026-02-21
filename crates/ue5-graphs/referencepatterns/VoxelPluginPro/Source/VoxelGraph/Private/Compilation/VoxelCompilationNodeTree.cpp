// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelCompilationNodeTree.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelDefaultComputeNodes.h"

TSharedRef<FVoxelCompilationNodeTree> FVoxelCompilationNodeTree::Create(FVoxelCompilationNode* FirstNode)
{
	VOXEL_FUNCTION_COUNTER();
	
	TSharedRef<FVoxelCompilationNodeTree> Result = MakeShareable(new FVoxelCompilationNodeTree(FirstNode));
	Result->RemoveAlreadyComputedNodes();
	Result->BakeSortedDataNodes();
	return Result;
}

FVoxelCompilationNodeTree::FVoxelCompilationNodeTree(FVoxelCompilationNode* ExecNode)
	: ExecNode(ExecNode)
{
	check(ExecNode);
	check(ExecNode->IsExecNode());

	FVoxelGraphCompilerHelpers::AddPreviousNodesToSet(ExecNode, DataNodesSet);
	DataNodesSet.Remove(ExecNode);

	if (!ExecNode->IsA<FVoxelFunctionCallCompilationNode>())
	{
		for (auto& Pin : ExecNode->IteratePins<EVoxelPinIter::Output>())
		{
			if (Pin.PinCategory == EVoxelPinCategory::Exec)
			{
				if (Pin.NumLinkedTo() > 0)
				{
					check(Pin.NumLinkedTo() == 1);
					Children.Emplace(&Pin.GetLinkedTo(0).Node);
				}
				else
				{
					// Else the children choice will be wrong
					check(ExecNode->GetExecOutputCount() == 1);
				}
			}
		}
		// Find the nodes computed by all branches, except if we're an Init as they might use our not computed yet data outputs
		if (!ExecNode->IsA<FVoxelFunctionInitCompilationNode>())
		{
			for (auto* DataNode : FVoxelGraphCompilerHelpers::GetAlwaysComputedNodes(ExecNode))
			{
				check(!DataNode->IsExecNode());
				DataNodesSet.Add(DataNode);
			}
		}
	}
}

void FVoxelCompilationNodeTree::RemoveAlreadyComputedNodes(const TSet<FVoxelCompilationNode*>& AlreadyComputedNodes)
{
	// Remove all nodes already in parents
	DataNodesSet = DataNodesSet.Difference(AlreadyComputedNodes);

	// Propagate to childs
	TSet<FVoxelCompilationNode*> AlreadyComputedNodesCopy = AlreadyComputedNodes;
	AlreadyComputedNodesCopy.Append(DataNodesSet);
	for (auto& Child : Children)
	{
		Child.RemoveAlreadyComputedNodes(AlreadyComputedNodesCopy);
	}
}

void FVoxelCompilationNodeTree::BakeSortedDataNodes()
{
	check(SortedDataNodes.Num() == 0);
	SortedDataNodes = DataNodesSet.Array();
	FVoxelGraphCompilerHelpers::SortNodes(SortedDataNodes);

	for (auto& Child : Children)
	{
		Child.BakeSortedDataNodes();
	}
}

inline bool ShouldComputeDataNode(EVoxelAxisDependencies NodeDependencies, EVoxelFunctionAxisDependencies FunctionDependencies)
{
	switch (NodeDependencies)
	{
	case EVoxelAxisDependencies::X:
		return
			FunctionDependencies == EVoxelFunctionAxisDependencies::X ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
	case EVoxelAxisDependencies::XY:
		return
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
	case EVoxelAxisDependencies::XYZ:
		return
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
	case EVoxelAxisDependencies::Constant:
	default:
		check(false);
		return false;
	}
}

inline bool AreExecNodeDependenciesInferiorOrEqual(EVoxelAxisDependencies NodeDependencies, EVoxelFunctionAxisDependencies FunctionDependencies)
{
	switch (NodeDependencies)
	{
	case EVoxelAxisDependencies::Constant:
	case EVoxelAxisDependencies::X:
		return true;
	case EVoxelAxisDependencies::XY:
		return
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
	case EVoxelAxisDependencies::XYZ:
		return
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithCache ||
			FunctionDependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
	default:
		check(false);
		return false;
	}
}

void FVoxelCompilationNodeTree::ConvertToComputeNodeTree(
	EVoxelFunctionAxisDependencies FunctionDependencies,
	FVoxelComputeNodeTree& OutTree,
	TArray<FVoxelFunctionCallComputeNode*>& OutFunctionCallsToLink,
	TSet<FVoxelCompilationNode*>& OutUsedNodes,
	FVoxelCreatedComputeNodes& CreatedNodes) const
{
	// Data nodes
	for (auto& DataNode : SortedDataNodes)
	{
		check(!DataNode->IsExecNode());
		if (DataNode->IsSeedNode() || ShouldComputeDataNode(FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(DataNode->Dependencies), FunctionDependencies))
		{
			OutUsedNodes.Add(DataNode);

			auto ComputeNode = CreatedNodes.GetComputeNode(*DataNode);

			if (ComputeNode->Type == EVoxelComputeNodeType::Data)
			{
				check(!DataNode->IsSeedNode());

				auto DataComputeNode = StaticCastSharedRef<FVoxelDataComputeNode>(ComputeNode);
				OutTree.DataNodes.Add(&DataComputeNode.Get());
				OutTree.DataNodesRefs.Add(DataComputeNode);
			}
			else
			{
				check(DataNode->IsSeedNode());
				check(ComputeNode->Type == EVoxelComputeNodeType::Seed);

				auto SeedComputeNode = StaticCastSharedRef<FVoxelSeedComputeNode>(ComputeNode);
				OutTree.SeedNodes.Add(&SeedComputeNode.Get());
				OutTree.SeedNodesRefs.Add(SeedComputeNode);
			}
		}
	}

	// Exec node
	if (ExecNode)
	{
		const EVoxelAxisDependencies NodeDependencies = FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(ExecNode->Dependencies);
	
		bool bCanComputeExecNode;
		switch (ExecNode->GetPrivateType())
		{
		case EVoxelCompilationNodeType::Passthrough:
		case EVoxelCompilationNodeType::FunctionInit:
		case EVoxelCompilationNodeType::Setter:
		{
			bCanComputeExecNode = true;
			break;
		}
		case EVoxelCompilationNodeType::If:
		case EVoxelCompilationNodeType::FunctionCall:
		{
			bCanComputeExecNode = AreExecNodeDependenciesInferiorOrEqual(NodeDependencies, FunctionDependencies);
			break;
		}
		default:
			bCanComputeExecNode = false;
			check(false);
		}

		if (bCanComputeExecNode)
		{
			OutUsedNodes.Add(ExecNode);

			TVoxelSharedPtr<FVoxelComputeNode> ComputeNode;
			if (ExecNode->IsA<FVoxelFunctionInitCompilationNode>())
			{
				ComputeNode = ExecNode->GetComputeNode();
			}
			else if (ExecNode->IsA<FVoxelFunctionCallCompilationNode>())
			{
				check(Children.Num() == 0);
				ComputeNode = CastCheckedVoxel<FVoxelFunctionCallCompilationNode>(ExecNode).GetComputeNode(FunctionDependencies);
			}
			else if (ExecNode->IsA<FVoxelSetterCompilationNode>())
			{
				if (FunctionDependencies != EVoxelFunctionAxisDependencies::XYZWithCache &&
					FunctionDependencies != EVoxelFunctionAxisDependencies::XYZWithoutCache)
				{
					ComputeNode = FVoxelGraphCompilerHelpers::GetPassthroughNode(EVoxelPinCategory::Exec, ExecNode->SourceNodes)->GetComputeNode();
				}
				else
				{
					ComputeNode = CreatedNodes.GetComputeNode(*ExecNode);
				}
			}
			else
			{
				ComputeNode = CreatedNodes.GetComputeNode(*ExecNode);
			}
			check(ComputeNode.IsValid());
			check(ComputeNode->Type == EVoxelComputeNodeType::Exec);

			const auto ExecComputeNode = StaticCastSharedPtr<FVoxelExecComputeNode>(ComputeNode);
			OutTree.ExecNode = ExecComputeNode.Get();
			OutTree.ExecNodeRef = ExecComputeNode;

			if (OutTree.ExecNode->ExecType == EVoxelComputeNodeExecType::FunctionCall)
			{
				OutFunctionCallsToLink.Add(static_cast<FVoxelFunctionCallComputeNode*>(OutTree.ExecNode));
			}

			for (auto& Child : Children)
			{
				Child.ConvertToComputeNodeTree(FunctionDependencies, *new (OutTree.Children) FVoxelComputeNodeTree(), OutFunctionCallsToLink, OutUsedNodes, CreatedNodes);
			}
		}
	}
}

