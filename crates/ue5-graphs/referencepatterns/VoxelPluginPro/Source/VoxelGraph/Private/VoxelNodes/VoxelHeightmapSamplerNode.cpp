// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelHeightmapSamplerNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelAssets/VoxelHeightmapAsset.h"
#include "VoxelAssets/VoxelHeightmapAssetSamplerWrapper.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "NodeFunctions/VoxelNodeFunctions.h"

UVoxelNode_HeightmapSampler::UVoxelNode_HeightmapSampler()
{
	SetInputs(
		{ "X", EC::Float, "X between 0 and heightmap width" },
		{ "Y", EC::Float, "Y between 0 and heightmap height" });
	SetOutputs(
		{ "Height", EC::Float, "Height at position X Y" },
		{ "Material", EC::Material, "Material at position X Y" },
		{ "Min Height", EC::Float, "Min height of the entire heightmap" },
		{ "Max Height", EC::Float, "Max height of the entire heightmap" },
		{ "Size X", EC::Float, "Width of the heightmap. Affected by the asset XY Scale setting, so it may be a float" },
		{ "Size Y", EC::Float, "Height of the heightmap. Affected by the asset XY Scale setting, so it may be a float" });
}

class FVoxelNode_HeightmapSampler_LocalVoxelComputeNode : public FVoxelDataComputeNode
{
public:
	GENERATED_DATA_COMPUTE_NODE_BODY_IMPL(FVoxelNode_HeightmapSampler_LocalVoxelComputeNode);

	FVoxelNode_HeightmapSampler_LocalVoxelComputeNode(const UVoxelNode_HeightmapSampler& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, bFloat(Node.bFloatHeightmap)
		, bCenter(Node.bCenter)
		, SamplerType(Node.SamplerType)
		, DataUINT16(bFloat ? nullptr : Node.HeightmapUINT16)
		, DataFloat(bFloat ? Node.HeightmapFloat : nullptr)
		, Variable(bFloat
			? MakeShared<FVoxelHeightmapVariable>(Node, Node.HeightmapFloat)
			: MakeShared<FVoxelHeightmapVariable>(Node, Node.HeightmapUINT16))
	{
	}

	void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
	{
		const auto Lambda = [&](auto& InData)
		{
			if (IsOutputUsed(0)) Outputs[0].Get<v_flt>() = InData.GetHeight(GetX(Inputs[0].Get<v_flt>(), InData), GetY(Inputs[1].Get<v_flt>(), InData), SamplerType);
			if (IsOutputUsed(1)) Outputs[1].Get<FVoxelMaterial>() = InData.GetMaterial(GetX(Inputs[0].Get<v_flt>(), InData), GetY(Inputs[1].Get<v_flt>(), InData), SamplerType);
			if (IsOutputUsed(2)) Outputs[2].Get<v_flt>() = InData.GetMinHeight();
			if (IsOutputUsed(3)) Outputs[3].Get<v_flt>() = InData.GetMaxHeight();
			if (IsOutputUsed(4)) Outputs[4].Get<v_flt>() = InData.GetWidth();
			if (IsOutputUsed(5)) Outputs[5].Get<v_flt>() = InData.GetHeight();
		};
		if (bFloat)
		{
			Lambda(DataFloat);
		}
		else
		{
			Lambda(DataUINT16);
		}
	}
	void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
	{
		const auto Lambda = [&](auto& InData)
		{
			if (IsOutputUsed(0)) Outputs[0].Get<v_flt>() = InData.GetHeightRange(GetX(Inputs[0].Get<v_flt>(), InData), GetY(Inputs[1].Get<v_flt>(), InData), SamplerType);
			if (IsOutputUsed(2)) Outputs[2].Get<v_flt>() = InData.GetMinHeight();
			if (IsOutputUsed(3)) Outputs[3].Get<v_flt>() = InData.GetMaxHeight();
			if (IsOutputUsed(4)) Outputs[4].Get<v_flt>() = InData.GetWidth();
			if (IsOutputUsed(5)) Outputs[5].Get<v_flt>() = InData.GetHeight();
		};
		if (bFloat)
		{
			Lambda(DataFloat);
		}
		else
		{
			Lambda(DataUINT16);
		}
	}
	void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		const FString SampleTypeString = SamplerType == EVoxelSamplerMode::Clamp ? "EVoxelSamplerMode::Clamp" : "EVoxelSamplerMode::Tile";
		if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = %s.GetHeight(%s, %s, %s);"), *Outputs[0], *Variable->CppName, *GetX(Inputs[0]), *GetY(Inputs[1]), *SampleTypeString);
		if (IsOutputUsed(1)) Constructor.AddLinef(TEXT("%s = %s.GetMaterial(%s, %s, %s);"), *Outputs[1], *Variable->CppName, *GetX(Inputs[0]), *GetY(Inputs[1]), *SampleTypeString);
		if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s.GetMinHeight();"), *Outputs[2], *Variable->CppName);
		if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s.GetMaxHeight();"), *Outputs[3], *Variable->CppName);
		if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s.GetWidth();"), *Outputs[4], *Variable->CppName);
		if (IsOutputUsed(5)) Constructor.AddLinef(TEXT("%s = %s.GetHeight();"), *Outputs[5], *Variable->CppName);
	}
	void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
	{
		const FString SampleTypeString = SamplerType == EVoxelSamplerMode::Clamp ? "EVoxelSamplerMode::Clamp" : "EVoxelSamplerMode::Tile";
		if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = %s.GetHeightRange(%s, %s, %s);"), *Outputs[0], *Variable->CppName, *GetX(Inputs[0]), *GetY(Inputs[1]), *SampleTypeString);
		if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s.GetMinHeight();"), *Outputs[2], *Variable->CppName);
		if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s.GetMaxHeight();"), *Outputs[3], *Variable->CppName);
		if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s.GetWidth();"), *Outputs[4], *Variable->CppName);
		if (IsOutputUsed(5)) Constructor.AddLinef(TEXT("%s = %s.GetHeight();"), *Outputs[5], *Variable->CppName);
	}
	void SetupCpp(FVoxelCppConfig& Config) const override
	{
		Config.AddExposedVariable(Variable);
	}

private:
	const bool bFloat;
	const bool bCenter;
	const EVoxelSamplerMode SamplerType;
	const TVoxelHeightmapAssetSamplerWrapper<uint16> DataUINT16;
	const TVoxelHeightmapAssetSamplerWrapper<float> DataFloat;
	const TSharedRef<FVoxelHeightmapVariable> Variable;

	template<typename T, typename TData>
	FORCEINLINE T GetX(T X, const TData& Data) const
	{
		return bCenter ? X - Data.GetWidth() / 2 : X;
	}
	template<typename T, typename TData>
	FORCEINLINE T GetY(T Y, const TData& Data) const
	{
		return bCenter ? Y - Data.GetHeight() / 2 : Y;
	}
	
	FString GetX(const FString& X) const
	{
		return bCenter ? FString::Printf(TEXT("%s - %s.GetWidth() / 2"), *X, *Variable->CppName) : X;
	}
	FString GetY(const FString& Y) const
	{
		return bCenter ? FString::Printf(TEXT("%s - %s.GetHeight() / 2"), *Y, *Variable->CppName) : Y;
	}
};

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_HeightmapSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	return MakeShareable(new FVoxelNode_HeightmapSampler_LocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_HeightmapSampler::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Heightmap: {0}"), Super::GetTitle());
}

void UVoxelNode_HeightmapSampler::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	if ((bFloatHeightmap && !HeightmapFloat) || (!bFloatHeightmap && !HeightmapUINT16))
	{
		ErrorReporter.AddMessageToNode(this, "invalid heightmap", EVoxelGraphNodeMessageType::Error);
	}
}