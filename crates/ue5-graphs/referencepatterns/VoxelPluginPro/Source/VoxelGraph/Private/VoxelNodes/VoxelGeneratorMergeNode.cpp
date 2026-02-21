// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGeneratorMergeNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGenerators/VoxelGeneratorInit.h"
#include "VoxelGenerators/VoxelGeneratorInstance.h"
#include "VoxelGraphOutputsConfig.h"
#include "NodeFunctions/VoxelNodeFunctions.h"
#include "Runtime/VoxelComputeNode.h"

constexpr int32 NumDefaultInputPins_WGMN = 3 + 2 * 4;

inline TArray<FName> GetFloatOutputs(UVoxelGraphOutputsConfig* Config)
{
	TArray<FName> Result;
	if (Config)
	{
		for (auto& Output : Config->Outputs)
		{
			if (Output.Category == EVoxelDataPinCategory::Float)
			{
				Result.Add(Output.Name);
			}
		}
	}
	return Result;
}

inline TArray<bool> GetComputeFloatOutputs(const TArray<FName>& FloatOutputs, const FVoxelComputeNode& Node)
{
	check(Node.OutputCount == 2 + FloatOutputs.Num() + 1);
	
	TArray<bool> Result;
	for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
	{
		Result.Add(Node.IsOutputUsed(2 + Index));
	}
	return Result;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GeneratorMerge::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

		FLocalVoxelComputeNode(const UVoxelNode_GeneratorMerge& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Tolerance(Node.Tolerance)
			, MaterialConfig(Node.MaterialConfig)
			, Variable(MakeShared<FVoxelGeneratorArrayVariable>(Node, Node.Generators))
			, Instances(FVoxelNodeFunctions::CreateGeneratorArray(Node.Generators))
			, FloatOutputs(GetFloatOutputs(Node.Outputs))
			, bComputeValue(IsOutputUsed(0))
			, bComputeMaterial(IsOutputUsed(1))
			, ComputeFloatOutputs(GetComputeFloatOutputs(FloatOutputs, *this))
		{
		}

		virtual void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			for (auto& Instance : Instances)
			{
				Instance->Init(InitStruct);
			}
		}
		virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("for (auto& Instance : %s)"), *Variable->CppName);
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("Instance->Init(%s);"), *FVoxelCppIds::InitStruct);
			Constructor.EndBlock();
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer InOutputs, const FVoxelContext& Context) const override
		{
			TArray<v_flt, TInlineAllocator<128>> OutFloatOutputs;
			FVoxelNodeFunctions::ComputeGeneratorsMerge(
				MaterialConfig,
				Tolerance,
				Instances,
				FloatOutputs,
				Context,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Inputs[3].Get<int32>(), Inputs[4].Get<v_flt>(),
				Inputs[5].Get<int32>(), Inputs[6].Get<v_flt>(),
				Inputs[7].Get<int32>(), Inputs[8].Get<v_flt>(),
				Inputs[9].Get<int32>(), Inputs[10].Get<v_flt>(),
				bComputeValue, bComputeMaterial, ComputeFloatOutputs,
				InOutputs[0].Get<v_flt>(),
				InOutputs[1].Get<FVoxelMaterial>(),
				OutFloatOutputs,
				InOutputs[OutputCount - 1].Get<int32>());

			for (int32 Index = 0; Index < OutFloatOutputs.Num(); Index++)
			{
				InOutputs[2 + Index].Get<v_flt>() = OutFloatOutputs[Index];
			}
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer InOutputs, const FVoxelContextRange& Context) const override
		{
			InOutputs[0].Get<v_flt>() = 0;
			
			TArray<TVoxelRange<v_flt>, TInlineAllocator<128>> OutFloatOutputs;
			FVoxelNodeFunctions::ComputeGeneratorsMergeRange(
				Instances,
				FloatOutputs,
				Context,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				bComputeValue, ComputeFloatOutputs,
				InOutputs[0].Get<v_flt>(),
				OutFloatOutputs,
				InOutputs[OutputCount - 1].Get<int32>());

			for (int32 Index = 0; Index < OutFloatOutputs.Num(); Index++)
			{
				InOutputs[2 + Index].Get<v_flt>() = OutFloatOutputs[Index];
			}
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& InOutputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();

			Constructor.NewLine();
			Constructor.AddLinef(TEXT("static TArray<FName> StaticFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(FloatOutputs));
			Constructor.AddLinef(TEXT("static TArray<bool> StaticComputeFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(ComputeFloatOutputs));
			Constructor.NewLine();

			const FString MaterialConfigString =
				MaterialConfig == EVoxelMaterialConfig::RGB
				? "EVoxelMaterialConfig::RGB"
				: MaterialConfig == EVoxelMaterialConfig::SingleIndex
				? "EVoxelMaterialConfig::SingleIndex"
				: "EVoxelMaterialConfig::DoubleIndex"; // TODO PLACEABLE ITEMS

			Constructor.AddLine("TArray<v_flt, TInlineAllocator<128>> OutFloatOutputs;");
			Constructor.AddLine("FVoxelNodeFunctions::ComputeGeneratorsMerge(");
			Constructor.Indent();
			Constructor.AddLinef(TEXT("%s,"), *MaterialConfigString);
			Constructor.AddLinef(TEXT("%f,"), Tolerance);
			Constructor.AddLinef(TEXT("%s,"), *Variable->CppName);
			Constructor.AddLinef(TEXT("StaticFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *FVoxelCppIds::Context);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[1]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[2]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[3], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[5], *Inputs[6]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[7], *Inputs[8]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[9], *Inputs[10]);
			Constructor.AddLinef(TEXT("%s, %s,"), *LexToString(bComputeValue), *LexToString(bComputeMaterial));
			Constructor.AddLinef(TEXT("StaticComputeFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[1]);
			Constructor.AddLinef(TEXT("OutFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s);"), *InOutputs[OutputCount - 1]);
			Constructor.Unindent();

			for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
			{
				if (ComputeFloatOutputs[Index]) 
				{
					Constructor.AddLinef(TEXT("%s = OutFloatOutputs[%d];"), *InOutputs[2 + Index], Index);
				}
			}
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& InOutputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();

			Constructor.NewLine();
			Constructor.AddLinef(TEXT("static TArray<FName> StaticFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(FloatOutputs));
			Constructor.AddLinef(TEXT("static TArray<bool> StaticComputeFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(ComputeFloatOutputs));
			Constructor.NewLine();
			
			Constructor.AddLine("TArray<TVoxelRange<v_flt>, TInlineAllocator<128>> OutFloatOutputs;");
			Constructor.AddLine("FVoxelNodeFunctions::ComputeGeneratorsMergeRange(");
			Constructor.Indent();
			Constructor.AddLinef(TEXT("%s,"), *Variable->CppName);
			Constructor.AddLinef(TEXT("StaticFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *FVoxelCppIds::Context);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[1]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[2]);
			Constructor.AddLinef(TEXT("%s,"), *LexToString(bComputeValue));
			Constructor.AddLinef(TEXT("StaticComputeFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[0]);
			Constructor.AddLinef(TEXT("OutFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s);"), *InOutputs[OutputCount - 1]);
			Constructor.Unindent();

			for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
			{
				if (ComputeFloatOutputs[Index]) 
				{
					Constructor.AddLinef(TEXT("%s = OutFloatOutputs[%d];"), *InOutputs[2 + Index], Index);
				}
			}
			Constructor.EndBlock();
		}

		virtual void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
		}

	private:
		const float Tolerance;
		const EVoxelMaterialConfig MaterialConfig;
		const TSharedRef<FVoxelGeneratorArrayVariable> Variable;
		const TArray<TVoxelSharedPtr<FVoxelGeneratorInstance>> Instances;
		const TArray<FName> FloatOutputs;

		const bool bComputeValue;
		const bool bComputeMaterial;
		const TArray<bool> ComputeFloatOutputs;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

UVoxelNode_GeneratorMerge::UVoxelNode_GeneratorMerge()
{
	SetInputs({
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" },
		{ "Index 0", EC::Int, "First generator index" },
		{ "Alpha 0", EC::Float, "First generator alpha" },
		{ "Index 1", EC::Int, "Second generator index" },
		{ "Alpha 1", EC::Float, "Second generator alpha" },
		{ "Index 2", EC::Int, "Third generator index" },
		{ "Alpha 2", EC::Float, "Third generator alpha" },
		{ "Index 3", EC::Int, "Fourth generator index" },
		{ "Alpha 3", EC::Float, "Fourth generator alpha" } });
	check(UVoxelNodeHelper::GetMinInputPins() == NumDefaultInputPins_WGMN);
}

FText UVoxelNode_GeneratorMerge::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Generator Merge: {0}"), Super::GetTitle());
}

int32 UVoxelNode_GeneratorMerge::GetOutputPinsCount() const
{
	return 2 + GetFloatOutputs(Outputs).Num() + 1;
}

FName UVoxelNode_GeneratorMerge::GetOutputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "Value";
	}
	if (PinIndex == 1)
	{
		return "Material";
	}
	if (PinIndex == GetOutputPinsCount() - 1)
	{
		return "Num Queried Generators";
	}
	PinIndex -= 2;
	auto FloatOutputs = GetFloatOutputs(Outputs);
	if (FloatOutputs.IsValidIndex(PinIndex))
	{
		return FloatOutputs[PinIndex];
	}
	return "Error";
}

EVoxelPinCategory UVoxelNode_GeneratorMerge::GetOutputPinCategory(int32 PinIndex) const
{
	if (PinIndex == 1)
	{
		return EC::Material;
	}
	else if (PinIndex == GetOutputPinsCount() - 1)
	{
		return EC::Int;
	}
	else
	{
		return EC::Float;
	}
}

void UVoxelNode_GeneratorMerge::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	
	for (auto& Generator : Generators)
	{
		if (!Generator.IsValid())
		{
			ErrorReporter.AddMessageToNode(this, "invalid generator", EVoxelGraphNodeMessageType::Error);
		}
	}
}