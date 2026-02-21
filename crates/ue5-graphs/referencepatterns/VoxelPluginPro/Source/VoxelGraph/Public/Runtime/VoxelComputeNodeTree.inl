// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelDefaultComputeNodes.h"
#include "VoxelNodes/VoxelIfNode.h"

template<typename T>
const FVoxelGraphFunction* FVoxelComputeNodeTree::Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, T& Recorder) const
{
	for (int32 Index = 0; Index < DataNodes.Num(); Index++)
	{
		FVoxelNodeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeType NodeOutputBuffer[MAX_VOXELNODE_PINS];

		auto* Node = DataNodes.GetData()[Index];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		{
			auto Scope = Recorder.MakeDataScope(Node, NodeInputBuffer, NodeOutputBuffer, Context);
			(Node->*(Node->ComputeFunctionPtr))(NodeInputBuffer, NodeOutputBuffer, Context);
		}
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}

	if (!ExecNode)
	{
		return nullptr;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			checkVoxelGraph(Children.Num() == 1);
			return Children.GetData()[0].Compute(Context, Buffers, Recorder);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::If:
	{
		checkVoxelGraph(Children.Num() == 2);
		const int32 InputId = ExecNode->GetInputId(0);
		const bool bCondition = InputId == -1 ? ExecNode->GetDefaultValue<FVoxelNodeType>(0).Get<bool>() : Buffers.Variables[InputId].Get<bool>();
		return Children.GetData()[bCondition ? 0 : 1].Compute(Context, Buffers, Recorder);
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		FVoxelNodeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		ExecNode->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		
		{
			auto Scope = Recorder.MakeSetterScope(ExecNode, NodeInputBuffer, Context);
			static_cast<FVoxelSetterComputeNode*>(ExecNode)->ComputeSetterNode(NodeInputBuffer, Buffers.GraphOutputs);
		}
		
		if (Children.Num() > 0)
		{
			checkVoxelGraph(Children.Num() == 1);
			return Children.GetData()[0].Compute(Context, Buffers, Recorder);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		checkVoxelGraph(Children.Num() == 0);
		ExecNode->CopyVariablesToInputs(Buffers.Variables, Buffers.FunctionInputsOutputs);
		return static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
	}
	default:
	{
		checkVoxelSlow(false);
		return nullptr;
	}
	}
}

template<typename T>
const FVoxelGraphFunction* FVoxelComputeNodeTree::ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers, T& Recorder) const
{
	auto& RangeFailStatus = FVoxelRangeFailStatus::Get();
	for (auto& Node : DataNodes)
	{
		ensureVoxelSlowNoSideEffects(!RangeFailStatus.HasFailed());
		
		FVoxelNodeRangeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeRangeType NodeOutputBuffer[MAX_VOXELNODE_PINS];

		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		{
			auto Scope = Recorder.MakeDataScope(Node, NodeInputBuffer, NodeOutputBuffer, Context);
			Node->ComputeRange(NodeInputBuffer, NodeOutputBuffer, Context);
		}
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);

		if (RangeFailStatus.HasFailed())
		{
			return nullptr;
		}
	}

	if (!ExecNode)
	{
		return nullptr;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			return Children[0].ComputeRange(Context, Buffers, Recorder);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::If:
	{
		checkVoxelSlow(Children.Num() == 2);
		const int32 InputId = ExecNode->GetInputId(0);

		bool bCondition;
		{
			auto Scope = Recorder.MakeIfScope(ExecNode);
			
			bCondition =
				InputId == -1
				? bool(ExecNode->GetDefaultValue<FVoxelNodeType>(0).Get<bool>())
				: bool(Buffers.Variables[InputId].Get<bool>());

			if (RangeFailStatus.HasFailed()) // The bool is unknown
			{
				auto* IfNode = static_cast<FVoxelIfComputeNode*>(ExecNode);
				if (IfNode->BranchToUseForRangeAnalysis != EVoxelNodeIfBranchToUseForRangeAnalysis::None)
				{
					bCondition = IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseTrue;
					checkVoxelSlow(bCondition || IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseFalse);

					RangeFailStatus.Reset();
				}
				else
				{
					return nullptr;
				}
			}
		}
		
		return Children[bCondition ? 0 : 1].ComputeRange(Context, Buffers, Recorder);
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		FVoxelNodeRangeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		ExecNode->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		
		{
			auto Scope = Recorder.MakeSetterScope(ExecNode, NodeInputBuffer, Context);
			static_cast<FVoxelSetterComputeNode*>(ExecNode)->ComputeRangeSetterNode(NodeInputBuffer, Buffers.GraphOutputs);
		}
		
		if (Children.Num() > 0)
		{
			return Children[0].ComputeRange(Context, Buffers, Recorder);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		checkVoxelSlow(Children.Num() == 0);
		ExecNode->CopyVariablesToInputs(Buffers.Variables, Buffers.FunctionInputsOutputs);
		return static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
	}
	default:
	{
		check(false);
		return nullptr;
	}
	}
}
