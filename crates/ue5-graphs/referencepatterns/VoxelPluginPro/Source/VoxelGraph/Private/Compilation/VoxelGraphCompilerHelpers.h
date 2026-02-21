// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compilation/VoxelCompilationEnums.h"
#include "Compilation/VoxelCompilationNode.h"

class FVoxelGraphCompiler;
class FVoxelCompilationNode;
struct FVoxelCompilationPin;

namespace FVoxelGraphCompilerHelpers
{
	TSharedRef<FVoxelCompilationNode> GetPassthroughNode(EVoxelPinCategory Category, const TArray<const UVoxelNode*>& SourceNodes = {});

	FVoxelCompilationNode* AddPassthrough(FVoxelGraphCompiler& Compiler, FVoxelCompilationPin& Pin, FVoxelCompilationNode* SourceNode = nullptr);

	void DuplicateNodes(
		FVoxelGraphCompiler& Compiler, 
		const TSet<FVoxelCompilationNode*>& Nodes,
		TMap<FVoxelCompilationPin*, FVoxelCompilationPin*>& OutOldPinsToNewPins, 
		TMap<FVoxelCompilationNode*, FVoxelCompilationNode*>& OldNodesToNewNodes);

	void GetSortedExecNodes(FVoxelCompilationNode* FirstNode, TArray<FVoxelCompilationNode*>& OutNodes);

	void GetAllSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes);

	void GetAllPredecessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes);

	void GetAllDataSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes);

	void GetAllExecSuccessors(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes);

	bool HasSetterOrFunctionCallSuccessor(FVoxelCompilationNode* Node);

	void GetAllUsedNodes(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& OutNodes);

	bool IsDataNodeSuccessor(FVoxelCompilationNode* DataNode, FVoxelCompilationNode* PossibleSuccessor);

	bool AreAllNodePredecessorsChildOfStartNodeExecOnly(FVoxelCompilationNode* Node, FVoxelCompilationNode* StartNode, FVoxelCompilationNode*& FaultyNode);

	void GetFunctionNodes(FVoxelCompilationNode* FunctionStartNode, TSet<FVoxelCompilationNode*>& OutNodes);

	void SortNodes(TArray<FVoxelCompilationNode*>& Nodes);

	void AddPreviousNodesToSet(FVoxelCompilationNode* Node, TSet<FVoxelCompilationNode*>& Nodes);

	FVoxelCompilationNode* GetPreviousFlowMergeOrFunctionSeparatorNode(FVoxelCompilationNode* Node);

	// Won't add function parameters as dependencies
	TSet<FVoxelCompilationNode*> GetAllExecSuccessorsAndTheirDataDependencies(FVoxelCompilationNode* FirstNode, bool bAddFirstNodeDataDependencies);

	// Will find all data nodes children that are always computed, no matter the branch
	TSet<FVoxelCompilationNode*> GetAlwaysComputedNodes(FVoxelCompilationNode* Node);

	// Will remove all nodes that have a successor in the set
	TSet<FVoxelCompilationNode*> FilterHeads(const TSet<FVoxelCompilationNode*>& Nodes);

	template<EVoxelPinIter Direction>
	void BreakNodeLinks(FVoxelCompilationNode& Node)
	{
		for (auto& Pin : Node.IteratePins<Direction>())
		{
			Pin.BreakAllLinks();
		}
	}

	// Like UE Ctrl Drag & Drop
	void MovePin(FVoxelCompilationPin& From, FVoxelCompilationPin& To);
	void MovePin(FVoxelCompilationPin& From, const TArray<FVoxelCompilationPin*>& To);
	void MoveInputPins(FVoxelCompilationNode& From, FVoxelCompilationNode& To);
	void MoveOutputPins(FVoxelCompilationNode& From, FVoxelCompilationNode& To);
}
