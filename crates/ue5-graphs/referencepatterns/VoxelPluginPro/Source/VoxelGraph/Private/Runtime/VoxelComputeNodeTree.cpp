// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelDefaultComputeNodes.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelNodes/VoxelIfNode.h"
#include "VoxelGraphGenerator.h"

void FVoxelComputeNodeTree::Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const
{
	for (auto& Node : SeedNodes)
	{
		FVoxelGraphSeed NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelGraphSeed NodeOutputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, NodeOutputBuffer, InitStruct);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}

	for (auto& Node : DataNodes)
	{
		FVoxelGraphSeed NodeInputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, InitStruct);
		Node->CacheFunctionPtr();
	}

	if (ExecNode)
	{
		// Used only for materials node to check that the right config is used
		ExecNode->Init(InitStruct);
	}

	for (auto& Child : Children)
	{
		Child.Init(InitStruct, Buffers);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelComputeNodeTree::GetNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	for (auto& Node : DataNodes)
	{
		Nodes.Add(Node);
	}

	if (ExecNode)
	{
		Nodes.Add(ExecNode);
	}

	for (auto& Child : Children)
	{
		Child.GetNodes(Nodes);
	}
}

void FVoxelComputeNodeTree::InitCpp(FVoxelCppConstructor& Constructor) const
{
	for (auto& Node : SeedNodes)
	{
		Constructor.QueueComment("// Init of " + Node->PrettyName);
		Node->CallInitCpp(Constructor);
		Constructor.EndComment();
	}

	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment("// Init of " + Node->PrettyName);
		Node->CallInitCpp(Constructor);
		Constructor.EndComment();
	}

	for (auto& Child : Children)
	{
		Child.InitCpp(Constructor);
	}
}

void FVoxelComputeNodeTree::ComputeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs)
{
	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment(FString::Printf(TEXT("// %s"), *Node->PrettyName));
		Node->CallComputeCpp(Constructor, VariableInfo);
		Constructor.EndComment();
	}

	if (!ExecNode)
	{
		return;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::If:
	{
		const int32 InputId = ExecNode->GetInputId(0);
		const FString Condition = InputId == -1 ? ExecNode->GetDefaultValueString(0) : Constructor.GetVariable(InputId, ExecNode);
		Constructor.AddLine("if (" + Condition + ")");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		Constructor.AddLine("else");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[1].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		break;
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->CallComputeSetterNodeCpp(Constructor, VariableInfo, GraphOutputs);
		if (Children.Num() > 0) // Only a setter can have no children (cf FVoxelRemoveUnusedExecsPass)
		{
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		check(Children.Num() == 0);
		auto* Function = static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
		Function->Call(Constructor, ExecNode->GetInputsNamesCpp(Constructor), EVoxelFunctionType::Compute);
		break;
	}
	default:
	{
		check(false);
		break;
	}
	}
}

void FVoxelComputeNodeTree::ComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs)
{
	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment(FString::Printf(TEXT("// %s"), *Node->PrettyName));
		Node->CallComputeRangeCpp(Constructor, VariableInfo);
		Constructor.EndComment();
	}

	if (!ExecNode)
	{
		return;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::If:
	{
		const int32 InputId = ExecNode->GetInputId(0);
		const FString Condition = InputId == -1 ? ExecNode->GetDefaultValueString(0) : Constructor.GetVariable(InputId, ExecNode);
		auto* IfNode = static_cast<FVoxelIfComputeNode*>(ExecNode);
		if (IfNode->BranchToUseForRangeAnalysis != EVoxelNodeIfBranchToUseForRangeAnalysis::None)
		{
			const bool bCondition = IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseTrue;
			check(bCondition || IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseFalse);
			Constructor.AddLine("if (FVoxelBoolRange::If(" + Condition + ", " + FString(bCondition ? "true" : "false") + "))");
		}
		else
		{
			Constructor.AddLine("if (" + Condition + ")");
		}
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		Constructor.AddLine("else");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[1].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		break;
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->CallComputeRangeSetterNodeCpp(Constructor, VariableInfo, GraphOutputs);
		if (Children.Num() > 0) // Only a setter can have no children (cf FVoxelRemoveUnusedExecsPass)
		{
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		check(Children.Num() == 0);
		auto* Function = static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
		Function->Call(Constructor, ExecNode->GetInputsNamesCpp(Constructor), EVoxelFunctionType::Compute);
		break;
	}
	default:
	{
		check(false);
		break;
	}
	}
}
