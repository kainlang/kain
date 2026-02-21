// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "VoxelGraphConstants.h"

FString FVoxelGraphFunctionInfo::GetFunctionName() const
{
	FString Name = "Function" + FString::FromInt(FunctionId) + "_" + FVoxelAxisDependencies::ToString(Dependencies);
	if (FunctionType == EVoxelFunctionType::Init)
	{
		Name += "_Init";
	}
	else
	{
		check(FunctionType == EVoxelFunctionType::Compute);
		Name += "_Compute";
	}
	return Name;
}

FVoxelGraphFunction::FVoxelGraphFunction(
	const TVoxelSharedRef<FVoxelComputeNodeTree>& Tree, 
	const TVoxelSharedRef<FVoxelComputeNode>& FunctionInit,
	int32 FunctionId,
	EVoxelFunctionAxisDependencies Dependencies)
	: FunctionId(FunctionId)
	, Dependencies(Dependencies)
	, Tree(Tree)
	, FunctionInit(FunctionInit)
{
	check(FunctionId >= 0);
	check(FunctionInit->Type == EVoxelComputeNodeType::Exec);
	check(static_cast<FVoxelExecComputeNode&>(FunctionInit.Get()).ExecType == EVoxelComputeNodeExecType::FunctionInit);
}

void FVoxelGraphFunction::Call(FVoxelCppConstructor& Constructor, const TArray<FString>& Args, EVoxelFunctionType FunctionType) const
{
	Constructor.AddFunctionCall(FVoxelGraphFunctionInfo(FunctionId, FunctionType, Dependencies), Args);
}

bool FVoxelGraphFunction::IsUsedForInit() const
{
	// For init we only call the full function 
	return Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
}

bool FVoxelGraphFunction::IsUsedForCompute(FVoxelCppConstructor& Constructor) const
{
	return !Constructor.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex)
		|| Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache;
}

void FVoxelGraphFunction::Init(const FVoxelGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const
{
	check(IsUsedForInit());

	Tree->Init(InitStruct, Buffers);
}

void FVoxelGraphFunction::GetNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	Tree->GetNodes(Nodes);
}

void FVoxelGraphFunction::DeclareInitFunction(FVoxelCppConstructor& Constructor) const
{
	check(IsUsedForInit());

	DeclareFunction(Constructor, EVoxelFunctionType::Init);
	Constructor.StartBlock();

	FVoxelCppVariableScope Scope(Constructor);
	FunctionInit->DeclareOutputs(Constructor, FVoxelVariableAccessInfo::FunctionParameter());
	Tree->InitCpp(Constructor);
	
	Constructor.EndBlock();
}

void FVoxelGraphFunction::DeclareComputeFunction(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const
{
	check(IsUsedForCompute(Constructor));

	DeclareFunction(Constructor, EVoxelFunctionType::Compute);
	Constructor.StartBlock();

	FVoxelCppVariableScope Scope(Constructor);
	FunctionInit->DeclareOutputs(Constructor, FVoxelVariableAccessInfo::FunctionParameter());
	if (Constructor.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
	{
		Tree->ComputeRangeCpp(Constructor, FVoxelVariableAccessInfo::Compute(Dependencies), GraphOutputs);
	}
	else
	{
		Tree->ComputeCpp(Constructor, FVoxelVariableAccessInfo::Compute(Dependencies), GraphOutputs);
	}

	Constructor.EndBlock();
}

void FVoxelGraphFunction::DeclareFunction(FVoxelCppConstructor& Constructor, EVoxelFunctionType Type) const
{
	TArray<FString> Args;
	for (int32 Index = 0; Index < FunctionInit->OutputCount; Index++)
	{
		const auto Category = FunctionInit->GetOutputCategory(Index);
		if (Type != EVoxelFunctionType::Init || Category == EVoxelPinCategory::Seed)
		{
			Args.Add(Constructor.GetTypeString(Category) + " " + FVoxelCppIds::GetVariableName(FunctionInit->GetOutputId(Index)));
		}
	}
	Constructor.AddFunctionDeclaration(FVoxelGraphFunctionInfo(FunctionId, Type, Dependencies), Args);
}
