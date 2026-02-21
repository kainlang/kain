// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelGeneratorSamplerNodes.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "VoxelGenerators/VoxelFlatGenerator.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGenerators/VoxelGeneratorInstance.inl"
#include "NodeFunctions/VoxelNodeFunctions.h"

TSharedPtr<FVoxelCompilationNode> UVoxelNode_GeneratorSamplerBase::GetCompilationNode() const
{
	auto Result = MakeShared<FVoxelAxisDependenciesCompilationNode>(*this);
	Result->DefaultDependencies = EVoxelAxisDependenciesFlags::X;
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SingleGeneratorSamplerBase::UVoxelNode_SingleGeneratorSamplerBase()
{
	Generator = UVoxelFlatGenerator::StaticClass();

	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" });
}

FText UVoxelNode_SingleGeneratorSamplerBase::GetTitle() const
{
	return FText::Format(
		VOXEL_LOCTEXT("Generator: {0}"),
		FText::FromString(UniqueName.ToString()));
}

void UVoxelNode_SingleGeneratorSamplerBase::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	
	if (!Generator.IsValid())
	{
		ErrorReporter.AddMessageToNode(this, "invalid generator", EVoxelGraphNodeMessageType::Error);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelGeneratorSamplerComputeNode : public FVoxelDataComputeNode
{
public:
	const int32 NumDefaultInputPins = 3;
	
	FVoxelGeneratorSamplerComputeNode(const UVoxelNode_SingleGeneratorSamplerBase& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, Generator(Node.Generator.GetInstance(false))
		, Variable(MakeShared<FVoxelGeneratorVariable>(Node, Node.Generator))
	{
	}

	void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override
	{
		Generator->Init(InitStruct);
	}
	void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.AddLine(Variable->CppName + "->Init(" + FVoxelCppIds::InitStruct + ");");
	}
	void SetupCpp(FVoxelCppConfig& Config) const override
	{
		Config.AddExposedVariable(Variable);
	}

protected:
	const TVoxelSharedRef<FVoxelGeneratorInstance> Generator;
	const TSharedRef<FVoxelGeneratorVariable> Variable;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetGeneratorValue::UVoxelNode_GetGeneratorValue()
{
	SetOutputs({"", EC::Float, "Value"});
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetGeneratorValue::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelGeneratorSamplerComputeNode::FVoxelGeneratorSamplerComputeNode;

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Generator->GetValue(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context.LOD,
				FVoxelItemStack(Context.Items.ItemHolder));
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange & Context) const override
		{
			Outputs[0].Get<v_flt>() = Generator->GetValueRange(
				FVoxelRangeUtilities::BoundsFromRanges(
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Inputs[2].Get<v_flt>()),
				Context.LOD,
				FVoxelItemStack(Context.Items.ItemHolder));
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s->GetValue(%s, %s, %s, %s.LOD, FVoxelItemStack(%s.Items.ItemHolder));"),
				*Outputs[0],
				*Variable->CppName,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s->GetValueRange(FVoxelRangeUtilities::BoundsFromRanges(%s, %s, %s), %s.LOD, FVoxelItemStack(%s.Items.ItemHolder));"),
				*Outputs[0],
				*Variable->CppName,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetGeneratorMaterial::UVoxelNode_GetGeneratorMaterial()
{
	SetOutputs({ "", EC::Material, "Material" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetGeneratorMaterial::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelGeneratorSamplerComputeNode::FVoxelGeneratorSamplerComputeNode;

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<FVoxelMaterial>() = Generator->GetMaterial(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(), 
				Inputs[2].Get<v_flt>(), 
				Context.LOD, 
				FVoxelItemStack(Context.Items.ItemHolder));
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s->GetMaterial(%s, %s, %s, %s.LOD, FVoxelItemStack(%s.Items.ItemHolder));"),
				*Outputs[0],
				*Variable->CppName,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetGeneratorCustomOutput::UVoxelNode_GetGeneratorCustomOutput()
{
	SetOutputs({ "", EC::Float, "Custom Output Value" });
}

FText UVoxelNode_GetGeneratorCustomOutput::GetTitle() const
{
	return FText::FromString("Get Generator Custom Output: " + OutputName.ToString());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetGeneratorCustomOutput::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();
		
		FLocalVoxelComputeNode(const UVoxelNode_GetGeneratorCustomOutput& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelGeneratorSamplerComputeNode(Node, CompilationNode)
			, OutputName(Node.OutputName)
		{
		}

		virtual void Compute(FVoxelNodeInputBuffer Inputs, FVoxelNodeOutputBuffer Outputs, const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetGeneratorCustomOutput(
				*Generator,
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context);
		}
		virtual void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetGeneratorCustomOutput(
				*Generator,
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetGeneratorCustomOutput(*%s, STATIC_FNAME(\"%s\"), %s, %s, %s, %s);"),
				*Outputs[0],
				*Variable->CppName,
				*OutputName.ToString(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetGeneratorCustomOutput(*%s, STATIC_FNAME(\"%s\"), %s, %s, %s, %s);"),
				*Outputs[0],
				*Variable->CppName,
				*OutputName.ToString(),
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
		}

	private:
		const FName OutputName;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
