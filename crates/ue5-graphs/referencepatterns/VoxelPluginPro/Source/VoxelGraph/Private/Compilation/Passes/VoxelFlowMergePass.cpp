// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/VoxelFlowMergePass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNodes/VoxelExecNodes.h"
#include "VoxelGraphErrorReporter.h"

inline void ReplaceFlowMerge(FVoxelGraphCompiler& Compiler, FVoxelFlowMergeCompilationNode* FlowMergeNode)
{
	const FString& Name = FlowMergeNode->GetPrettyName();
	const auto& Types = CastChecked<UVoxelNode_FlowMerge>(&FlowMergeNode->Node)->Types;

	FVoxelCompilationNode* PassthroughExecOutput;
	TArray<FVoxelCompilationNode*> PassthroughValueOutputs;

	FVoxelCompilationNode* PassthroughExecInputA;
	TArray<FVoxelCompilationNode*> PassthroughValueInputsA;
	FVoxelCompilationNode* PassthroughExecInputB;
	TArray<FVoxelCompilationNode*> PassthroughValueInputsB;

	{
		int32 OutputIndex = 0;
		auto& ExecOutputPin = FlowMergeNode->GetOutputPin(OutputIndex++);
		PassthroughExecOutput = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ExecOutputPin);
		for (int32 Index = 0; Index < Types.Num(); Index++)
		{
			auto& ValueOutputPin = FlowMergeNode->GetOutputPin(OutputIndex++);
			PassthroughValueOutputs.Add(FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ValueOutputPin));
		}

		int32 InputIndex = 0;
		auto& ExecInputPinA = FlowMergeNode->GetInputPin(InputIndex++);
		PassthroughExecInputA = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ExecInputPinA);
		for (int32 Index = 0; Index < Types.Num(); Index++)
		{
			auto& ValueInputPinA = FlowMergeNode->GetInputPin(InputIndex++);
			PassthroughValueInputsA.Add(FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ValueInputPinA));
		}
		auto& ExecInputPinB = FlowMergeNode->GetInputPin(InputIndex++);
		PassthroughExecInputB = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ExecInputPinB);
		for (int32 Index = 0; Index < Types.Num(); Index++)
		{
			auto& ValueInputPinB = FlowMergeNode->GetInputPin(InputIndex++);
			PassthroughValueInputsB.Add(FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ValueInputPinB));
		}
	}
	check(PassthroughValueOutputs.Num() == Types.Num());
	check(PassthroughValueInputsA.Num() == Types.Num());
	check(PassthroughValueInputsB.Num() == Types.Num());

	TSet<FVoxelCompilationNode*> NodesToDuplicate;
	FVoxelGraphCompilerHelpers::GetAllSuccessors(FlowMergeNode, NodesToDuplicate);
	NodesToDuplicate.Remove(FlowMergeNode);

	for (auto& Node : NodesToDuplicate)
	{
		FVoxelCompilationNode* FaultyNode = nullptr;
		if (Node->IsExecNode() && !FVoxelGraphCompilerHelpers::AreAllNodePredecessorsChildOfStartNodeExecOnly(Node, FlowMergeNode, FaultyNode))
		{
			Compiler.ErrorReporter.AddError("Invalid FlowMerge node " + Name + ": child " + Node->GetPrettyName() + " has a parent that isn't child of this flow merge node: " + FaultyNode->GetPrettyName());
			Compiler.ErrorReporter.AddNodeToSelect(FlowMergeNode);
			Compiler.ErrorReporter.AddNodeToSelect(Node);
			Compiler.ErrorReporter.AddNodeToSelect(FaultyNode);
			Compiler.ErrorReporter.AddMessageToNode(FlowMergeNode, "flow merge", EVoxelGraphNodeMessageType::Info);
			Compiler.ErrorReporter.AddMessageToNode(Node, "child", EVoxelGraphNodeMessageType::Info);
			Compiler.ErrorReporter.AddMessageToNode(FaultyNode, "should be child of flow merge", EVoxelGraphNodeMessageType::Error);
			return;
		}
	}

	TMap<FVoxelCompilationPin*, FVoxelCompilationPin*> OldPinsToNewPins;
	TMap<FVoxelCompilationNode*, FVoxelCompilationNode*> OldNodesToNewNodes;
	FVoxelGraphCompilerHelpers::DuplicateNodes(Compiler, NodesToDuplicate, OldPinsToNewPins, OldNodesToNewNodes);

	if (Compiler.ErrorReporter.HasError())
	{
		return;
	}

	PassthroughExecInputA->GetOutputPin(0).LinkTo(PassthroughExecOutput->GetInputPin(0));
	PassthroughExecInputB->GetOutputPin(0).LinkTo(*OldPinsToNewPins[&PassthroughExecOutput->GetInputPin(0)]);
	for (int32 Index = 0; Index < Types.Num(); Index++)
	{
		PassthroughValueInputsA[Index]->GetOutputPin(0).LinkTo(PassthroughValueOutputs[Index]->GetInputPin(0));
		PassthroughValueInputsB[Index]->GetOutputPin(0).LinkTo(*OldPinsToNewPins[&PassthroughValueOutputs[Index]->GetInputPin(0)]);
	}

	FlowMergeNode->BreakAllLinks();
	Compiler.RemoveNode(FlowMergeNode);
}



void FVoxelReplaceFlowMergePass::Apply(FVoxelGraphCompiler& Compiler)
{
	bool bContinue = true;
	while (bContinue && !Compiler.ErrorReporter.HasError())
	{
		bContinue = false;

		TArray<FVoxelCompilationNode*> SortedNodes;
		FVoxelGraphCompilerHelpers::GetSortedExecNodes(Compiler.FirstNode, SortedNodes);

		for (auto& Node : SortedNodes)
		{
			if (auto* FlowMerge = CastVoxel<FVoxelFlowMergeCompilationNode>(Node))
			{
				ReplaceFlowMerge(Compiler, FlowMerge);
				bContinue = true;
				break;
			}
			if (Compiler.ErrorReporter.HasError())
			{
				return;
			}
		}
	}

	if (!Compiler.ErrorReporter.HasError())
	{
		TSet<FVoxelCompilationNode*> Nodes;
		FVoxelGraphCompilerHelpers::GetAllUsedNodes(Compiler.FirstNode, Nodes);
		for (auto& Node : Nodes)
		{
			if (Node->IsA<FVoxelFlowMergeCompilationNode>())
			{
				Compiler.ErrorReporter.AddMessageToNode(Node, "FlowMerge data output is used but exec output isn't connected", EVoxelGraphNodeMessageType::Error);
				break;
			}
		}
	}
}

void FVoxelFixMultipleOutputsExecPass::Apply(FVoxelGraphCompiler& Compiler)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (Node->GetExecOutputCount() > 1)
		{
			for (auto& OutputPin : Node->IteratePins<EVoxelPinIter::Output>())
			{
				if (OutputPin.PinCategory == EVoxelPinCategory::Exec && OutputPin.NumLinkedTo() == 0)
				{
					FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, OutputPin);
				}
			}
		}
	}
}
