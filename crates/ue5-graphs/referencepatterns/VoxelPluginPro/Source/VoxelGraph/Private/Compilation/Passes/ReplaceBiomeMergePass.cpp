// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Compilation/Passes/ReplaceBiomeMergePass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "VoxelNodes/VoxelMathNodes.h"
#include "VoxelNodes/VoxelBinaryNodes.h"
#include "VoxelNodes/VoxelExecNodes.h"
#include "VoxelNodes/VoxelIfNode.h"

inline void AddMultiplyAdd(
	FVoxelGraphCompiler& Compiler,
	const FVoxelBiomeMergeCompilationNode& SourceNode,
	FVoxelCompilationPin& ExecPin,
	FVoxelCompilationPin& SumPin,
	FVoxelCompilationPin& ValuePin,
	FVoxelCompilationPin& AlphaPin,
	FVoxelCompilationPin*& OutExecPin,
	FVoxelCompilationPin*& OutSumPin)
{
	auto* ClampNode = Compiler.AddNode(GetDefault<UVoxelNode_Clamp>()->GetCompilationNode());
	ClampNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	ClampNode->GetInputPin(0).LinkTo(AlphaPin);
	ClampNode->GetInputPin(1).SetDefaultValue("0");
	ClampNode->GetInputPin(2).SetDefaultValue("1");

	auto* EqualNode = Compiler.AddNode(GetDefault<UVoxelNode_FLessEqual>()->GetCompilationNode());
	EqualNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	EqualNode->GetInputPin(0).LinkTo(ClampNode->GetOutputPin(0));
	EqualNode->GetInputPin(1).SetDefaultValue(FString::SanitizeFloat(SourceNode.Tolerance));

	auto* IfNode = Compiler.AddNode(GetDefault<UVoxelNode_IfWithDefaultToFalse>()->GetCompilationNode());
	IfNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	IfNode->GetInputPin(0).LinkTo(ExecPin);
	IfNode->GetInputPin(1).LinkTo(EqualNode->GetOutputPin(0));

	auto* MultiplyNode = Compiler.AddNode(GetDefault<UVoxelNode_FMultiply>()->GetCompilationNode());
	MultiplyNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	MultiplyNode->GetInputPin(0).LinkTo(ClampNode->GetOutputPin(0));
	MultiplyNode->GetInputPin(1).LinkTo(ValuePin);

	auto* AddNode = Compiler.AddNode(GetDefault<UVoxelNode_FAdd>()->GetCompilationNode());
	AddNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	AddNode->GetInputPin(0).LinkTo(SumPin);
	AddNode->GetInputPin(1).LinkTo(MultiplyNode->GetOutputPin(0));

	auto* FlowMergeNode = Compiler.AddNode(GetDefault<UVoxelNode_FlowMerge>()->GetCompilationNode());
	FlowMergeNode->SourceNodes.Insert(SourceNode.SourceNodes, 0);

	FlowMergeNode->GetInputPin(0).LinkTo(IfNode->GetOutputPin(0));
	FlowMergeNode->GetInputPin(1).LinkTo(SumPin);
	FlowMergeNode->GetInputPin(2).LinkTo(IfNode->GetOutputPin(1));
	FlowMergeNode->GetInputPin(3).LinkTo(AddNode->GetOutputPin(0));
	
	OutExecPin = &FlowMergeNode->GetOutputPin(0);
	OutSumPin = &FlowMergeNode->GetOutputPin(1);
}

void FVoxelReplaceBiomeMergePass::Apply(FVoxelGraphCompiler& Compiler)
{
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (auto* BiomeMergeNode = CastVoxel<FVoxelBiomeMergeCompilationNode>(Node))
		{
			ensure((BiomeMergeNode->GetInputCount() - 1) % 2 == 0);
			const int32 BiomesNum = (BiomeMergeNode->GetInputCount() - 1) / 2;

			auto& ExecInputPin = BiomeMergeNode->GetInputPin(0);
			ensure(ExecInputPin.PinCategory == EVoxelPinCategory::Exec);
			auto& ExecOutputPin = BiomeMergeNode->GetOutputPin(0);
			ensure(ExecOutputPin.PinCategory == EVoxelPinCategory::Exec);

			auto& ResultOutputPin = BiomeMergeNode->GetOutputPin(1);
			ensure(ResultOutputPin.PinCategory == EVoxelPinCategory::Float);

			TArray<FVoxelCompilationPin*> BiomesInputPins;
			for (int32 Index = 0; Index < 2 * BiomesNum; Index++)
			{
				auto& InputPin = BiomeMergeNode->GetInputPin(1 + Index);
				auto* Passthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, InputPin, BiomeMergeNode);
				BiomesInputPins.Add(&Passthrough->GetOutputPin(0));
			}

			auto* ExecInputPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ExecInputPin, BiomeMergeNode);
			FVoxelCompilationPin* LastExecOutput = &ExecInputPassthrough->GetOutputPin(0);
			auto* SumInputPassthrough = Compiler.AddNode(FVoxelGraphCompilerHelpers::GetPassthroughNode(EVoxelPinCategory::Float, BiomeMergeNode->SourceNodes));
			FVoxelCompilationPin* LastSumOutput = &SumInputPassthrough->GetOutputPin(0);

			TArray<FVoxelCompilationPin*> LastBiomesOutputs;
			for (int32 Index = 0; Index < BiomesNum; Index++)
			{
				// Value
				LastBiomesOutputs.Add(BiomesInputPins[2 * Index + 0]);
				// Alpha
				LastBiomesOutputs.Add(BiomesInputPins[2 * Index + 1]);
			}

			for (int32 BiomeIndex = 0; BiomeIndex < BiomesNum; BiomeIndex++)
			{
				AddMultiplyAdd(
					Compiler,
					*BiomeMergeNode,
					*LastExecOutput,
					*LastSumOutput,
					*LastBiomesOutputs[2 * BiomeIndex + 0],
					*LastBiomesOutputs[2 * BiomeIndex + 1],
					LastExecOutput,
					LastSumOutput);

				auto* FunctionSeparator = Compiler.AddNode(GetDefault<UVoxelNode_FunctionSeparator>()->GetCompilationNode(), BiomeMergeNode);
				FunctionSeparator->GetInputPin(0).LinkTo(*LastExecOutput);
				LastExecOutput = &FunctionSeparator->GetOutputPin(0);
				
				for (int32 Index = 0; Index < BiomesNum; Index++)
				{
					if (Index <= BiomeIndex)
					{
						LastBiomesOutputs[2 * Index + 0] = nullptr;
						LastBiomesOutputs[2 * Index + 1] = nullptr;
					}
				}
			}

			auto* ExecOutputPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ExecOutputPin, BiomeMergeNode);
			ExecOutputPassthrough->GetInputPin(0).LinkTo(*LastExecOutput);
			auto* ResultOutputPassthrough = FVoxelGraphCompilerHelpers::AddPassthrough(Compiler, ResultOutputPin, BiomeMergeNode);
			ResultOutputPassthrough->GetInputPin(0).LinkTo(*LastSumOutput);

			auto& OutputPassthroughs = CastCheckedVoxel<FVoxelBiomeMergeCompilationNode>(BiomeMergeNode).OutputPassthroughs;
			OutputPassthroughs.Add(ResultOutputPassthrough);

			if (Compiler.FirstNode == BiomeMergeNode)
			{
				ensure(Compiler.FirstNodePinIndex == 0);
				Compiler.FirstNode = ExecInputPassthrough;
				Compiler.FirstNodePinIndex = 0;
			}

			BiomeMergeNode->BreakAllLinks();
			Compiler.RemoveNode(BiomeMergeNode);
		}
	}
}
