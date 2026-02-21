// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGradientPerturbNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "VoxelContext.h"
#include "FastNoise/VoxelFastNoise.inl"

UVoxelNode_2DGradientPerturb::UVoxelNode_2DGradientPerturb()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Frequency", EC::Float, "The frequency of the noise" },
		{ "Amplitude", EC::Float, "The amplitude of the perturbation, in the same unit as the input" },
		{ "Seed", EC::Seed, "Seed" });
	SetOutputs(
		{ "X", EC::Float, "X with perturbation" },
		{ "Y", EC::Float, "Y with perturbation" });
}

UVoxelNode_2DGradientPerturbFractal::UVoxelNode_2DGradientPerturbFractal()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Frequency", EC::Float, "The frequency of the noise" },
		{ "Amplitude", EC::Float, "The amplitude of the perturbation, in the same unit as the input" },
		{ "Seed", EC::Seed, "Seed" });
	SetOutputs(
		{ "X", EC::Float, "X with perturbation" },
		{ "Y", EC::Float, "Y with perturbation" });
}

UVoxelNode_3DGradientPerturb::UVoxelNode_3DGradientPerturb()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" },
		{ "Frequency", EC::Float, "The frequency of the noise" },
		{ "Amplitude", EC::Float, "The amplitude of the perturbation, in the same unit as the input" },
		{ "Seed", EC::Seed, "Seed" });
	SetOutputs(
		{ "X", EC::Float, "X with perturbation" },
		{ "Y", EC::Float, "Y with perturbation" },
		{ "Z", EC::Float, "Z with perturbation" });
}

UVoxelNode_3DGradientPerturbFractal::UVoxelNode_3DGradientPerturbFractal()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" },
		{ "Frequency", EC::Float, "The frequency of the noise" },
		{ "Amplitude", EC::Float, "The amplitude of the perturbation, in the same unit as the input" },
		{ "Seed", EC::Seed, "Seed" });
	SetOutputs(
		{ "X", EC::Float, "X with perturbation" },
		{ "Y", EC::Float, "Y with perturbation" },
		{ "Z", EC::Float, "Z with perturbation" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DGradientPerturb::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelNoiseComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelNoiseComputeNode::FVoxelNoiseComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
			Outputs[1].Get<v_flt>() = Inputs[1].Get<v_flt>();
			GetNoise().GradientPerturb_2D(Outputs[0].Get<v_flt>(), Outputs[1].Get<v_flt>(), Inputs[2].Get<v_flt>(), Inputs[3].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[0].Get<v_flt>().Min - 2 * Inputs[3].Get<v_flt>().Max, Inputs[0].Get<v_flt>().Max + 2 * Inputs[3].Get<v_flt>().Max);
			Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[1].Get<v_flt>().Min - 2 * Inputs[3].Get<v_flt>().Max, Inputs[1].Get<v_flt>().Max + 2 * Inputs[3].Get<v_flt>().Max);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Inputs[0] + ";");
			Constructor.AddLine(Outputs[1] + " = " + Inputs[1] + ";");
			Constructor.AddLine(GetNoiseName() + ".GradientPerturb_2D(" + Outputs[0] + ", " + Outputs[1] + ", " + Inputs[2] + ", " + Inputs[3] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[0], *Inputs[0], *Inputs[3], *Inputs[0], *Inputs[3]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[1], *Inputs[1], *Inputs[3], *Inputs[1], *Inputs[3]);
		}
		
		virtual int32 GetSeedInputIndex() const override { return 4; }
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_2DGradientPerturbFractal::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelNoiseFractalComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelNoiseFractalComputeNode::FVoxelNoiseFractalComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
			Outputs[1].Get<v_flt>() = Inputs[1].Get<v_flt>();
			GetNoise().GradientPerturbFractal_2D(Outputs[0].Get<v_flt>(), Outputs[1].Get<v_flt>(), Inputs[2].Get<v_flt>(), GET_OCTAVES, Inputs[3].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[0].Get<v_flt>().Min - 2 * Inputs[3].Get<v_flt>().Max, Inputs[0].Get<v_flt>().Max + 2 * Inputs[3].Get<v_flt>().Max);
			Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[1].Get<v_flt>().Min - 2 * Inputs[3].Get<v_flt>().Max, Inputs[1].Get<v_flt>().Max + 2 * Inputs[3].Get<v_flt>().Max);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Inputs[0] + ";");
			Constructor.AddLine(Outputs[1] + " = " + Inputs[1] + ";");
			Constructor.AddLine(GetNoiseName() + ".GradientPerturbFractal_2D(" + Outputs[0] + ", " + Outputs[1] + ", " + Inputs[2] + ", " + GET_OCTAVES_CPP + ", " + Inputs[3] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[0], *Inputs[0], *Inputs[3], *Inputs[0], *Inputs[3]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[1], *Inputs[1], *Inputs[3], *Inputs[1], *Inputs[3]);
		}
		
		virtual int32 GetSeedInputIndex() const override { return 4; }
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_3DGradientPerturb::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelNoiseComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		using FVoxelNoiseComputeNode::FVoxelNoiseComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
			Outputs[1].Get<v_flt>() = Inputs[1].Get<v_flt>();
			Outputs[2].Get<v_flt>() = Inputs[2].Get<v_flt>();
			GetNoise().GradientPerturb_3D(Outputs[0].Get<v_flt>(), Outputs[1].Get<v_flt>(), Outputs[2].Get<v_flt>(), Inputs[3].Get<v_flt>(), Inputs[4].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[0].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[0].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
			Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[1].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[1].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
			Outputs[2].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[2].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[2].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Inputs[0] + ";");
			Constructor.AddLine(Outputs[1] + " = " + Inputs[1] + ";");
			Constructor.AddLine(Outputs[2] + " = " + Inputs[2] + ";");
			Constructor.AddLine(GetNoiseName() + ".GradientPerturb_3D(" + Outputs[0] + ", " + Outputs[1] + ", " + Outputs[2] + ", " + Inputs[3] + ", " + Inputs[4] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[0], *Inputs[0], *Inputs[4], *Inputs[0], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[1], *Inputs[1], *Inputs[4], *Inputs[1], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[2], *Inputs[2], *Inputs[4], *Inputs[2], *Inputs[4]);
		}
		
		virtual int32 GetSeedInputIndex() const override { return 5; }
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_3DGradientPerturbFractal::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelNoiseFractalComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		using FVoxelNoiseFractalComputeNode::FVoxelNoiseFractalComputeNode;

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
			Outputs[1].Get<v_flt>() = Inputs[1].Get<v_flt>();
			Outputs[2].Get<v_flt>() = Inputs[2].Get<v_flt>();
			GetNoise().GradientPerturbFractal_3D(Outputs[0].Get<v_flt>(), Outputs[1].Get<v_flt>(), Outputs[2].Get<v_flt>(), Inputs[3].Get<v_flt>(), GET_OCTAVES, Inputs[4].Get<v_flt>());
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[0].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[0].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
			Outputs[1].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[1].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[1].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
			Outputs[2].Get<v_flt>() = TVoxelRange<v_flt>::FromList(Inputs[2].Get<v_flt>().Min - 2 * Inputs[4].Get<v_flt>().Max, Inputs[2].Get<v_flt>().Max + 2 * Inputs[4].Get<v_flt>().Max);
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Inputs[0] + ";");
			Constructor.AddLine(Outputs[1] + " = " + Inputs[1] + ";");
			Constructor.AddLine(Outputs[2] + " = " + Inputs[2] + ";");
			Constructor.AddLine(GetNoiseName() + ".GradientPerturbFractal_3D(" + Outputs[0] + ", " + Outputs[1] + ", " + Outputs[2] + ", " + Inputs[3] + ", " + GET_OCTAVES_CPP + ", " + Inputs[4] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[0], *Inputs[0], *Inputs[4], *Inputs[0], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[1], *Inputs[1], *Inputs[4], *Inputs[1], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s = TVoxelRange<v_flt>::FromList(%s.Min - 2 * %s.Max, %s.Max + 2 * %s.Max);"), *Outputs[2], *Inputs[2], *Inputs[4], *Inputs[2], *Inputs[4]);
		}
		
		virtual int32 GetSeedInputIndex() const override { return 5; }
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
