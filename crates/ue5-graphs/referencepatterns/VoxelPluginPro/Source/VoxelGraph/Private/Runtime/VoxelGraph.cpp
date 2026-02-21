// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGraphConstants.h"
#include "VoxelContext.h"

FVoxelGraphFunctions::FVoxelGraphFunctions(
	int32 FunctionId,
	const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionX,
	const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithCache,
	const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithoutCache, 
	const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithCache, 
	const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithoutCache)
	: FunctionId(FunctionId)
	, FunctionX(FunctionX)
	, FunctionXYWithCache(FunctionXYWithCache)
	, FunctionXYWithoutCache(FunctionXYWithoutCache)
	, FunctionXYZWithCache(FunctionXYZWithCache)
	, FunctionXYZWithoutCache(FunctionXYZWithoutCache)
{
	check(FunctionX);
	check(FunctionXYWithCache);
	check(FunctionXYWithoutCache);
	check(FunctionXYZWithCache);
	check(FunctionXYZWithoutCache);
}

FVoxelGraph::FVoxelGraph(
	const FString& Name,
	const TArray<FVoxelGraphFunctions>& AllFunctions,
	const FVoxelGraphFunctions& FirstFunctions,
	const TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& ConstantComputeNodes,
	const TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& SeedComputeNodes,
	int32 VariablesBufferSize)
	: Name(Name)
	, AllFunctions(AllFunctions)
	, FirstFunctions(FirstFunctions)
	, ConstantComputeNodes(ConstantComputeNodes)
	, SeedComputeNodes(SeedComputeNodes)
	, VariablesBufferSize(VariablesBufferSize)
{
	for (auto& Functions : AllFunctions)
	{
		check(Functions.IsValid());
	}
}

void FVoxelGraph::Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const
{
	// First init seeds
	for (auto& Node : SeedComputeNodes)
	{
		FVoxelGraphSeed NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelGraphSeed NodeOutputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, NodeOutputBuffer, InitStruct);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}

	// Then constant nodes
	for (auto& Node : ConstantComputeNodes)
	{
		FVoxelGraphSeed NodeInputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, InitStruct);
	}

	// And then other nodes
	for (auto& Functions : AllFunctions)
	{
		for (auto& Function : Functions.Iterate())
		{
			if (Function->IsUsedForInit())
			{
				Function->Init(InitStruct, Buffers);
			}
		}
	}
}

void FVoxelGraph::ComputeConstants(FVoxelGraphVMComputeBuffers& Buffers) const
{
	for (auto& Node : ConstantComputeNodes)
	{
		FVoxelNodeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeType NodeOutputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Compute(NodeInputBuffer, NodeOutputBuffer, FVoxelContext::EmptyContext);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}
}

void FVoxelGraph::ComputeRangeConstants(FVoxelGraphVMComputeRangeBuffers& Buffers) const
{
	FVoxelRangeFailStatus::Get().Reset();
	for (auto& Node : ConstantComputeNodes)
	{
		FVoxelNodeRangeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeRangeType NodeOutputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->ComputeRange(NodeInputBuffer, NodeOutputBuffer, FVoxelContextRange::EmptyContext);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}
	ensureAlwaysMsgf(!FVoxelRangeFailStatus::Get().HasFailed(), TEXT("A constant node has failed range analysis. This isn't supported!"));
}

void FVoxelGraph::GetSeedNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	for (auto& Node : SeedComputeNodes)
	{
		Nodes.Add(&Node.Get());
	}
}

void FVoxelGraph::GetConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	for (auto& Node : ConstantComputeNodes)
	{
		Nodes.Add(&Node.Get());
	}
}

void FVoxelGraph::GetNotConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	for (auto& Functions : AllFunctions)
	{
		for (auto& Function : Functions.Iterate())
		{
			Function->GetNodes(Nodes);
		}
	}
}

void FVoxelGraph::GetAllNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	GetSeedNodes(Nodes);
	GetConstantNodes(Nodes);
	GetNotConstantNodes(Nodes);
}

void FVoxelGraph::Init(FVoxelCppConstructor& Constructor) const
{
	Constructor.AddLine("////////////////////////////////////////////////////");
	Constructor.AddLine("/////////////// Constant nodes init ////////////////");
	Constructor.AddLine("////////////////////////////////////////////////////");
	Constructor.StartBlock();
	{
		FVoxelCppVariableScope Scope(Constructor);

		Constructor.AddLine("/////////////////////////////////////////////////////////////////////////////////");
		Constructor.AddLine("//////// First compute all seeds in case they are used by constant nodes ////////");
		Constructor.AddLine("/////////////////////////////////////////////////////////////////////////////////");
		Constructor.NewLine();
		
		// First init seeds
		for (auto& Node : SeedComputeNodes)
		{
			Constructor.QueueComment("// Init of " + Node->PrettyName);
			Node->CallInitCpp(Constructor);
			Constructor.EndComment();
		}

		Constructor.NewLine();
		Constructor.AddLine("////////////////////////////////////////////////////");
		Constructor.AddLine("///////////// Then init constant nodes /////////////");
		Constructor.AddLine("////////////////////////////////////////////////////");
		Constructor.NewLine();
		
		// Then constant nodes
		for (auto& Node : ConstantComputeNodes)
		{
			Constructor.QueueComment("// Init of " + Node->PrettyName);
			Node->CallInitCpp(Constructor);
			Constructor.EndComment();
		}
	}
	Constructor.EndBlock();
	Constructor.NewLine();
	Constructor.AddLine("////////////////////////////////////////////////////");
	Constructor.AddLine("//////////////////// Other inits ///////////////////");
	Constructor.AddLine("////////////////////////////////////////////////////");
	
	// And then other nodes
	for (auto& Functions : AllFunctions)
	{
		for (auto* Function : Functions.Iterate())
		{
			if (Function->IsUsedForInit())
			{
				Function->Call(Constructor, {}, EVoxelFunctionType::Init);
			}
		}
	}
}

void FVoxelGraph::ComputeConstants(FVoxelCppConstructor& Constructor) const
{
	FVoxelCppVariableScope Scope(Constructor);
	for (auto& Node : ConstantComputeNodes)
	{
		Constructor.QueueComment("// " + Node->PrettyName);
		if (Constructor.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
		{
			Node->CallComputeRangeCpp(Constructor, FVoxelVariableAccessInfo::Constant());
		}
		else
		{
			Node->CallComputeCpp(Constructor, FVoxelVariableAccessInfo::Constant());
		}
		Constructor.EndComment();
	}
}

void FVoxelGraph::Compute(FVoxelCppConstructor& Constructor, EVoxelFunctionAxisDependencies Dependencies) const
{
	FirstFunctions.Get(Dependencies).Call(Constructor, {}, EVoxelFunctionType::Compute);
}

void FVoxelGraph::DeclareInitFunctions(FVoxelCppConstructor& Constructor) const
{	
	for (auto& Functions : AllFunctions)
	{
		for (auto& Function : Functions.Iterate())
		{
			if (Function->IsUsedForInit())
			{
				Function->DeclareInitFunction(Constructor);
				Constructor.NewLine();
			}
		}
	}
}

void FVoxelGraph::DeclareComputeFunctions(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const
{
	for (auto& Functions : AllFunctions)
	{
		for (auto& Function : Functions.Iterate())
		{
			if (Function->IsUsedForCompute(Constructor))
			{
				Function->DeclareComputeFunction(Constructor, GraphOutputs);
				Constructor.NewLine();
			}
		}
	}
}
