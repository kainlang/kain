// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGetRangeAnalysisPass.h"
#include "Compilation/Passes/VoxelSetIdsPass.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelCompilationNodeTree.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "VoxelNodes/VoxelExecNodes.h"
#include "VoxelGraphErrorReporter.h"

void FVoxelGetRangeAnalysisPass::Apply(FVoxelGraphCompiler& Compiler)
{
	TArray<FVoxelGetRangeAnalysisCompilationNode*> GetRangeAnalysisNodes;
	for (auto* NodeIt : Compiler.GetAllNodes())
	{
		auto* Node = CastVoxel<FVoxelGetRangeAnalysisCompilationNode>(NodeIt);
		if (!Node)
		{
			continue;
		}

		GetRangeAnalysisNodes.Add(Node);

		TSet<FVoxelCompilationNode*> Predecessors;
		FVoxelGraphCompilerHelpers::GetAllPredecessors(Node, Predecessors);
		for (auto* It : Predecessors)
		{
			if (It == Node)
			{
				continue;
			}
			if (It->IsExecNode())
			{
				Compiler.ErrorReporter.AddMessageToNode(Node, "cannot have an exec node before a GetRangeAnalysis node", EVoxelGraphNodeMessageType::Error);
				return;
			}
			if (It->IsA<FVoxelGetRangeAnalysisCompilationNode>())
			{
				Compiler.ErrorReporter.AddMessageToNode(Node, "cannot have a GetRangeAnalysis node before a GetRangeAnalysis node", EVoxelGraphNodeMessageType::Error);
				return;
			}
		}
	}

	for (auto* PreviousGetRangeAnalysisNode : GetRangeAnalysisNodes)
	{
		TMap<FVoxelCompilationNode*, FVoxelCompilationNode*> OldNodesToNewNodes;
		const auto LocalCompiler = Compiler.Clone("GetRangeAnalysis", OldNodesToNewNodes);

		FVoxelCompilationNode* SetValue;
		
		// Add SetValue node
		{
			SetValue = LocalCompiler->AddNode(GetMutableDefault<UVoxelNode_SetValueNode>()->GetCompilationNode());

			auto& GetRangeAnalysisNode = *OldNodesToNewNodes[PreviousGetRangeAnalysisNode];
			FVoxelGraphCompilerHelpers::MovePin(GetRangeAnalysisNode.GetInputPin(0), SetValue->GetInputPin(1));

			// Set the first node now, so we can remove the original one
			LocalCompiler->FirstNode = SetValue;
			LocalCompiler->FirstNodePinIndex = 0;
		}

		TSet<FVoxelCompilationNode*> Predecessors;
		FVoxelGraphCompilerHelpers::GetAllPredecessors(SetValue, Predecessors);

		// Compute GetRange dependencies
		uint8 Dependencies = 0;
		for (auto* Predecessor : Predecessors)
		{
			Dependencies |= Predecessor->GetDefaultAxisDependencies();
		}
		
		// Remove all other nodes
		for (auto* NodeIt : LocalCompiler->GetAllNodesCopy())
		{
			// Force set dependencies to not have any constant node
			NodeIt->Dependencies = EVoxelAxisDependenciesFlags::XYZ;
			if (!Predecessors.Contains(NodeIt))
			{
				NodeIt->BreakAllLinks();
				LocalCompiler->RemoveNode(NodeIt);
			}
		}

		// Assign ids
		int32 Id = 0;
		LocalCompiler->ApplyPass<FVoxelSetPinsIdsPass>(Id);

		// Create the tree
		const TVoxelSharedRef<FVoxelComputeNodeTree> ComputeTree = MakeVoxelShared<FVoxelComputeNodeTree>();
		{
			const auto Tree = FVoxelCompilationNodeTree::Create(&*SetValue);

			TArray<FVoxelFunctionCallComputeNode*> FunctionCallsToLink;
			TSet<FVoxelCompilationNode*> UsedNodes;
			FVoxelCreatedComputeNodes CreatedNodes;
			Tree->ConvertToComputeNodeTree(EVoxelFunctionAxisDependencies::XYZWithoutCache, *ComputeTree, FunctionCallsToLink, UsedNodes, CreatedNodes);
			ensure(FunctionCallsToLink.Num() == 0);
		}

		check(PreviousGetRangeAnalysisNode->VariablesBufferSize == -1 && !PreviousGetRangeAnalysisNode->Tree);
		PreviousGetRangeAnalysisNode->VariablesBufferSize = Id;
		PreviousGetRangeAnalysisNode->Tree = ComputeTree;
		// Make sure we always have a context
		PreviousGetRangeAnalysisNode->DefaultDependencies = FMath::Max<uint8>(Dependencies, EVoxelAxisDependenciesFlags::X);

		// Disconnect the input
		PreviousGetRangeAnalysisNode->GetInputPin(0).BreakAllLinks();
	}
}
