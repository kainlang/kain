// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNodes/VoxelGraphMacro.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphGlobals.h"

FVoxelGraphCompiler::FVoxelGraphCompiler(const TSharedRef<FVoxelGraphErrorReporter>& ErrorReporter)
	: ErrorReporter(*ErrorReporter)
	, ErrorReporterRef(ErrorReporter)
{
}

FVoxelGraphCompiler::FVoxelGraphCompiler(UVoxelGraphGenerator* Graph)
	: FVoxelGraphCompiler(MakeShared<FVoxelGraphErrorReporter>(Graph))
{
}

TSharedRef<FVoxelGraphCompiler> FVoxelGraphCompiler::Clone(const FString& ErrorPrefix, TMap<FVoxelCompilationNode*, FVoxelCompilationNode*>& OutOldNodesToNewNodes) const
{
	TSharedRef<FVoxelGraphCompiler> Result = MakeShareable(new FVoxelGraphCompiler(MakeShared<FVoxelGraphErrorReporter>(ErrorReporter, ErrorPrefix)));
	
	TMap<FVoxelCompilationPin*, FVoxelCompilationPin*> OldPinsToNewPins;
	FVoxelGraphCompilerHelpers::DuplicateNodes(*Result, Nodes, OldPinsToNewPins, OutOldNodesToNewNodes);
	Result->FirstNode = FirstNode ? OutOldNodesToNewNodes[FirstNode] : nullptr;
	Result->FirstNodePinIndex = FirstNodePinIndex;

	return Result;
}

TSharedRef<FVoxelGraphCompiler> FVoxelGraphCompiler::Clone(const FString& ErrorPrefix) const
{
	TMap<FVoxelCompilationNode*, FVoxelCompilationNode*> OldNodesToNewNodes;
	return Clone(ErrorPrefix, OldNodesToNewNodes);
}

TMap<UVoxelNode*, FVoxelCompilationNode*> FVoxelGraphCompiler::InitFromNodes(const TArray<UVoxelNode*>& InNodes, UVoxelNode* InFirstNode, int32 InFirstNodePinIndex)
{
	TMap<UVoxelNode*, FVoxelCompilationNode*> Map;

	// Convert nodes
	for (auto& Node : InNodes)
	{
		check(Node);
		Map.Add(Node, AddNode(Node));
	}
	if (ErrorReporter.HasError())
	{
		return {};
	}

	// Set default values
	for (auto& Node : InNodes)
	{
		FVoxelCompilationNode* CompilationNode = Map.FindChecked(Node);
		for (int32 I = 0; I < Node->InputPins.Num(); I++)
		{
			CompilationNode->GetInputPin(I).SetDefaultValue(Node->InputPins[I].DefaultValue);
		}
	}

	// Fix links
	for (auto& Node : InNodes)
	{
		FVoxelCompilationNode* CompilationNode = Map.FindChecked(Node);

		for (int32 PinIndex = 0; PinIndex < Node->InputPins.Num(); PinIndex++)
		{
			auto& NodeInputPin = Node->InputPins[PinIndex];
			auto& CompilationNodeInputPin = CompilationNode->GetInputPin(PinIndex);

			check(NodeInputPin.OtherNodes.Num() == NodeInputPin.OtherPinIds.Num());
			for (int32 I = 0; I < NodeInputPin.OtherNodes.Num(); I++)
			{
				auto OtherNode = NodeInputPin.OtherNodes[I];
				auto OtherPinId = NodeInputPin.OtherPinIds[I];
				if (!OtherNode)
				{
					ErrorReporter.AddError("Invalid node, please delete the node or use Voxel/Recreate Nodes");
					ErrorReporter.AddMessageToNode(CompilationNode, "An invalid node is linked to this node", EVoxelGraphNodeMessageType::Error);
					return {};
				}

				const int32 OtherPinIndex = OtherNode->GetOutputPinIndex(OtherPinId);
				check(OtherPinIndex >= 0);

				if (!Map.Contains(OtherNode))
				{
					ErrorReporter.AddMessageToNode(OtherNode, "Internal error: Node not found", EVoxelGraphNodeMessageType::Error);
					return {};
				}
				FVoxelCompilationNode* OtherCompilationNode = Map.FindChecked(OtherNode);
				auto& OtherPin = OtherCompilationNode->GetOutputPin(OtherPinIndex);

				if (!CompilationNodeInputPin.IsLinkedTo(OtherPin))
				{
					if (CompilationNodeInputPin.PinCategory != OtherPin.PinCategory)
					{
						ErrorReporter.AddMessageToNode(OtherNode, "Pins with different categories are linked together", EVoxelGraphNodeMessageType::Error);
						return {};
					}
					CompilationNodeInputPin.LinkTo(OtherPin);
				}
			}
		}

		for (int32 PinIndex = 0; PinIndex < Node->OutputPins.Num(); PinIndex++)
		{
			auto& NodeOutputPin = Node->OutputPins[PinIndex];
			auto& CompilationNodeOutputPin = CompilationNode->GetOutputPin(PinIndex);

			check(NodeOutputPin.OtherNodes.Num() == NodeOutputPin.OtherPinIds.Num());
			for (int32 I = 0; I < NodeOutputPin.OtherNodes.Num(); I++)
			{
				auto OtherNode = NodeOutputPin.OtherNodes[I];
				auto OtherPinId = NodeOutputPin.OtherPinIds[I];
				if (!OtherNode)
				{
					ErrorReporter.AddError("Invalid node, please delete the node or use Voxel/Recreate Nodes");
					ErrorReporter.AddMessageToNode(CompilationNode, "An invalid node is linked to this node", EVoxelGraphNodeMessageType::Error);
					return {};
				}

				const int32 OtherPinIndex = OtherNode->GetInputPinIndex(OtherPinId);
				check(OtherPinIndex >= 0);

				if (!Map.Contains(OtherNode))
				{
					ErrorReporter.AddMessageToNode(OtherNode, "Internal error: Node not found", EVoxelGraphNodeMessageType::Error);
					return {};
				}
				FVoxelCompilationNode* OtherCompilationNode = Map.FindChecked(OtherNode);
				auto& OtherPin = OtherCompilationNode->GetInputPin(OtherPinIndex);

				if (!CompilationNodeOutputPin.IsLinkedTo(OtherPin))
				{
					if (CompilationNodeOutputPin.PinCategory != OtherPin.PinCategory)
					{
						ErrorReporter.AddMessageToNode(OtherNode, "Pins with different categories are linked together", EVoxelGraphNodeMessageType::Error);
						return {};
					}
					CompilationNodeOutputPin.LinkTo(OtherPin);
				}
			}
		}
	}

	// Set first node
	FirstNode = InFirstNode ? Map.FindChecked(InFirstNode) : nullptr;
	FirstNodePinIndex = InFirstNodePinIndex;

	// Check validity
	Check();

	return Map;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelCompilationNode* FVoxelGraphCompiler::AddNode(UVoxelNode* Node)
{
	check(IsValid(Node));
	Node->LogErrors(ErrorReporter);
	if (!ErrorReporter.HasError())
	{
		return AddNode(Node->GetCompilationNode());
	}
	else
	{
		return nullptr;
	}
}

FVoxelCompilationNode* FVoxelGraphCompiler::AddNode(const TSharedPtr<FVoxelCompilationNode>& Ref, FVoxelCompilationNode* SourceNode)
{
	if (SourceNode)
	{
		Ref->SourceNodes.Append(SourceNode->SourceNodes);
	}
	
	NodesRefs.Add(Ref);
	Nodes.Add(Ref.Get());
	return Ref.Get();
}

void FVoxelGraphCompiler::RemoveNode(FVoxelCompilationNode* Node)
{
	ensureVoxelGraph(FirstNode != Node, Node); // Can't remove first node
	Nodes.Remove(Node);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphCompiler::Check() const
{
	VOXEL_FUNCTION_COUNTER();
	
	if (ErrorReporter.HasError())
	{
		return;
	}
	for (auto& Node : Nodes)
	{
		check(Node->GetInputCount() < MAX_VOXELNODE_PINS);
		check(Node->GetOutputCount() < MAX_VOXELNODE_PINS);
		Node->Check(ErrorReporter);
		if (Node->IsSeedNode() &&
			!Node->IsA<FVoxelMacroInputOuputCompilationNode>() &&
			!Node->IsA<FVoxelMacroCompilationNode>())
		{
			ensureVoxelGraph(FVoxelAxisDependencies::IsConstant(Node->Dependencies), Node);
			for (auto& Pin : Node->IteratePins<EVoxelPinIter::All>())
			{
				ensureVoxelGraph(Pin.PinCategory == EVoxelPinCategory::Seed, Node);
			}
		}
		for (auto& Pin : Node->IteratePins<EVoxelPinIter::All>())
		{
			Pin.Check(ErrorReporter);
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				ensureVoxelGraph(Nodes.Contains(&LinkedToPin.Node), Node);
			}
			if (Pin.Direction == EVoxelPinDirection::Output && Node->IsSeedNode() && !Node->IsA<FVoxelMacroCompilationNode>() && !Node->IsA<FVoxelMacroInputOuputCompilationNode>())
			{
				ensureVoxelGraph(Pin.PinCategory == EVoxelPinCategory::Seed, Node);
			}
		}
	}
	ensureVoxelGraph(!FirstNode || Nodes.Contains(FirstNode), nullptr);
	ensureVoxelGraph(!FirstNode || FirstNode->IsA<FVoxelFunctionInitCompilationNode>() || FirstNode->GetInputPin(FirstNodePinIndex).PinCategory == EVoxelPinCategory::Exec, nullptr);
}

void FVoxelGraphCompiler::AppendAndClear(FVoxelGraphCompiler& Other)
{
	NodesRefs.Append(Other.NodesRefs);
	Nodes.Append(Other.Nodes);

	Other.NodesRefs.Reset();
	Other.Nodes.Reset();
}

int32 FVoxelGraphCompiler::CompilationId = 0;

TVoxelSharedRef<FVoxelComputeNode> FVoxelCreatedComputeNodes::GetComputeNode(const FVoxelCompilationNode& Node)
{
	check(!Node.IsA<FVoxelFunctionInitCompilationNode>() && !Node.IsA<FVoxelFunctionCallCompilationNode>());

	auto& Result = Map.FindOrAdd(&Node);
	if (!Result.IsValid())
	{
		Result = Node.GetComputeNode();
	}
	return Result.ToSharedRef();
}
