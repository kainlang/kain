// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelDependenciesPass.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Runtime/VoxelComputeNode.h"
#include "VoxelGraphErrorReporter.h"

void FVoxelMarkDependenciesPass::Apply(FVoxelGraphCompiler& Compiler)
{
	// Mark
	for (auto* Node : Compiler.GetAllNodes())
	{
		// Functions separators depend on X, else their outputs are cached
		const uint8 Flag = Node->GetDefaultAxisDependencies();

		if (Flag != 0)
		{
			// Ignore exec successors as we can't affect them (no side effects)
			// Will still get the exec nodes with a data input
			TSet<FVoxelCompilationNode*> Successors;
			FVoxelGraphCompilerHelpers::GetAllDataSuccessors(Node, Successors);
			for (auto& Successor : Successors)
			{
				Successor->Dependencies |= Flag;
			}
		}
	}

	// All nodes after a branch node that are in only one branch must be flagged at least like it, unless they are constants. Need to loop to propagate changes
	bool bContinue = true;
	while (bContinue)
	{
		bContinue = false;
		for (auto* Node : Compiler.GetAllNodes())
		{
			if (Node->IsA<FVoxelIfCompilationNode>())
			{
				TSet<FVoxelCompilationNode*> AlwaysComputedNodes = FVoxelGraphCompilerHelpers::GetAlwaysComputedNodes(Node);
				TSet<FVoxelCompilationNode*> Successors = FVoxelGraphCompilerHelpers::GetAllExecSuccessorsAndTheirDataDependencies(Node, false);
				for (auto* Successor : Successors)
				{
					const bool bAlwaysComputed = AlwaysComputedNodes.Contains(Successor);
					check(!(Successor->IsExecNode() && bAlwaysComputed));
					if (!bAlwaysComputed && (Successor->IsExecNode() || !FVoxelAxisDependencies::IsConstant(Successor->Dependencies)))
					{
						const uint8 OldDependencies = Successor->Dependencies;
						Successor->Dependencies |= Node->Dependencies;
						bContinue |= (OldDependencies != Successor->Dependencies);
					}
				}
			}
		}
		// Propagate dependencies
		// Needed for some rare cases where a predecessor is used in more branches than a successor
		// See https://i.imgur.com/PBwJAz7.png ("X -" on the left)
		for (auto* Node : Compiler.GetAllNodes())
		{
			if (Node->Dependencies != 0)
			{
				TSet<FVoxelCompilationNode*> Successors;
				FVoxelGraphCompilerHelpers::GetAllDataSuccessors(Node, Successors);
				for (auto& Successor : Successors)
				{
					const uint8 OldDependencies = Successor->Dependencies;
					Successor->Dependencies |= Node->Dependencies;
					bContinue |= (OldDependencies != Successor->Dependencies);
				}
			}
		}
	}
}

void FVoxelDebugDependenciesPass::Apply(FVoxelGraphCompiler& Compiler)
{
	for (auto& Node : Compiler.GetAllNodes())
	{
		Compiler.ErrorReporter.AddMessageToNode(
			Node,
			FVoxelAxisDependencies::ToString(FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(Node->Dependencies)),
			EVoxelGraphNodeMessageType::Info,
			false,
			false);
	}
}

void FVoxelGetSortedConstantsAndRemoveConstantsPass::Apply(
	FVoxelGraphCompiler& Compiler, 
	FVoxelCreatedComputeNodes& CreatedNodes,
	TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& ConstantNodes,
	TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& SeedNodes)
{
	TArray<FVoxelCompilationNode*> Nodes;
	for (auto* Node : Compiler.GetAllNodes())
	{
		// Note: don't add any other checks here, or it'll break the compilation tree
		if (!Node->IsExecNode() && FVoxelAxisDependencies::IsConstant(Node->Dependencies))
		{
			Nodes.Add(Node);
		}
	}

	FVoxelGraphCompilerHelpers::SortNodes(Nodes);

	for (auto* Node : Nodes)
	{
		auto ComputeNode = CreatedNodes.GetComputeNode(*Node);

		if (Node->IsSeedNode())
		{
			check(ComputeNode->Type == EVoxelComputeNodeType::Seed);
			SeedNodes.Add(StaticCastSharedRef<FVoxelSeedComputeNode>(ComputeNode));
		}
		else
		{
			check(ComputeNode->Type == EVoxelComputeNodeType::Data);
			ConstantNodes.Add(StaticCastSharedRef<FVoxelDataComputeNode>(ComputeNode));

			// Don't remove seed nodes, as they are needed for each function initialization in C++ (as there is no global state)
			Node->BreakAllLinks();
			Compiler.RemoveNode(Node);
		}
	}
}
