// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelBiomeMapNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelTextureSamplerNode.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphGenerator.h"
#include "NodeFunctions/VoxelNodeFunctions.h"

#include "Engine/Texture2D.h"

UVoxelNode_BiomeMapSampler::UVoxelNode_BiomeMapSampler()
{
	SetInputs(
		{ "U", EC::Float, "Coordinate between 0 and texture width" },
		{ "V", EC::Float, "Coordinate between 0 and texture height" });
}

int32 UVoxelNode_BiomeMapSampler::GetOutputPinsCount() const
{
	return Biomes.Num();
}

FName UVoxelNode_BiomeMapSampler::GetOutputPinName(int32 PinIndex) const
{
	return Biomes.IsValidIndex(PinIndex) ? *Biomes[PinIndex].Name : TEXT("Error");
}

EVoxelPinCategory UVoxelNode_BiomeMapSampler::GetOutputPinCategory(int32 PinIndex) const
{
	return EC::Float;
}

FText UVoxelNode_BiomeMapSampler::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Biome Map: {0}"), Super::GetTitle());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_BiomeMapSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_BiomeMapSampler& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Threshold(Node.Threshold)
			, Colors(Node.GetColors())
			, Texture(FVoxelTextureUtilities::CreateFromTexture_Color(Node.Texture))
			, ColorsVariable("TArray<FColor>", UniqueName.ToString() + "_Colors")
			, TextureVariable(MakeShared<FVoxelColorTextureVariable>(Node, Node.Texture))
		{
		}

		void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(ColorsVariable);
		}
		
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(ColorsVariable.CppName + " = ");
			Constructor.Indent();
			Constructor.AddLine("{");
			Constructor.Indent();
			for (int32 Index = 0; Index < Colors.Num(); Index++)
			{
				auto& C = Colors[Index];
				FString ColorString = FString::Printf(TEXT("FColor(%d, %d, %d, %d)"), C.R, C.G, C.B, C.A);
				if (Index == Colors.Num() - 1)
				{
					Constructor.AddLine(ColorString);
				}
				else
				{
					Constructor.AddLine(ColorString + ",");
				}
			}
			Constructor.Unindent();
			Constructor.AddLine("};");
			Constructor.Unindent();
		}

		void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			const v_flt X = Inputs[0].Get<v_flt>();
			const v_flt Y = Inputs[1].Get<v_flt>();

			FVoxelNodeFunctions::FindColorsLerpedAlphas(
				Threshold, 
				Colors, 
				Texture, 
				X, 
				Y, 
				[&](int32 Index, v_flt Alpha) { Outputs[Index].Get<v_flt>() = Alpha; });
		}
		void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			for (int32 Index = 0; Index < Colors.Num(); Index++)
			{
				Outputs[Index].Get<v_flt>() = { 0, 1 };
			}
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			FString SwitchString;
			SwitchString += "switch (Index) { ";
			for (int32 Index = 0; Index < Outputs.Num(); Index++)
			{
				SwitchString += FString::Printf(TEXT("case %d: %s = Alpha; break; "), Index, *Outputs[Index]);
			}
			SwitchString += "}";

			Constructor.AddLinef(
				TEXT("FVoxelNodeFunctions::FindColorsLerpedAlphas(%d, %s, %s, %s, %s, [&](int32 Index, v_flt Alpha) { %s; });"),
				Threshold,
				*ColorsVariable.CppName,
				*TextureVariable->CppName,
				*Inputs[0],
				*Inputs[1],
				*SwitchString);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			for (auto& Output : Outputs)
			{
				Constructor.AddLinef(TEXT("%s = { 0, 1 };"), *Output);
			}
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(TextureVariable);
		}

	private:
		const int Threshold;
		const TArray<FColor> Colors;
		const TVoxelTexture<FColor> Texture;
		FVoxelVariable const ColorsVariable;
		const TSharedRef<FVoxelColorTextureVariable> TextureVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

TArray<FColor> UVoxelNode_BiomeMapSampler::GetColors() const
{
	TArray<FColor> Colors;
	for (auto& Biome : Biomes)
	{
		Colors.Add(Biome.Color);
	}
	return Colors;
}

void UVoxelNode_BiomeMapSampler::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	
	FString Error;
	if (!FVoxelTextureUtilities::CanCreateFromTexture(Texture, Error))
	{
		ErrorReporter.AddMessageToNode(this, Error, EVoxelGraphNodeMessageType::Error);
	}
}

#if WITH_EDITOR
void UVoxelNode_BiomeMapSampler::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (Biomes.Num() >= 256)
		{
			Biomes.SetNum(256);
		}
		for (int32 Index = 0; Index < Biomes.Num(); Index++)
		{
			auto& Biome = Biomes[Index];
			if (Biome.Name.IsEmpty())
			{
				Biome.Name = FString::Printf(TEXT("Biome %d"), Index);
			}
		}
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif