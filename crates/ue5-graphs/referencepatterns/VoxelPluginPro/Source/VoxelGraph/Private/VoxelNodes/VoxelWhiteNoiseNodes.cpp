// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelWhiteNoiseNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGraphGenerator.h"
#include "FastNoise/VoxelFastNoise.inl"

template<uint32 Dimension>
class TVoxelWhiteNoiseComputeNode : public FVoxelDataComputeNode
{
public:
	using FLocalVoxelComputeNode = TVoxelWhiteNoiseComputeNode<Dimension>;
	GENERATED_DATA_COMPUTE_NODE_BODY();

	TVoxelWhiteNoiseComputeNode(const UVoxelNode& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, NoiseVariable("FVoxelFastNoise", UniqueName.ToString() + "_Noise")
	{
	}

	virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
	{
		PrivateNoise.SetSeed(Inputs[Dimension]);
	}
	virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.AddLine(NoiseVariable.CppName + ".SetSeed(" + Inputs[Dimension] + ");");
	}

	virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		Outputs[0].Get<v_flt>() =
			Dimension == 2
			? PrivateNoise.GetWhiteNoise_2D(Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>())
			: PrivateNoise.GetWhiteNoise_3D(Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>(), Inputs[2].Get<v_flt>());
	}
	virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		Outputs[0].Get<v_flt>() = { -1, 1 };
	}
	virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		if (Dimension == 2)
		{
			Constructor.AddLinef(
				TEXT("%s = %s.GetWhiteNoise_2D(%s, %s);"),
				*Outputs[0],
				*NoiseVariable.CppName,
				*Inputs[0],
				*Inputs[1]);
		}
		else
		{
			Constructor.AddLinef(
				TEXT("%s = %s.GetWhiteNoise_3D(%s, %s, %s);"),
				*Outputs[0],
				*NoiseVariable.CppName,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2]);
		}
	}
	virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.AddLinef(TEXT("%s = { -1, 1 };"), *Outputs[0]);
	}

	virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
	{
		PrivateVariables.Add(NoiseVariable);
	}

private:
	FVoxelVariable const NoiseVariable;
	FVoxelFastNoise PrivateNoise;
};

UVoxelNode_2DWhiteNoise::UVoxelNode_2DWhiteNoise()
{
	SetInputs(
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"},
		{"Seed", EC::Seed, "Seed"});
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DWhiteNoise::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new TVoxelWhiteNoiseComputeNode<2>(*this, InCompilationNode));
}

UVoxelNode_3DWhiteNoise::UVoxelNode_3DWhiteNoise()
{
	SetInputs(
		{"X", EC::Float, "X"},
		{"Y", EC::Float, "Y"},
		{"Z", EC::Float, "Z"},
		{"Seed", EC::Seed, "Seed"});
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_3DWhiteNoise::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new TVoxelWhiteNoiseComputeNode<3>(*this, InCompilationNode));
}
