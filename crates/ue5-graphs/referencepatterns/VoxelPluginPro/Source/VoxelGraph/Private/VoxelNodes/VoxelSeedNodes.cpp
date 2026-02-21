// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelSeedNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeColors.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGenerators/VoxelGeneratorInit.h"
#include "VoxelGraphGlobals.h"

UVoxelSeedNode::UVoxelSeedNode()
{
	SetColor(FVoxelNodeColors::SeedNode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_Seed::UVoxelNode_Seed()
{
	SetOutputs(EC::Seed);
	SetColor(FVoxelNodeColors::SeedNode);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Seed::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		FLocalVoxelComputeNode(const UVoxelNode_Seed& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelSeedComputeNode(Node, CompilationNode)
			, Value(Node.GetParameter<int32>())
			, Variable(MakeShared<FVoxelExposedVariable>(Node, FVoxelCppUtils::TypeToString<int32>(), FVoxelCppUtils::TypeToString<int32>(), FVoxelCppUtils::LexToCpp(Node.DefaultValue)))
		{
		}
		
		void Init(FVoxelGraphSeed Inputs[], FVoxelGraphSeed Outputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			Outputs[0] = Value;
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + Variable->CppName + ";");
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
		}

	private:
		const int32 Value;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_AddSeeds::UVoxelNode_AddSeeds()
{
	SetInputs(EC::Seed);
	SetOutputs(EC::Seed);
	SetInputsCount(1, MAX_VOXELNODE_PINS);
}

void UVoxelNode_Seed::PostLoad()
{
	Super::PostLoad();

	if (!Name_DEPRECATED.IsNone())
	{
		DisplayName = Name_DEPRECATED.ToString();
		UniqueName = Name_DEPRECATED;
	}
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_AddSeeds::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		using FVoxelSeedComputeNode::FVoxelSeedComputeNode;

		void Init(FVoxelGraphSeed Inputs[], FVoxelGraphSeed Outputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			uint32 X = FVoxelUtilities::MurmurHash32(Inputs[0]);
			for (int32 I = 1; I < InputCount; I++)
			{
				X = FVoxelUtilities::MurmurHash32(X ^ FVoxelUtilities::MurmurHash32(Inputs[I]));
			}
			Outputs[0] = X;
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			FString Line;
			Line += Outputs[0] + " = ";

			for (int32 I = 0; I < InputCount - 1; I++)
			{
				Line += "FVoxelUtilities::MurmurHash32(FVoxelUtilities::MurmurHash32(" + Inputs[I] + ") ^ ";
			}
			Line += "FVoxelUtilities::MurmurHash32(" + Inputs[InputCount - 1];

			for (int32 I = 0; I < InputCount; I++)
			{
				Line += ")";
			}
			Line += ";";

			Constructor.AddLine(Line);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_MakeSeeds::UVoxelNode_MakeSeeds()
{
	SetInputs(EC::Seed);
	SetOutputs(EC::Seed);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_MakeSeeds::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		using FVoxelSeedComputeNode::FVoxelSeedComputeNode;

		void Init(FVoxelGraphSeed Inputs[], FVoxelGraphSeed Outputs[], const FVoxelGeneratorInit& InitStruct) override
		{
			for (int32 Index = 0; Index < OutputCount; Index++)
			{
				Outputs[Index] = FVoxelUtilities::MurmurHash32(Index == 0 ? Inputs[0] : Outputs[Index - 1]);
			}
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			for (int32 Index = 0; Index < OutputCount; Index++)
			{
				Constructor.AddLinef(TEXT("%s = FVoxelUtilities::MurmurHash32(%s);"), *Outputs[Index], Index == 0 ? *Inputs[0] : *Outputs[Index - 1]);
			}
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
