// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelDataAssetSamplerNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelAssets/VoxelDataAssetData.inl"

UVoxelNode_DataAssetSampler::UVoxelNode_DataAssetSampler()
{
	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" });
	SetOutputs(
		{ "Value", EC::Float, "Data asset value at position X Y Z. Between -1 and 1." },
		{ "Material", EC::Material, "Data asset material at position X Y Z" },
		{ "Size X", EC::Int, "Size of the data asset" },
		{ "Size Y", EC::Int, "Size of the data asset" },
		{ "Size Z", EC::Int, "Size of the data asset" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_DataAssetSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_DataAssetSampler& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, bBilinearInterpolation(Node.bBilinearInterpolation)
			, Data(Node.Asset->GetData())
			, Variable(MakeShared<FVoxelDataAssetVariable>(Node, Node.Asset))
		{
		}

		void Compute(FVoxelNodeInputBuffer I, FVoxelNodeOutputBuffer O, const FVoxelContext& Context) const override
		{
			if (bBilinearInterpolation)
			{
				if (IsOutputUsed(0)) O[0].Get<v_flt>() = Data->GetInterpolatedValue(I[0].Get<v_flt>(), I[1].Get<v_flt>(), I[2].Get<v_flt>(), FVoxelValue::Empty());
				if (IsOutputUsed(1)) O[1].Get<FVoxelMaterial>() = Data->GetInterpolatedMaterial(I[0].Get<v_flt>(), I[1].Get<v_flt>(), I[2].Get<v_flt>());
			}
			else
			{
				if (IsOutputUsed(0)) O[0].Get<v_flt>() = Data->GetValue(I[0].Get<int32>(), I[1].Get<int32>(), I[2].Get<int32>(), FVoxelValue::Empty()).ToFloat();
				if (IsOutputUsed(1)) O[1].Get<FVoxelMaterial>() = Data->GetMaterial(I[0].Get<int32>(), I[1].Get<int32>(), I[2].Get<int32>());
			}
			if (IsOutputUsed(2)) O[2].Get<int32>() = Data->GetSize().X;
			if (IsOutputUsed(3)) O[3].Get<int32>() = Data->GetSize().Y;
			if (IsOutputUsed(4)) O[4].Get<int32>() = Data->GetSize().Z;
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer O, const FVoxelContextRange& Context) const override
		{
			O[0].Get<v_flt>() = { -1.f,1.f };
			O[2].Get<int32>() = Data->GetSize().X;
			O[3].Get<int32>() = Data->GetSize().Y;
			O[4].Get<int32>() = Data->GetSize().Z;
		}
		void ComputeCpp(const TArray<FString>& I, const TArray<FString>& O, FVoxelCppConstructor& Constructor) const override
		{
			if (bBilinearInterpolation)
			{
				if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = %s->GetInterpolatedValue(%s, %s, %s, FVoxelValue::Empty());"), *O[0], *Variable->CppName, *I[0], *I[1], *I[2]);
				if (IsOutputUsed(1)) Constructor.AddLinef(TEXT("%s = %s->GetInterpolatedMaterial(%s, %s, %s);"), *O[1], *Variable->CppName, *I[0], *I[1], *I[2]);
			}
			else
			{
				if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = %s->GetValue(%s, %s, %s, FVoxelValue::Empty());"), *O[0], *Variable->CppName, *I[0], *I[1], *I[2]);
				if (IsOutputUsed(1)) Constructor.AddLinef(TEXT("%s = %s->GetMaterial(%s, %s, %s);"), *O[1], *Variable->CppName, *I[0], *I[1], *I[2]);
			}
			if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s->GetSize().X;"), *O[2], *Variable->CppName);
			if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s->GetSize().Y;"), *O[3], *Variable->CppName);
			if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s->GetSize().Z;"), *O[4], *Variable->CppName);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = { -1.f, 1.f };"), *Outputs[0]);
			if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s->GetSize().X;"), *Outputs[2], *Variable->CppName);
			if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s->GetSize().Y;"), *Outputs[3], *Variable->CppName);
			if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s->GetSize().Z;"), *Outputs[4], *Variable->CppName);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
		}

	private:
		const bool bBilinearInterpolation;
		const TVoxelSharedRef<const FVoxelDataAssetData> Data;
		const TSharedRef<FVoxelDataAssetVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

EVoxelPinCategory UVoxelNode_DataAssetSampler::GetInputPinCategory(int32 PinIndex) const
{
	return bBilinearInterpolation ? EC::Float : EC::Int;
}

FText UVoxelNode_DataAssetSampler::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Data Asset: {0}"), Super::GetTitle());
}