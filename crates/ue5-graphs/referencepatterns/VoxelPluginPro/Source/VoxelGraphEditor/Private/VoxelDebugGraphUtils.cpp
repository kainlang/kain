// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDebugGraphUtils.h"
#include "VoxelGraphGenerator.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "VoxelGraphNodes/VoxelGraphNode.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"

void FVoxelDebugGraphUtils::DebugNodes(const TSet<FVoxelCompilationNode*>& Nodes, UVoxelGraphGenerator* Generator)
{
	struct FVoxelTmpLink
	{
		FVoxelCompilationNode* From;
		int32 FromIndex;
		FVoxelCompilationNode* To;
		int32 ToIndex;
	};

	UEdGraph* const Graph = Generator->VoxelDebugGraph;
	Graph->Modify();
	Generator->Modify();

	// Delete existing nodes
	{
		auto NodesCopy = Graph->Nodes; // Copy as we are modifying it
		for (auto& Node : NodesCopy)
		{
			Graph->RemoveNode(Node);
		}
	}
	Generator->DebugNodes.Reset();

	int32 MaxSourceNodesDepth = 0;
	for (auto& Node : Nodes)
	{
		MaxSourceNodesDepth = FMath::Max(MaxSourceNodesDepth, Node->SourceNodes.Num());
	}

	TMap<FVoxelCompilationNode*, UVoxelGraphNode*> NodeMap;
	TArray<FVoxelTmpLink> LinksToCreate;
	for (auto* CompilationNode : Nodes)
	{
		UVoxelDebugNode* VoxelNode = NewObject<UVoxelDebugNode>();
		Generator->DebugNodes.Add(VoxelNode);

		VoxelNode->CompilationNode = CompilationNode->Clone();
		VoxelNode->Name = FText::FromString(CompilationNode->GetPrettyName());
		VoxelNode->Tooltip = FText::FromString("Class: " + CompilationNode->GetClassName());
		if (CompilationNode->SourceNodes.Num() > 0)
		{
			auto* SourceNode = CompilationNode->SourceNodes.Last();
			VoxelNode->Color = SourceNode->GetColor();
			VoxelNode->bIsCompact = SourceNode->IsCompact();
		}

		FGraphNodeCreator<UVoxelGraphNode> NodeCreator(*Graph);
		UVoxelGraphNode* GraphNode = NodeCreator.CreateNode();
		for (auto& Message : CompilationNode->DebugMessages)
		{
			GraphNode->InfoMsg += Message;
		}
		
		VoxelNode->GraphNode = GraphNode;
		GraphNode->SetVoxelNode(VoxelNode);
		NodeCreator.Finalize();

		FString& InfoMsg = CastChecked<UVoxelGraphNodeInterface>(GraphNode)->InfoMsg;
		if (Generator->bShowPinsIds)
		{
			int32 InputIdsCount = CompilationNode->GetInputCountWithoutExecs();
			int32 OutputIdsCount = CompilationNode->GetOutputCountWithoutExecs();
			for (int32 Index = 0; Index < FMath::Max(InputIdsCount, OutputIdsCount); Index++)
			{
				if (!InfoMsg.IsEmpty())
				{
					InfoMsg += "\n";
				}
				if (Index < InputIdsCount)
				{
					InfoMsg += FString::FromInt(CompilationNode->GetInputId(Index));
				}
				InfoMsg += "\t\t";
				if (Index < OutputIdsCount)
				{
					InfoMsg += FString::FromInt(CompilationNode->GetOutputId(Index));
				}
			}
		}
		if (Generator->bShowAxisDependencies)
		{
			if (!InfoMsg.IsEmpty())
			{
				InfoMsg += "\n";
			}
			InfoMsg += FVoxelAxisDependencies::ToString(FVoxelAxisDependencies::GetVoxelAxisDependenciesFromFlag(CompilationNode->Dependencies));
		}

		// Pin Debug
		for (int32 Index = 0; Index < CompilationNode->GetInputCount() ; Index++)
		{
			GraphNode->GetInputPin(Index)->PinToolTip = "NumLinkedTo: " + FString::FromInt(CompilationNode->GetInputPin(Index).NumLinkedTo());
		}
		for (int32 Index = 0; Index < CompilationNode->GetOutputCount(); Index++)
		{
			GraphNode->GetOutputPin(Index)->PinToolTip = "NumLinkedTo: " + FString::FromInt(CompilationNode->GetOutputPin(Index).NumLinkedTo());
		}

		GraphNode->NodePosX = 0;
		GraphNode->NodePosY = 0;

		for (int32 Index = 0; Index < MaxSourceNodesDepth; Index++)
		{
			GraphNode->NodePosX *= Generator->NodesDepthScaleFactor;
			GraphNode->NodePosY *= Generator->NodesDepthScaleFactor;

			auto& SourceNodes = CompilationNode->SourceNodes;
			if (SourceNodes.IsValidIndex(Index))
			{
				auto* SourceNode = SourceNodes.Last(Index);
				if (SourceNode && SourceNode->GraphNode)
				{
					GraphNode->NodePosX += SourceNode->GraphNode->NodePosX;
					GraphNode->NodePosY += SourceNode->GraphNode->NodePosY;
				}
			}
		}


		NodeMap.Add(CompilationNode, GraphNode);

		for (int32 I = 0; I < CompilationNode->GetOutputCount(); I++)
		{
			auto& Pin = CompilationNode->GetOutputPin(I);
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				auto& OtherNode = LinkedToPin.Node;

				if (Nodes.Contains(&OtherNode) && (Pin.PinCategory == EVoxelPinCategory::Exec || !Generator->bHideDataNodes))
				{
					LinksToCreate.Add({ CompilationNode, I, &OtherNode, LinkedToPin.Index });
				}
			}
		}
		for (int32 I = 0; I < CompilationNode->GetInputCount(); I++)
		{
			auto& Pin = CompilationNode->GetInputPin(I);
			for (auto& LinkedToPin : Pin.IterateLinkedTo())
			{
				auto& OtherNode = LinkedToPin.Node;

				if (Nodes.Contains(&OtherNode) && (Pin.PinCategory == EVoxelPinCategory::Exec || !Generator->bHideDataNodes))
				{
					LinksToCreate.Add({ &OtherNode, LinkedToPin.Index, CompilationNode, I });
				}
			}
		}
	}

	for (auto& Link : LinksToCreate)
	{
		auto* From = NodeMap[Link.From];
		auto* To = NodeMap[Link.To];

		auto* FromPin = From->GetOutputPin(Link.FromIndex);
		auto* ToPin = To->GetInputPin(Link.ToIndex);

		FromPin->MakeLinkTo(ToPin);
	}
}
